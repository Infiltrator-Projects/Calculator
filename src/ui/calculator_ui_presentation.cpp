/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator_ui_controller.hpp"

#include <infiltratr/core.h>

#include <algorithm>
#include <charconv>
#include <sstream>
#include <system_error>
#include <utility>

namespace calculator::ui {

std::vector<AdditionalResult> Controller::additional_results() const {
    if (state_.mode == Mode::Programmer) {
        // Programmer has a well-defined zero bit-pattern even before the user
        // types an expression. Keep Bases/Bits useful in that initial state and
        // consistent with programmer_bits(), which already exposes zero bits.
        const std::string expression =
            state_.expression.empty() ? "0" : state_.expression;
        const ProgrammerResult result = evaluate_programmer(
            expression, state_.programmer_base,
            state_.programmer_width);
        if (!result.ok) {
            return {{"Error", result.error}};
        }

        const auto representations = programmer_representations(
            result.value, state_.programmer_width,
            state_.programmer_signed);

        return {
            {"HEX", group_programmer_digits(
                representations.hexadecimal, ProgrammerBase::Hexadecimal)},
            {"DEC", representations.decimal},
            {"OCT", group_programmer_digits(
                representations.octal, ProgrammerBase::Octal)},
            {"BIN", group_programmer_digits(
                representations.binary, ProgrammerBase::Binary)}
        };
    }

    if (state_.expression.empty()) return {};

    if (state_.mode == Mode::Scientific) {
        const ScientificResult result =
            session_.preview_scientific(
                state_.expression, state_.angle_unit,
                scientific_digits_);
        if (!result.ok) {
            return {{"Error", result.error}};
        }
        if (!result.display.empty()) {
            return {{"Result", result.display}};
        }
        return {
            {"Decimal", calculator::format_scientific_value(
                result.value, scientific_digits_)},
            {"Scientific", calculator::format_scientific_value(
                result.value, scientific_digits_, true)},
            {"Engineering", calculator::format_engineering_value(
                result.value, 13U)}
        };
    }

    const Result result = evaluate_standard(state_.expression);
    if (!result.ok) {
        return {{"Error", result.error}};
    }

    return {
        {"Decimal", calculator::format_value(result.value)},
        {"Scientific", calculator::format_scientific_value(result.value)},
        {"Engineering", calculator::format_engineering_value(result.value)}
    };
}

std::string Controller::additional_results_text() const {
    const auto rows = additional_results();
    if (rows.empty()) return "Enter a value or expression.";

    std::ostringstream out;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        if (index != 0) out << '\n';
        out << rows[index].label << "  " << rows[index].value;
    }
    return out.str();
}

std::string Controller::programmer_representations_text() const {
    if (state_.mode != Mode::Programmer) {
        return "Programmer mode is not active.";
    }
    return additional_results_text();
}

std::vector<bool> Controller::programmer_bits() const {
    const unsigned width = static_cast<unsigned>(state_.programmer_width);
    std::uint64_t value = 0U;
    if (!state_.expression.empty()) {
        const ProgrammerResult result = evaluate_programmer(
            state_.expression, state_.programmer_base,
            state_.programmer_width);
        if (!result.ok) return {};
        value = result.value;
    }
    std::vector<bool> bits(width, false);
    for (unsigned bit = 0; bit < width; ++bit) {
        bits[bit] = ((value >> bit) & 1ULL) != 0U;
    }
    return bits;
}

bool Controller::toggle_programmer_bit(unsigned bit) {
    if (state_.mode != Mode::Programmer) return false;
    const unsigned width = static_cast<unsigned>(state_.programmer_width);
    if (bit >= width) return false;

    std::uint64_t value = 0U;
    if (!state_.expression.empty()) {
        const ProgrammerResult result = evaluate_programmer(
            state_.expression, state_.programmer_base,
            state_.programmer_width);
        if (!result.ok) return false;
        value = result.value;
    }
    value ^= (1ULL << bit);
    state_.expression = format_programmer(
        value, state_.programmer_base, state_.programmer_width, false);
    state_.result = group_programmer_digits(
        format_programmer(
            value, state_.programmer_base, state_.programmer_width,
            state_.programmer_signed),
        state_.programmer_base);
    refresh_evaluation_cache();
    set_status(programmer_status_text());
    return true;
}

bool Controller::swap_programmer_endianness() {
    if (state_.mode != Mode::Programmer) return false;

    std::uint64_t value = 0U;
    if (!state_.expression.empty()) {
        const ProgrammerResult result = evaluate_programmer(
            state_.expression, state_.programmer_base,
            state_.programmer_width);
        if (!result.ok) return false;
        value = result.value;
    }

    value = calculator::swap_programmer_endianness(
        value, state_.programmer_width);
    state_.expression = format_programmer(
        value, state_.programmer_base, state_.programmer_width, false);
    state_.result = group_programmer_digits(
        format_programmer(
            value, state_.programmer_base, state_.programmer_width,
            state_.programmer_signed),
        state_.programmer_base);
    refresh_evaluation_cache();
    set_status(programmer_status_text());
    return true;
}

void Controller::set_display_preferences(DisplayPreferences preferences) {
    preferences.decimal_places =
        std::min<unsigned>(preferences.decimal_places, 15U);
    display_preferences_ = preferences;

    if (state_.mode == Mode::Scientific &&
        scientific_cache_.ok &&
        scientific_cache_.display.empty()) {
        state_.result =
            format_scientific_result(scientific_cache_.value);
    } else if (state_.mode == Mode::Standard &&
               real_cache_.ok && real_cache_.display.empty()) {
        state_.result = format_display(real_cache_.value);
    }
}

void Controller::set_scientific_digits(unsigned digits) {
    const unsigned bounded = std::clamp(
        digits, kScientificMinDigits, kScientificMaxDigits);
    if (bounded == scientific_digits_) return;
    scientific_digits_ = bounded;
    refresh_evaluation_cache();
    if (state_.mode == Mode::Scientific) update_scientific_preview();
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

std::string Controller::format_display(double value) const {
    const auto trim_fraction_zeroes = [](std::string text) {
        const std::size_t exponent = text.find_first_of("eE");
        const std::size_t mantissa_end =
            exponent == std::string::npos ? text.size() : exponent;
        const std::size_t dot = text.find('.');
        if (dot != std::string::npos && dot < mantissa_end) {
            std::size_t end = mantissa_end;
            while (end > dot + 1U && text[end - 1U] == '0') --end;
            if (end == dot + 1U) --end;
            text.erase(end, mantissa_end - end);
        }
        return text;
    };
    const auto group_integer = [](std::string text) {
        const std::size_t exponent = text.find_first_of("eE");
        const std::size_t mantissa_end =
            exponent == std::string::npos ? text.size() : exponent;
        const std::size_t dot = text.find('.');
        const std::size_t integer_end =
            dot != std::string::npos && dot < mantissa_end ? dot : mantissa_end;
        const std::size_t first_digit =
            !text.empty() && (text.front() == '-' || text.front() == '+')
                ? 1U : 0U;
        if (integer_end <= first_digit + 3U) return text;
        for (std::size_t pos = integer_end; pos > first_digit + 3U;) {
            pos -= 3U;
            text.insert(pos, 1, ',');
        }
        return text;
    };

    std::string text;
    if (display_preferences_.format == ResultFormat::Automatic) {
        text = calculator::format_value(value);
    } else if (display_preferences_.format == ResultFormat::Engineering) {
        text = calculator::format_engineering_value(value);
    } else {
        char buffer[128] = {};
        const std::chars_format style =
            display_preferences_.format == ResultFormat::Fixed
                ? std::chars_format::fixed
                : std::chars_format::scientific;
        const auto converted = std::to_chars(
            buffer, buffer + sizeof(buffer), value, style,
            static_cast<int>(display_preferences_.decimal_places));
        if (converted.ec != std::errc{}) {
            text = calculator::format_value(value);
        } else {
            text.assign(buffer, converted.ptr);
        }
        if (!display_preferences_.trailing_zeroes) {
            text = trim_fraction_zeroes(std::move(text));
        }
    }

    if (display_preferences_.group_thousands &&
        display_preferences_.format != ResultFormat::Scientific &&
        display_preferences_.format != ResultFormat::Engineering) {
        text = group_integer(std::move(text));
    }
    return text;
}

std::string Controller::format_scientific_result(
    const ScientificValue& value) const {
    if (state_.scientific_notation) {
        return calculator::format_scientific_value(
            value, scientific_digits_, true);
    }

    switch (display_preferences_.format) {
    case ResultFormat::Fixed:
        return calculator::format_scientific_display(
            value, ScientificDisplayFormat::Fixed,
            display_preferences_.decimal_places,
            display_preferences_.trailing_zeroes,
            display_preferences_.group_thousands);
    case ResultFormat::Scientific:
        return calculator::format_scientific_display(
            value, ScientificDisplayFormat::Scientific,
            display_preferences_.decimal_places,
            display_preferences_.trailing_zeroes, false);
    case ResultFormat::Engineering:
        return calculator::format_engineering_value(value, 13U);
    case ResultFormat::Automatic:
    default:
        return calculator::format_scientific_display(
            value, ScientificDisplayFormat::General,
            scientific_digits_, false,
            display_preferences_.group_thousands);
    }
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

bool Controller::command_enabled(Command command) const {
    if (state_.mode == Mode::Scientific) {
        command = effective_scientific_command(command);
    }

    switch (command) {
    case Command::MemoryClear:
        return !session_.memory_empty();
    case Command::MemoryRecall:
        return state_.mode == Mode::Scientific
            ? !session_.memory_empty()
            : session_.memory_binary64_available();
    case Command::MemoryStore:
        return expression_has_value();
    case Command::MemoryAdd:
    case Command::MemorySubtract:
        return expression_has_value() &&
               (state_.mode == Mode::Scientific ||
                session_.memory_empty() ||
                session_.memory_binary64_available());
    case Command::ClearEntry:
        return state_.mode == Mode::Standard && !state_.expression.empty();
    case Command::Backspace:
        return !state_.expression.empty();
    case Command::Equals:
        return expression_can_calculate();
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
    case Command::BitXnor:
    case Command::ShiftArithmeticRight:
    case Command::Modulo:
        return !state_.expression.empty();
    case Command::ByteSwap:
        return state_.mode == Mode::Programmer;
    default:
        return true;
    }
}

} // namespace calculator::ui
