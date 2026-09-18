/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator_ui_controller.hpp"

#include <algorithm>
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
    return {position + text.size(), true};
}

DispatchResult Controller::backspace(std::size_t cursor) {
    const std::size_t position = normalized_cursor(cursor);
    if (position == 0) return {0, false};

    state_.expression.erase(position - 1, 1);
    return {position - 1, true};
}

bool Controller::current_value(double& value) {
    const Result result = session_.evaluate(state_.expression);
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

void Controller::calculate() {
    if (state_.mode == Mode::Programmer) {
        calculate_programmer();
        return;
    }

    double value = 0.0;
    if (!current_value(value)) return;

    state_.result = format_value(value);
    if (state_.mode == Mode::Scientific) {
        set_status(state_.degrees ? "SCIENTIFIC · DEGREES"
                                  : "SCIENTIFIC · RADIANS");
    } else {
        set_status("READY");
    }
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

DispatchResult Controller::dispatch(Command command, std::size_t cursor) {
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
    if (!text.empty()) return insert(text, cursor);
    return {normalized_cursor(cursor), false};
}

} // namespace infiltrator::calc::ui
