/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator_ui_controller.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace infiltrator::calc::ui {
namespace {

constexpr double kPi = 3.14159265358979323846;

std::string format_value(double value) {
    std::ostringstream out;
    out << std::setprecision(15) << value;
    return out.str();
}

} // namespace

void Controller::set_expression(std::string expression) {
    state_.expression = std::move(expression);
    state_.fault = false;
    if (state_.mode == Mode::Standard) update_standard_preview();
}

void Controller::set_mode(Mode mode) {
    state_.mode = mode;
    if (mode == Mode::Scientific) {
        set_status(state_.degrees ? "SCIENTIFIC · DEGREES"
                                  : "SCIENTIFIC · RADIANS");
    } else if (mode == Mode::Programmer) {
        set_status(programmer_status_text());
    } else {
        set_status("READY");
    }
}

void Controller::clear_history() noexcept {
    session_.clear_history();
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
    Result result;
    if (state_.mode == Mode::Standard) {
        result = evaluate_immediate(state_.expression);
        if (!result.ok) {
            result = infiltrator::calc::evaluate(
                state_.expression, session_.variables());
        }
    } else {
        result = infiltrator::calc::evaluate(
            state_.expression, session_.variables());
    }

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

void Controller::calculate_programmer() {
    std::uint64_t value = 0;
    if (!current_programmer_value(value)) return;

    state_.result = format_programmer(
        value, state_.programmer_base,
        state_.programmer_width, state_.programmer_signed);
    set_status(programmer_status_text());
}

void Controller::calculate_standard() {
    Result result = evaluate_immediate(state_.expression);
    if (result.ok) {
        session_.record_history(state_.expression, result);
    } else {
        result = session_.evaluate(state_.expression);
    }

    if (!result.ok) {
        state_.result = "Error: " + result.error;
        set_status("CALCULATION ERROR", true);
        return;
    }

    state_.result = format_value(result.value);
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

    state_.result = format_value(result.value);
    set_status(state_.degrees ? "SCIENTIFIC · DEGREES"
                              : "SCIENTIFIC · RADIANS");
}

void Controller::clear_calculation() {
    state_.expression.clear();
    state_.result = "0";

    if (state_.mode == Mode::Programmer) {
        set_status(programmer_status_text());
    } else if (state_.mode == Mode::Scientific) {
        set_status(state_.degrees ? "SCIENTIFIC · DEGREES"
                                  : "SCIENTIFIC · RADIANS");
    } else {
        set_status("READY");
    }
}

void Controller::unary_transform(Command command) {
    double value = 0.0;
    if (!current_value(value)) return;

    switch (command) {
    case Command::Negate:
        value = -value;
        break;
    case Command::Square:
        value *= value;
        break;
    case Command::SquareRoot:
        if (value < 0.0) {
            set_status("DOMAIN ERROR", true);
            return;
        }
        value = std::sqrt(value);
        break;
    case Command::Reciprocal:
        if (value == 0.0) {
            set_status("DIVISION BY ZERO", true);
            return;
        }
        value = 1.0 / value;
        break;
    default:
        return;
    }

    state_.expression = format_value(value);
    state_.result = state_.expression;
    set_status("READY");
}

void Controller::scientific_transform(Command command) {
    double value = 0.0;
    if (!current_value(value)) return;

    double argument = value;
    if (state_.degrees &&
        (command == Command::Sin ||
         command == Command::Cos ||
         command == Command::Tan)) {
        argument = value * kPi / 180.0;
    }

    switch (command) {
    case Command::Sin: value = std::sin(argument); break;
    case Command::Cos: value = std::cos(argument); break;
    case Command::Tan: value = std::tan(argument); break;
    case Command::Asin:
        value = std::asin(value);
        if (state_.degrees) value = value * 180.0 / kPi;
        break;
    case Command::Acos:
        value = std::acos(value);
        if (state_.degrees) value = value * 180.0 / kPi;
        break;
    case Command::Atan:
        value = std::atan(value);
        if (state_.degrees) value = value * 180.0 / kPi;
        break;
    case Command::Ln: value = std::log(value); break;
    case Command::Log10: value = std::log10(value); break;
    case Command::Exp: value = std::exp(value); break;
    case Command::Abs: value = std::fabs(value); break;
    default: return;
    }

    if (!std::isfinite(value)) {
        set_status("DOMAIN ERROR", true);
        return;
    }

    state_.expression = format_value(value);
    state_.result = state_.expression;
    set_status(state_.degrees ? "SCIENTIFIC · DEGREES"
                              : "SCIENTIFIC · RADIANS");
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

    if (!state_.expression.empty()) calculate_programmer();
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
        state_.result = format_value(result.value);
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
        Result result = evaluate_immediate(state_.expression);
        if (result.ok) return true;
    }

    return infiltrator::calc::evaluate(
        state_.expression, session_.variables()).ok;
}

bool Controller::command_enabled(Command command) const {
    switch (command) {
    case Command::MemoryClear:
    case Command::MemoryRecall:
        return !session_.memory_empty();
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
    case Command::SquareRoot:
    case Command::Negate:
    case Command::Sin:
    case Command::Cos:
    case Command::Tan:
    case Command::Asin:
    case Command::Acos:
    case Command::Atan:
    case Command::Ln:
    case Command::Log10:
    case Command::Exp:
    case Command::Abs:
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

    switch (command) {
    case Command::ToggleDegrees:
        state_.degrees = !state_.degrees;
        set_status(state_.degrees ? "SCIENTIFIC · DEGREES"
                                  : "SCIENTIFIC · RADIANS");
        return {normalized_cursor(cursor), false};
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
        unary_transform(command);
        return {state_.expression.size(), true};
    case Command::MemoryClear:
        session_.memory_clear();
        set_status("MEMORY CLEARED");
        return {normalized_cursor(cursor), false};
    case Command::MemoryRecall:
        set_status("MEMORY RECALL");
        return insert(format_value(session_.memory_recall()), cursor);
    case Command::MemoryAdd:
    case Command::MemorySubtract: {
        double value = 0.0;
        if (current_value(value)) {
            if (command == Command::MemoryAdd) session_.memory_add(value);
            else session_.memory_subtract(value);
            set_status("MEMORY UPDATED");
        }
        return {normalized_cursor(cursor), false};
    }
    case Command::Sin:
    case Command::Cos:
    case Command::Tan:
    case Command::Asin:
    case Command::Acos:
    case Command::Atan:
    case Command::Ln:
    case Command::Log10:
    case Command::Exp:
    case Command::Abs:
        scientific_transform(command);
        return {state_.expression.size(), true};
    case Command::Pi:
        return insert("pi", cursor);
    case Command::Euler:
        return insert("e", cursor);
    case Command::Factorial:
        return insert("!", cursor);
    case Command::CubeRoot:
        return insert("cbrt(", cursor);
    default:
        break;
    }

    const std::string_view text = insertion_text(command);
    if (!text.empty()) {
        if (state_.mode == Mode::Standard &&
            (command == Command::Divide || command == Command::Multiply ||
             command == Command::Subtract || command == Command::Add ||
             command == Command::Power) &&
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

} // namespace infiltrator::calc::ui
