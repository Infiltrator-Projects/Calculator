/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator_ui_controller.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace calculator::ui {

void Controller::set_expression(std::string expression) {
    state_.expression = std::move(expression);
    state_.fault = false;
    if (state_.mode == Mode::Standard) update_standard_preview();
}

void Controller::set_mode(Mode mode) {
    state_.mode = mode;
    if (mode == Mode::Scientific) {
        set_status(scientific_status_text());
    } else if (mode == Mode::Programmer) {
        set_status(programmer_status_text());
    } else {
        set_status("READY");
    }
}

std::string Controller::history_text(std::size_t limit,
                                     std::string_view newline) const {
    return session_.history_text(limit, newline);
}

std::size_t Controller::history_count() const noexcept {
    return session_.history_count();
}

std::optional<HistoryEntry> Controller::history_entry(
    std::size_t index_from_newest) const {
    return session_.history_from_newest(index_from_newest);
}

bool Controller::recall_history(std::size_t index_from_newest) {
    const auto entry = session_.history_from_newest(index_from_newest);
    if (!entry) return false;

    if (entry->kind == HistoryKind::Standard) {
        state_.mode = Mode::Standard;
    } else if (entry->kind == HistoryKind::Programmer) {
        state_.mode = Mode::Programmer;
        switch (entry->context.programmer_base) {
        case 2: state_.programmer_base = ProgrammerBase::Binary; break;
        case 8: state_.programmer_base = ProgrammerBase::Octal; break;
        case 16: state_.programmer_base = ProgrammerBase::Hexadecimal; break;
        default: state_.programmer_base = ProgrammerBase::Decimal; break;
        }
        switch (entry->context.programmer_width) {
        case 8: state_.programmer_width = IntegerWidth::Bits8; break;
        case 16: state_.programmer_width = IntegerWidth::Bits16; break;
        case 32: state_.programmer_width = IntegerWidth::Bits32; break;
        default: state_.programmer_width = IntegerWidth::Bits64; break;
        }
        state_.programmer_signed = entry->context.programmer_signed;
    } else {
        state_.mode = Mode::Scientific;
    }

    state_.expression = entry->input;
    state_.result = entry->output;
    state_.fault = !entry->ok;
    if (!entry->ok) {
        state_.status = "HISTORY · ERROR";
    } else if (state_.mode == Mode::Programmer) {
        state_.status = programmer_status_text();
    } else if (state_.mode == Mode::Scientific) {
        state_.status = scientific_status_text();
    } else {
        state_.status = "HISTORY RECALL";
    }
    return true;
}

void Controller::clear_history() noexcept {
    session_.clear_history();
}

std::string Controller::programmer_representations_text() const {
    if (state_.mode != Mode::Programmer || state_.expression.empty()) {
        return "Enter a Programmer value.";
    }

    const ProgrammerResult result = evaluate_programmer(
        state_.expression, state_.programmer_base, state_.programmer_width);
    if (!result.ok) return "Error: " + result.error;

    const auto representations = programmer_representations(
        result.value, state_.programmer_width, state_.programmer_signed);

    auto grouped_binary = [](const std::string& binary) {
        std::string grouped;
        grouped.reserve(binary.size() + binary.size() / 4U);
        for (std::size_t i = 0; i < binary.size(); ++i) {
            if (i != 0 && (binary.size() - i) % 4U == 0U) {
                grouped.push_back(' ');
            }
            grouped.push_back(binary[i]);
        }
        return grouped;
    };

    return "HEX  " + representations.hexadecimal +
           "\nDEC  " + representations.decimal +
           "\nOCT  " + representations.octal +
           "\nBIN  " + grouped_binary(representations.binary);
}

std::string Controller::scientific_status_text() const {
    const char* angle = "DEG";
    if (state_.angle_unit == AngleUnit::Radians) angle = "RAD";
    else if (state_.angle_unit == AngleUnit::Gradians) angle = "GRAD";

    std::string status = "SCIENTIFIC · ";
    status += angle;
    if (state_.scientific_second) status += " · 2ND";
    if (state_.scientific_hyperbolic) status += " · HYP";
    if (state_.scientific_notation) status += " · F-E";
    return status;
}

std::string Controller::format_real(double value) const {
    return state_.mode == Mode::Scientific && state_.scientific_notation
        ? calculator::format_scientific_value(value)
        : calculator::format_value(value);
}

Command Controller::effective_scientific_command(Command command) const {
    if (state_.mode != Mode::Scientific) return command;

    if (command == Command::Sin || command == Command::Cos ||
        command == Command::Tan) {
        if (state_.scientific_hyperbolic) {
            if (state_.scientific_second) {
                if (command == Command::Sin) return Command::Asinh;
                if (command == Command::Cos) return Command::Acosh;
                return Command::Atanh;
            }
            if (command == Command::Sin) return Command::Sinh;
            if (command == Command::Cos) return Command::Cosh;
            return Command::Tanh;
        }

        if (state_.scientific_second) {
            if (command == Command::Sin) return Command::Asin;
            if (command == Command::Cos) return Command::Acos;
            return Command::Atan;
        }
        return command;
    }

    if (!state_.scientific_second) return command;

    switch (command) {
    case Command::SquareRoot: return Command::Square;
    case Command::CubeRoot: return Command::Cube;
    case Command::Abs: return Command::Floor;
    case Command::Percent: return Command::Ceil;
    case Command::Log10: return Command::TenPower;
    case Command::Exp: return Command::TwoPower;
    default: return command;
    }
}

std::string Controller::button_label(
    Command command, std::string_view fallback) const {
    if (state_.mode != Mode::Scientific) return std::string(fallback);

    switch (command) {
    case Command::CycleAngleUnit:
        if (state_.angle_unit == AngleUnit::Degrees) return "DEG";
        if (state_.angle_unit == AngleUnit::Radians) return "RAD";
        return "GRAD";
    case Command::Sin: {
        const Command effective = effective_scientific_command(command);
        if (effective == Command::Asin) return "asin";
        if (effective == Command::Sinh) return "sinh";
        if (effective == Command::Asinh) return "asinh";
        return "sin";
    }
    case Command::Cos: {
        const Command effective = effective_scientific_command(command);
        if (effective == Command::Acos) return "acos";
        if (effective == Command::Cosh) return "cosh";
        if (effective == Command::Acosh) return "acosh";
        return "cos";
    }
    case Command::Tan: {
        const Command effective = effective_scientific_command(command);
        if (effective == Command::Atan) return "atan";
        if (effective == Command::Tanh) return "tanh";
        if (effective == Command::Atanh) return "atanh";
        return "tan";
    }
    case Command::SquareRoot:
        return state_.scientific_second ? "x²" : "√";
    case Command::CubeRoot:
        return state_.scientific_second ? "x³" : "∛";
    case Command::Abs:
        return state_.scientific_second ? "floor" : "abs";
    case Command::Percent:
        return state_.scientific_second ? "ceil" : "%";
    case Command::Log10:
        return state_.scientific_second ? "10ˣ" : "log";
    case Command::Exp:
        return state_.scientific_second ? "2ˣ" : "eˣ";
    default:
        return std::string(fallback);
    }
}

void Controller::set_status(std::string text, bool fault) {
    state_.status = std::move(text);
    state_.fault = fault;
}

std::size_t Controller::normalized_cursor(std::size_t cursor) const noexcept {
    return cursor == kEnd ? state_.expression.size()
                          : std::min(cursor, state_.expression.size());
}

DispatchResult Controller::insert(std::string_view text, std::size_t cursor) {
    const std::size_t position = normalized_cursor(cursor);
    state_.expression.insert(position, text.data(), text.size());
    state_.fault = false;
    if (state_.mode == Mode::Standard) update_standard_preview();
    return {position + text.size(), true};
}

DispatchResult Controller::backspace(std::size_t cursor) {
    const std::size_t position = normalized_cursor(cursor);
    if (position == 0) return {0, false};

    state_.expression.erase(position - 1, 1);
    state_.fault = false;
    if (state_.mode == Mode::Standard) update_standard_preview();
    return {position - 1, true};
}

bool Controller::current_value(double& value) {
    const Result result =
        state_.mode == Mode::Standard
            ? evaluate_immediate(state_.expression)
            : calculator::evaluate(state_.expression, session_.variables());

    if (!result.ok) {
        state_.result = "Error: " + result.error;
        set_status("CALCULATION ERROR", true);
        return false;
    }

    value = result.value;
    return true;
}

bool Controller::current_programmer_value(std::uint64_t& value) {
    const ProgrammerResult result = evaluate_programmer(
        state_.expression, state_.programmer_base, state_.programmer_width);
    if (!result.ok) {
        state_.result = "Error: " + result.error;
        set_status("PROGRAMMER ERROR", true);
        return false;
    }

    value = result.value;
    return true;
}

std::string Controller::programmer_status_text() const {
    const char* base = "DEC";
    switch (state_.programmer_base) {
    case ProgrammerBase::Binary: base = "BIN"; break;
    case ProgrammerBase::Octal: base = "OCT"; break;
    case ProgrammerBase::Decimal: base = "DEC"; break;
    case ProgrammerBase::Hexadecimal: base = "HEX"; break;
    }

    std::ostringstream out;
    out << "PROGRAMMER · " << base
        << " · " << static_cast<unsigned>(state_.programmer_width)
        << " BIT · " << (state_.programmer_signed ? "SIGNED" : "UNSIGNED");
    return out.str();
}

void Controller::calculate_programmer(bool record_history) {
    const ProgrammerResult result = evaluate_programmer(
        state_.expression, state_.programmer_base, state_.programmer_width);

    HistoryContext context{};
    switch (state_.programmer_base) {
    case ProgrammerBase::Binary: context.programmer_base = 2; break;
    case ProgrammerBase::Octal: context.programmer_base = 8; break;
    case ProgrammerBase::Decimal: context.programmer_base = 10; break;
    case ProgrammerBase::Hexadecimal: context.programmer_base = 16; break;
    }
    context.programmer_width =
        static_cast<unsigned>(state_.programmer_width);
    context.programmer_signed = state_.programmer_signed;

    if (!result.ok) {
        state_.result = "Error: " + result.error;
        if (record_history) {
            session_.record_history_text(
                state_.expression, state_.result, false,
                HistoryKind::Programmer, context);
        }
        set_status("PROGRAMMER ERROR", true);
        return;
    }

    state_.result = format_programmer(
        result.value, state_.programmer_base,
        state_.programmer_width, state_.programmer_signed);
    if (record_history) {
        session_.record_history_text(
            state_.expression, state_.result, true,
            HistoryKind::Programmer, context);
    }
    set_status(programmer_status_text());
}

void Controller::calculate_standard() {
    const Result result = evaluate_immediate(state_.expression);
    session_.record_history(
        state_.expression, result, HistoryKind::Standard);

    if (!result.ok) {
        state_.result = "Error: " + result.error;
        set_status("CALCULATION ERROR", true);
        return;
    }

    state_.result = calculator::format_value(result.value);
    set_status("READY");
}

void Controller::calculate() {
    if (state_.mode == Mode::Programmer) {
        calculate_programmer();
        return;
    }
    if (state_.mode == Mode::Standard) {
        calculate_standard();
        return;
    }

    const Result result = session_.evaluate(state_.expression);
    if (!result.ok) {
        state_.result = "Error: " + result.error;
        set_status("CALCULATION ERROR", true);
        return;
    }

    state_.result = format_real(result.value);
    set_status(scientific_status_text());
}

void Controller::clear_calculation() {
    state_.expression.clear();
    state_.result = "0";

    if (state_.mode == Mode::Programmer) {
        set_status(programmer_status_text());
    } else if (state_.mode == Mode::Scientific) {
        set_status(scientific_status_text());
    } else {
        set_status("READY");
    }
}

void Controller::unary_transform(Command command) {
    double value = 0.0;
    if (!current_value(value)) return;

    if (command == Command::Negate) {
        value = -value;
        state_.expression = format_real(value);
        state_.result = state_.expression;
        set_status("READY");
        return;
    }

    RealFunction function = RealFunction::Abs;
    switch (command) {
    case Command::Square: function = RealFunction::Square; break;
    case Command::SquareRoot: function = RealFunction::SquareRoot; break;
    case Command::Reciprocal: function = RealFunction::Reciprocal; break;
    default: return;
    }

    const Result transformed =
        calculator::apply_real_function(function, value);
    if (!transformed.ok) {
        set_status(
            transformed.error == "division by zero"
                ? "DIVISION BY ZERO"
                : "DOMAIN ERROR",
            true);
        return;
    }

    state_.expression = format_real(transformed.value);
    state_.result = state_.expression;
    set_status(state_.mode == Mode::Scientific
                   ? scientific_status_text()
                   : "READY");
}

void Controller::scientific_transform(Command command) {
    double value = 0.0;
    if (!current_value(value)) return;

    command = effective_scientific_command(command);

    RealFunction function = RealFunction::Abs;
    switch (command) {
    case Command::Square: function = RealFunction::Square; break;
    case Command::Cube: function = RealFunction::Cube; break;
    case Command::SquareRoot: function = RealFunction::SquareRoot; break;
    case Command::CubeRoot: function = RealFunction::Cbrt; break;
    case Command::Reciprocal: function = RealFunction::Reciprocal; break;
    case Command::Sin: function = RealFunction::Sin; break;
    case Command::Cos: function = RealFunction::Cos; break;
    case Command::Tan: function = RealFunction::Tan; break;
    case Command::Asin: function = RealFunction::Asin; break;
    case Command::Acos: function = RealFunction::Acos; break;
    case Command::Atan: function = RealFunction::Atan; break;
    case Command::Sinh: function = RealFunction::Sinh; break;
    case Command::Cosh: function = RealFunction::Cosh; break;
    case Command::Tanh: function = RealFunction::Tanh; break;
    case Command::Asinh: function = RealFunction::Asinh; break;
    case Command::Acosh: function = RealFunction::Acosh; break;
    case Command::Atanh: function = RealFunction::Atanh; break;
    case Command::Ln: function = RealFunction::Ln; break;
    case Command::Log10: function = RealFunction::Log10; break;
    case Command::Exp: function = RealFunction::Exp; break;
    case Command::TwoPower: function = RealFunction::TwoPower; break;
    case Command::TenPower: function = RealFunction::TenPower; break;
    case Command::Abs: function = RealFunction::Abs; break;
    case Command::Floor: function = RealFunction::Floor; break;
    case Command::Ceil: function = RealFunction::Ceil; break;
    default: return;
    }

    const Result transformed = calculator::apply_real_function(
        function, value, state_.angle_unit);
    if (!transformed.ok) {
        set_status(
            transformed.error == "division by zero"
                ? "DIVISION BY ZERO"
                : "DOMAIN ERROR",
            true);
        return;
    }

    state_.expression = format_real(transformed.value);
    state_.result = state_.expression;
    set_status(scientific_status_text());
}

void Controller::programmer_mode_change(Command command) {
    switch (command) {
    case Command::BaseBin: state_.programmer_base = ProgrammerBase::Binary; break;
    case Command::BaseOct: state_.programmer_base = ProgrammerBase::Octal; break;
    case Command::BaseDec: state_.programmer_base = ProgrammerBase::Decimal; break;
    case Command::BaseHex: state_.programmer_base = ProgrammerBase::Hexadecimal; break;
    case Command::Width8: state_.programmer_width = IntegerWidth::Bits8; break;
    case Command::Width16: state_.programmer_width = IntegerWidth::Bits16; break;
    case Command::Width32: state_.programmer_width = IntegerWidth::Bits32; break;
    case Command::Width64: state_.programmer_width = IntegerWidth::Bits64; break;
    case Command::ToggleSigned:
        state_.programmer_signed = !state_.programmer_signed;
        break;
    default:
        return;
    }

    if (!state_.expression.empty()) calculate_programmer(false);
    else set_status(programmer_status_text());
}

void Controller::update_standard_preview() {
    if (state_.mode != Mode::Standard || state_.expression.empty()) return;

    std::string preview = state_.expression;
    while (!preview.empty() &&
           std::isspace(static_cast<unsigned char>(preview.back()))) {
        preview.pop_back();
    }
    if (preview.empty()) return;

    const char last = preview.back();
    if (last == '+' || last == '*' || last == '/' || last == '^') {
        preview.pop_back();
    } else if (last == '-' && preview.size() > 1U) {
        const char previous = preview[preview.size() - 2U];
        if (std::isdigit(static_cast<unsigned char>(previous)) ||
            previous == '.' || previous == '%') {
            preview.pop_back();
        }
    }

    if (preview.empty()) return;
    const Result result = evaluate_immediate(preview);
    if (result.ok) {
        state_.result = calculator::format_value(result.value);
        set_status("READY");
    }
}

bool Controller::current_number_has_decimal() const {
    for (auto it = state_.expression.rbegin(); it != state_.expression.rend(); ++it) {
        const char ch = *it;
        if (ch == '.') return true;
        if (!std::isdigit(static_cast<unsigned char>(ch))) break;
    }
    return false;
}

bool Controller::has_unmatched_open_parenthesis() const {
    int depth = 0;
    for (char ch : state_.expression) {
        if (ch == '(') ++depth;
        else if (ch == ')' && depth > 0) --depth;
    }
    return depth > 0;
}

bool Controller::expression_ends_with_binary_operator() const {
    if (state_.expression.empty()) return false;
    const char ch = state_.expression.back();
    return ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '^' ||
           ch == '&' || ch == '|';
}

bool Controller::expression_has_value() const {
    if (state_.expression.empty()) return false;

    if (state_.mode == Mode::Programmer) {
        const ProgrammerResult result = evaluate_programmer(
            state_.expression, state_.programmer_base, state_.programmer_width);
        return result.ok;
    }

    if (state_.mode == Mode::Standard) {
        return evaluate_immediate(state_.expression).ok;
    }

    return calculator::evaluate(
        state_.expression, session_.variables()).ok;
}

bool Controller::command_enabled(Command command) const {
    if (state_.mode == Mode::Scientific) {
        command = effective_scientific_command(command);
    }

    switch (command) {
    case Command::MemoryClear:
    case Command::MemoryRecall:
        return !session_.memory_empty();
    case Command::MemoryStore:
    case Command::MemoryAdd:
    case Command::MemorySubtract:
        return expression_has_value();
    case Command::Backspace:
        return !state_.expression.empty();
    case Command::Equals:
        return expression_has_value();
    case Command::Percent:
    case Command::Reciprocal:
    case Command::Square:
    case Command::Cube:
    case Command::SquareRoot:
    case Command::CubeRoot:
    case Command::Negate:
    case Command::Sin:
    case Command::Cos:
    case Command::Tan:
    case Command::Asin:
    case Command::Acos:
    case Command::Atan:
    case Command::Sinh:
    case Command::Cosh:
    case Command::Tanh:
    case Command::Asinh:
    case Command::Acosh:
    case Command::Atanh:
    case Command::Ln:
    case Command::Log10:
    case Command::Exp:
    case Command::TwoPower:
    case Command::TenPower:
    case Command::Abs:
    case Command::Floor:
    case Command::Ceil:
    case Command::Factorial:
        return expression_has_value();
    case Command::DecimalPoint:
        return state_.mode != Mode::Programmer && !current_number_has_decimal();
    case Command::CloseParen:
        return has_unmatched_open_parenthesis();
    case Command::Digit2:
    case Command::Digit3:
    case Command::Digit4:
    case Command::Digit5:
    case Command::Digit6:
    case Command::Digit7:
        return state_.mode != Mode::Programmer ||
               state_.programmer_base != ProgrammerBase::Binary;
    case Command::Digit8:
    case Command::Digit9:
        return state_.mode != Mode::Programmer ||
               state_.programmer_base == ProgrammerBase::Decimal ||
               state_.programmer_base == ProgrammerBase::Hexadecimal;
    case Command::HexA:
    case Command::HexB:
    case Command::HexC:
    case Command::HexD:
    case Command::HexE:
    case Command::HexF:
        return state_.mode == Mode::Programmer &&
               state_.programmer_base == ProgrammerBase::Hexadecimal;
    case Command::Divide:
    case Command::Multiply:
    case Command::Subtract:
    case Command::Add:
    case Command::Power:
    case Command::BitAnd:
    case Command::BitOr:
    case Command::BitXor:
    case Command::ShiftLeft:
    case Command::ShiftRight:
    case Command::RotateLeft:
    case Command::RotateRight:
    case Command::BitNand:
    case Command::BitNor:
        return !state_.expression.empty();
    default:
        return true;
    }
}

DispatchResult Controller::dispatch(Command command, std::size_t cursor) {
    if (!command_enabled(command)) {
        return {normalized_cursor(cursor), false};
    }

    if (state_.mode == Mode::Programmer) {
        if (is_programmer_selector(command)) {
            programmer_mode_change(command);
            return {normalized_cursor(cursor), false};
        }
        if (command == Command::Equals) {
            calculate_programmer();
            return {normalized_cursor(cursor), false};
        }
        if (command == Command::AllClear) {
            clear_calculation();
            return {0, true};
        }
        if (command == Command::Backspace) {
            return backspace(cursor);
        }

        const std::string_view text = insertion_text(command);
        if (!text.empty()) return insert(text, cursor);
        return {normalized_cursor(cursor), false};
    }

    if (state_.mode == Mode::Scientific) {
        switch (command) {
        case Command::CycleAngleUnit:
            if (state_.angle_unit == AngleUnit::Degrees) {
                state_.angle_unit = AngleUnit::Radians;
            } else if (state_.angle_unit == AngleUnit::Radians) {
                state_.angle_unit = AngleUnit::Gradians;
            } else {
                state_.angle_unit = AngleUnit::Degrees;
            }
            set_status(scientific_status_text());
            return {normalized_cursor(cursor), false};
        case Command::ToggleSecond:
            state_.scientific_second = !state_.scientific_second;
            set_status(scientific_status_text());
            return {normalized_cursor(cursor), false};
        case Command::ToggleHyperbolic:
            state_.scientific_hyperbolic = !state_.scientific_hyperbolic;
            set_status(scientific_status_text());
            return {normalized_cursor(cursor), false};
        case Command::ToggleScientificNotation: {
            state_.scientific_notation = !state_.scientific_notation;
            double value = 0.0;
            if (current_value(value)) {
                state_.result = format_real(value);
                state_.fault = false;
            }
            set_status(scientific_status_text());
            return {normalized_cursor(cursor), false};
        }
        default:
            break;
        }
    }

    const Command effective =
        state_.mode == Mode::Scientific
            ? effective_scientific_command(command)
            : command;

    switch (effective) {
    case Command::Equals:
        calculate();
        return {normalized_cursor(cursor), false};
    case Command::Clear:
        clear_calculation();
        return {0, true};
    case Command::Backspace:
        return backspace(cursor);
    case Command::Negate:
    case Command::Square:
    case Command::SquareRoot:
    case Command::Reciprocal:
        if (state_.mode == Mode::Scientific) {
            scientific_transform(effective);
        } else {
            unary_transform(effective);
        }
        return {state_.expression.size(), true};
    case Command::Cube:
    case Command::CubeRoot:
    case Command::Sin:
    case Command::Cos:
    case Command::Tan:
    case Command::Asin:
    case Command::Acos:
    case Command::Atan:
    case Command::Sinh:
    case Command::Cosh:
    case Command::Tanh:
    case Command::Asinh:
    case Command::Acosh:
    case Command::Atanh:
    case Command::Ln:
    case Command::Log10:
    case Command::Exp:
    case Command::TwoPower:
    case Command::TenPower:
    case Command::Abs:
    case Command::Floor:
    case Command::Ceil:
        scientific_transform(effective);
        return {state_.expression.size(), true};
    case Command::MemoryClear:
        session_.memory_clear();
        set_status("MEMORY CLEARED");
        return {normalized_cursor(cursor), false};
    case Command::MemoryRecall:
        set_status("MEMORY RECALL");
        return insert(calculator::format_value(session_.memory_recall()), cursor);
    case Command::MemoryStore:
    case Command::MemoryAdd:
    case Command::MemorySubtract: {
        double value = 0.0;
        if (current_value(value)) {
            if (effective == Command::MemoryStore) session_.memory_store(value);
            else if (effective == Command::MemoryAdd) session_.memory_add(value);
            else session_.memory_subtract(value);
            set_status(effective == Command::MemoryStore
                           ? "MEMORY STORED"
                           : "MEMORY UPDATED");
        }
        return {normalized_cursor(cursor), false};
    }
    case Command::Pi:
        return insert("pi", cursor);
    case Command::Euler:
        return insert("e", cursor);
    case Command::Factorial:
        return insert("!", cursor);
    default:
        break;
    }

    const std::string_view text = insertion_text(effective);
    if (!text.empty()) {
        if (state_.mode == Mode::Standard &&
            (effective == Command::Divide || effective == Command::Multiply ||
             effective == Command::Subtract || effective == Command::Add ||
             effective == Command::Power) &&
            expression_ends_with_binary_operator() &&
            normalized_cursor(cursor) == state_.expression.size()) {
            state_.expression.back() = text.front();
            update_standard_preview();
            return {state_.expression.size(), true};
        }
        return insert(text, cursor);
    }
    return {normalized_cursor(cursor), false};
}

} // namespace calculator::ui
