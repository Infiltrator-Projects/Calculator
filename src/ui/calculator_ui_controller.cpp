/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator_ui_controller.hpp"

#include <infiltratr/core.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <system_error>
#include <utility>

namespace calculator::ui {

bool Controller::set_expression(std::string expression) {
    if (expression.size() > kMaxExpressionBytes) {
        state_.expression.clear();
        real_cache_ = {};
        scientific_cache_ = {};
        programmer_cache_ = {};
        state_.result =
            "Error: expression exceeds " +
            std::to_string(kMaxExpressionBytes) + "-byte input limit";
        set_status("INPUT LIMIT", true);
        return false;
    }

    state_.expression = std::move(expression);
    state_.fault = false;
    refresh_evaluation_cache();
    if (state_.mode == Mode::Standard) update_standard_preview();
    else if (state_.mode == Mode::Scientific) update_scientific_preview();
    return true;
}

void Controller::set_mode(Mode mode) {
    state_.mode = mode;
    refresh_evaluation_cache();
    if (mode == Mode::Scientific) {
        set_status(scientific_status_text());
        update_scientific_preview();
    } else if (mode == Mode::Programmer) {
        set_status(programmer_status_text());
    } else {
        set_status("READY");
        update_standard_preview();
    }
}

void Controller::set_angle_unit(AngleUnit unit) {
    state_.angle_unit = unit;
    refresh_evaluation_cache();
    if (state_.mode == Mode::Scientific) {
        set_status(scientific_status_text());
        update_scientific_preview();
    }
}

void Controller::set_programmer_context(
    ProgrammerBase base, IntegerWidth width, bool signed_display) {
    state_.programmer_base = base;
    state_.programmer_width = width;
    state_.programmer_signed = signed_display;
    refresh_evaluation_cache();
    if (state_.mode == Mode::Programmer) {
        set_status(programmer_status_text());
        if (!state_.expression.empty()) calculate_programmer(false);
    }
}

std::string Controller::history_text(std::size_t limit,
                                     std::string_view newline) const {
    return session_.history_text(limit, newline);
}

std::size_t Controller::history_count() const noexcept {
    return session_.history_count();
}

std::uint64_t Controller::history_revision() const noexcept {
    return session_.history_revision();
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
    refresh_evaluation_cache();
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

std::string Controller::function_definitions_text() const {
    return session_.function_definitions_text();
}

bool Controller::load_function_definitions_text(std::string_view text) {
    const bool loaded = session_.load_function_definitions_text(text);
    if (loaded) refresh_evaluation_cache();
    return loaded;
}

std::string Controller::variables_text() const {
    return session_.variables_text();
}

bool Controller::load_variables_text(std::string_view text) {
    const bool loaded = session_.load_variables_text(text);
    if (loaded) refresh_evaluation_cache();
    return loaded;
}

std::string Controller::persistent_state_text() const {
    const std::string variables = session_.variables_text();
    const std::string functions = session_.function_definitions_text();

    unsigned programmer_base = 10U;
    switch (state_.programmer_base) {
    case ProgrammerBase::Binary: programmer_base = 2U; break;
    case ProgrammerBase::Octal: programmer_base = 8U; break;
    case ProgrammerBase::Decimal: programmer_base = 10U; break;
    case ProgrammerBase::Hexadecimal: programmer_base = 16U; break;
    }

    std::ostringstream out;
    out << "INFILTRATOR_CALCULATOR_STATE 2\n"
        << "mode=" << static_cast<unsigned>(state_.mode) << '\n'
        << "angle-unit=" << static_cast<unsigned>(state_.angle_unit) << '\n'
        << "programmer-base=" << programmer_base << '\n'
        << "programmer-width="
        << static_cast<unsigned>(state_.programmer_width) << '\n'
        << "programmer-signed=" << (state_.programmer_signed ? 1 : 0) << '\n'
        << "scientific-digits=" << scientific_digits_ << '\n'
        << "result-format="
        << static_cast<unsigned>(display_preferences_.format) << '\n'
        << "decimal-places=" << display_preferences_.decimal_places << '\n'
        << "group-thousands="
        << (display_preferences_.group_thousands ? 1 : 0) << '\n'
        << "trailing-zeroes="
        << (display_preferences_.trailing_zeroes ? 1 : 0) << '\n'
        << "variables-bytes=" << variables.size() << '\n'
        << variables << '\n'
        << "functions-bytes=" << functions.size() << '\n'
        << functions;
    return out.str();
}

bool Controller::load_persistent_state_text(std::string_view text) {
    if (text.size() > kMaxPersistentStateBytes) return false;

    std::size_t cursor = 0U;
    const auto read_line = [&](std::string_view& line) -> bool {
        if (cursor > text.size()) return false;
        const std::size_t end = text.find('\n', cursor);
        if (end == std::string_view::npos) return false;
        line = text.substr(cursor, end - cursor);
        cursor = end + 1U;
        return true;
    };
    const auto parse_unsigned =
        [](std::string_view line, std::string_view key,
           unsigned& value) -> bool {
            if (line.substr(0, key.size()) != key) return false;
            const std::string_view number = line.substr(key.size());
            if (number.empty()) return false;
            unsigned parsed = 0U;
            const auto result = std::from_chars(
                number.data(), number.data() + number.size(), parsed);
            if (result.ec != std::errc{} ||
                result.ptr != number.data() + number.size()) {
                return false;
            }
            value = parsed;
            return true;
        };
    const auto parse_size =
        [](std::string_view line, std::string_view key,
           std::size_t& value) -> bool {
            if (line.substr(0, key.size()) != key) return false;
            const std::string_view number = line.substr(key.size());
            if (number.empty()) return false;
            std::size_t parsed = 0U;
            const auto result = std::from_chars(
                number.data(), number.data() + number.size(), parsed);
            if (result.ec != std::errc{} ||
                result.ptr != number.data() + number.size()) {
                return false;
            }
            value = parsed;
            return true;
        };

    std::string_view line;
    if (!read_line(line)) return false;
    const bool state_v2 = line == "INFILTRATOR_CALCULATOR_STATE 2";
    if (!state_v2 && line != "INFILTRATOR_CALCULATOR_STATE 1") {
        return false;
    }

    unsigned mode = static_cast<unsigned>(state_.mode);
    unsigned angle_unit = static_cast<unsigned>(state_.angle_unit);
    unsigned programmer_base = 10U;
    switch (state_.programmer_base) {
    case ProgrammerBase::Binary: programmer_base = 2U; break;
    case ProgrammerBase::Octal: programmer_base = 8U; break;
    case ProgrammerBase::Decimal: programmer_base = 10U; break;
    case ProgrammerBase::Hexadecimal: programmer_base = 16U; break;
    }
    unsigned programmer_width =
        static_cast<unsigned>(state_.programmer_width);
    unsigned programmer_signed = state_.programmer_signed ? 1U : 0U;

    if (state_v2) {
        if (!read_line(line) || !parse_unsigned(line, "mode=", mode) ||
            mode > static_cast<unsigned>(Mode::Programmer) ||
            !read_line(line) ||
            !parse_unsigned(line, "angle-unit=", angle_unit) ||
            angle_unit > static_cast<unsigned>(AngleUnit::Gradians) ||
            !read_line(line) ||
            !parse_unsigned(line, "programmer-base=", programmer_base) ||
            (programmer_base != 2U && programmer_base != 8U &&
             programmer_base != 10U && programmer_base != 16U) ||
            !read_line(line) ||
            !parse_unsigned(line, "programmer-width=", programmer_width) ||
            (programmer_width != 8U && programmer_width != 16U &&
             programmer_width != 32U && programmer_width != 64U) ||
            !read_line(line) ||
            !parse_unsigned(
                line, "programmer-signed=", programmer_signed) ||
            programmer_signed > 1U) {
            return false;
        }
    }

    unsigned digits = 0U;
    unsigned format = 0U;
    unsigned decimal_places = 0U;
    unsigned group_thousands = 0U;
    unsigned trailing_zeroes = 0U;
    if (!read_line(line) ||
        !parse_unsigned(line, "scientific-digits=", digits) ||
        digits < kScientificMinDigits || digits > kScientificMaxDigits ||
        !read_line(line) ||
        !parse_unsigned(line, "result-format=", format) || format > 3U ||
        !read_line(line) ||
        !parse_unsigned(line, "decimal-places=", decimal_places) ||
        decimal_places > 15U ||
        !read_line(line) ||
        !parse_unsigned(line, "group-thousands=", group_thousands) ||
        group_thousands > 1U ||
        !read_line(line) ||
        !parse_unsigned(line, "trailing-zeroes=", trailing_zeroes) ||
        trailing_zeroes > 1U) {
        return false;
    }

    std::size_t variables_bytes = 0U;
    if (!read_line(line) ||
        !parse_size(line, "variables-bytes=", variables_bytes) ||
        variables_bytes > text.size() - cursor) {
        return false;
    }
    const std::string variables(text.substr(cursor, variables_bytes));
    cursor += variables_bytes;
    if (cursor >= text.size() || text[cursor] != '\n') return false;
    ++cursor;

    std::size_t functions_bytes = 0U;
    if (!read_line(line) ||
        !parse_size(line, "functions-bytes=", functions_bytes) ||
        functions_bytes != text.size() - cursor) {
        return false;
    }
    const std::string functions(text.substr(cursor, functions_bytes));

    // Validate the whole document before mutating live Controller state.
    Session staged;
    if (!staged.load_function_definitions_text(functions) ||
        !staged.load_variables_text(variables)) {
        return false;
    }

    if (!session_.load_function_definitions_text(functions) ||
        !session_.load_variables_text(variables)) {
        return false;
    }

    scientific_digits_ = digits;
    DisplayPreferences preferences;
    preferences.format = static_cast<ResultFormat>(format);
    preferences.decimal_places = decimal_places;
    preferences.group_thousands = group_thousands != 0U;
    preferences.trailing_zeroes = trailing_zeroes != 0U;
    display_preferences_ = preferences;

    if (state_v2) {
        state_.mode = static_cast<Mode>(mode);
        state_.angle_unit = static_cast<AngleUnit>(angle_unit);
        switch (programmer_base) {
        case 2U: state_.programmer_base = ProgrammerBase::Binary; break;
        case 8U: state_.programmer_base = ProgrammerBase::Octal; break;
        case 16U: state_.programmer_base = ProgrammerBase::Hexadecimal; break;
        default: state_.programmer_base = ProgrammerBase::Decimal; break;
        }
        state_.programmer_width =
            static_cast<IntegerWidth>(programmer_width);
        state_.programmer_signed = programmer_signed != 0U;
    }

    refresh_evaluation_cache();
    if (state_.mode == Mode::Scientific) {
        set_status(scientific_status_text());
        update_scientific_preview();
    } else if (state_.mode == Mode::Programmer) {
        set_status(programmer_status_text());
        if (!state_.expression.empty()) calculate_programmer(false);
    } else {
        set_status("READY");
        update_standard_preview();
    }
    return true;
}

std::vector<std::string> Controller::completion_candidates(
    std::string_view prefix) const {
    if (state_.mode != Mode::Scientific || prefix.empty()) return {};

    std::vector<std::string> candidates;
    const auto prefix_matches =
        [](std::string_view candidate, std::string_view wanted,
           bool case_insensitive) {
            if (candidate.size() < wanted.size()) return false;
            for (std::size_t i = 0; i < wanted.size(); ++i) {
                const unsigned char actual =
                    static_cast<unsigned char>(candidate[i]);
                const unsigned char expected =
                    static_cast<unsigned char>(wanted[i]);
                if (case_insensitive) {
                    if (infiltratr_ascii_to_lower(actual) !=
                        infiltratr_ascii_to_lower(expected)) {
                        return false;
                    }
                } else if (actual != expected) {
                    return false;
                }
            }
            return true;
        };
    const auto add = [&](std::string_view candidate,
                         bool case_insensitive = false) {
        if (!prefix_matches(candidate, prefix, case_insensitive)) return;
        candidates.emplace_back(candidate);
    };

    for (const auto function : calculator::scientific_function_catalog()) {
        add(function, true);
    }
    add("rand", true);
    add("i", true);

    for (const auto& constant : calculator::constant_catalog()) {
        add(constant.name, true);
        bool alias_identifier = !constant.alias.empty() &&
            (infiltratr_ascii_is_alpha(
                 static_cast<unsigned char>(constant.alias.front())) ||
             constant.alias.front() == '_');
        for (char ch : constant.alias) {
            alias_identifier = alias_identifier &&
                (infiltratr_ascii_is_alnum(
                     static_cast<unsigned char>(ch)) || ch == '_');
        }
        if (alias_identifier) add(constant.alias, true);
    }

    for (const auto& [name, value] : session_.scientific_variables()) {
        (void)value;
        if (name != "_") add(name);
    }
    for (const auto& [name, definition] : session_.functions()) {
        (void)definition;
        add(name);
    }

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(
        std::unique(candidates.begin(), candidates.end()),
        candidates.end());
    return candidates;
}

DispatchResult Controller::complete_expression(std::size_t cursor) {
    if (state_.mode != Mode::Scientific) {
        return {normalized_cursor(cursor), false};
    }

    const std::size_t point = normalized_cursor(cursor);
    std::size_t begin = point;
    while (begin > 0U) {
        const unsigned char ch =
            static_cast<unsigned char>(state_.expression[begin - 1U]);
        if (!infiltratr_ascii_is_alnum(ch) &&
            state_.expression[begin - 1U] != '_') {
            break;
        }
        --begin;
    }
    if (begin == point) return {point, false};

    const std::string prefix =
        state_.expression.substr(begin, point - begin);
    const auto candidates = completion_candidates(prefix);
    if (candidates.empty()) return {point, false};

    std::string replacement = candidates.front();
    if (candidates.size() > 1U) {
        std::size_t common = replacement.size();
        for (std::size_t i = 1U; i < candidates.size(); ++i) {
            common = std::min(common, candidates[i].size());
            std::size_t matched = 0U;
            while (matched < common &&
                   replacement[matched] == candidates[i][matched]) {
                ++matched;
            }
            common = matched;
        }
        replacement.resize(common);
        if (replacement.size() <= prefix.size()) return {point, false};
    } else {
        bool function = false;
        for (const auto name : calculator::scientific_function_catalog()) {
            if (replacement == name) {
                function = true;
                break;
            }
        }
        if (!function) {
            function =
                session_.functions().find(replacement) !=
                session_.functions().end();
        }
        if (function) replacement.push_back('(');
    }

    const std::size_t replaced = point - begin;
    if (replacement.size() >
        kMaxExpressionBytes - (state_.expression.size() - replaced)) {
        state_.result =
            "Error: expression exceeds " +
            std::to_string(kMaxExpressionBytes) + "-byte input limit";
        set_status("INPUT LIMIT", true);
        return {point, false};
    }

    state_.expression.replace(begin, replaced, replacement);
    state_.fault = false;
    refresh_evaluation_cache();
    update_scientific_preview();
    return {begin + replacement.size(), true};
}

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

        return {
            {"HEX", representations.hexadecimal},
            {"DEC", representations.decimal},
            {"OCT", representations.octal},
            {"BIN", grouped_binary(representations.binary)}
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
    state_.result = format_programmer(
        value, state_.programmer_base, state_.programmer_width,
        state_.programmer_signed);
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

void Controller::set_status(std::string text, bool fault) {
    state_.status = std::move(text);
    state_.fault = fault;
}

std::size_t Controller::normalized_cursor(std::size_t cursor) const noexcept {
    return cursor == kEnd ? state_.expression.size()
                          : std::min(cursor, state_.expression.size());
}

// Cache one side-effect-free interpretation of the current expression after
// each state mutation. Native shells query enablement/labels repeatedly, so
// reparsing there would be both wasteful and a source of semantic drift.
void Controller::refresh_evaluation_cache() {
    real_cache_ = {};
    scientific_cache_ = {};
    programmer_cache_ = {};
    if (state_.expression.empty()) return;

    if (state_.mode == Mode::Programmer) {
        programmer_cache_ = evaluate_programmer(
            state_.expression, state_.programmer_base,
            state_.programmer_width);
    } else if (state_.mode == Mode::Standard) {
        real_cache_ = evaluate_standard(state_.expression);
    } else {
        scientific_cache_ = session_.preview_scientific(
            state_.expression, state_.angle_unit,
            scientific_digits_);
    }
}

DispatchResult Controller::insert(std::string_view text, std::size_t cursor) {
    const std::size_t position = normalized_cursor(cursor);
    if (text.size() > kMaxExpressionBytes - state_.expression.size()) {
        state_.result =
            "Error: expression exceeds " +
            std::to_string(kMaxExpressionBytes) + "-byte input limit";
        set_status("INPUT LIMIT", true);
        return {position, false};
    }
    state_.expression.insert(position, text.data(), text.size());
    state_.fault = false;
    refresh_evaluation_cache();
    if (state_.mode == Mode::Standard) update_standard_preview();
    else if (state_.mode == Mode::Scientific) update_scientific_preview();
    return {position + text.size(), true};
}

DispatchResult Controller::backspace(std::size_t cursor) {
    const std::size_t position = normalized_cursor(cursor);
    if (position == 0) return {0, false};

    state_.expression.erase(position - 1, 1);
    state_.fault = false;
    refresh_evaluation_cache();
    if (state_.mode == Mode::Standard) update_standard_preview();
    else if (state_.mode == Mode::Scientific) update_scientific_preview();
    return {position - 1, true};
}

bool Controller::current_value(double& value) {
    if (!real_cache_.ok || !real_cache_.display.empty()) {
        state_.result = "Error: " +
            (real_cache_.error.empty()
                 ? std::string("expression is not a numeric value")
                 : real_cache_.error);
        set_status("CALCULATION ERROR", true);
        return false;
    }

    value = real_cache_.value;
    return true;
}

bool Controller::current_scientific_value(
    ScientificValue& value) {
    if (!scientific_cache_.ok ||
        !scientific_cache_.display.empty()) {
        state_.result = "Error: " +
            (scientific_cache_.error.empty()
                 ? std::string("expression is not a numeric value")
                 : scientific_cache_.error);
        set_status("CALCULATION ERROR", true);
        return false;
    }

    value = scientific_cache_.value;
    return true;
}

bool Controller::current_programmer_value(std::uint64_t& value) {
    if (!programmer_cache_.ok) {
        state_.result = "Error: " + programmer_cache_.error;
        set_status("PROGRAMMER ERROR", true);
        return false;
    }

    value = programmer_cache_.value;
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
    const ProgrammerResult& result = programmer_cache_;

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
    session_.record_history(
        state_.expression, real_cache_, HistoryKind::Standard);

    if (!real_cache_.ok) {
        state_.result = "Error: " + real_cache_.error;
        set_status("CALCULATION ERROR", true);
        return;
    }

    state_.result = format_display(real_cache_.value);
    set_status("READY");
}

// Equals is the side-effect boundary: Standard records the cached
// precedence-aware binary64 result, Programmer records the fixed-width
// result/context, and Scientific
// delegates to Session::evaluate_scientific so assignments/functions/history
// are committed exactly once.
void Controller::calculate() {
    if (state_.mode == Mode::Programmer) {
        calculate_programmer();
        return;
    }
    if (state_.mode == Mode::Standard) {
        calculate_standard();
        return;
    }

    const ScientificResult result =
        session_.evaluate_scientific(
            state_.expression, state_.angle_unit,
            scientific_digits_);
    refresh_evaluation_cache();
    if (!result.ok) {
        state_.result = "Error: " + result.error;
        set_status("CALCULATION ERROR", true);
        return;
    }

    state_.result = result.display.empty()
        ? format_scientific_result(result.value)
        : result.display;
    set_status(scientific_status_text());
}

void Controller::clear_calculation() {
    state_.expression.clear();
    real_cache_ = {};
    scientific_cache_ = {};
    programmer_cache_ = {};
    state_.result = "0";

    // Clear returns Scientific display notation to its ordinary fixed form.
    // Mode/angle/2nd/HYP remain user-selected state, but F-E is a transient
    // presentation toggle and must not survive a cleared calculation.
    if (state_.mode == Mode::Scientific) {
        state_.scientific_notation = false;
    }

    if (state_.mode == Mode::Programmer) {
        set_status(programmer_status_text());
    } else if (state_.mode == Mode::Scientific) {
        set_status(scientific_status_text());
    } else {
        set_status("READY");
    }
}

// Standard CE removes the operand containing the editing cursor, not the whole
// expression. Top-level binary operators delimit operands; signs that belong to
// exponents/unary terms are deliberately not treated as delimiters.
std::size_t Controller::clear_entry(std::size_t cursor) {
    if (state_.mode != Mode::Standard) return normalized_cursor(cursor);

    const std::size_t point = normalized_cursor(cursor);
    std::size_t segment_start = 0;
    std::size_t segment_end = state_.expression.size();
    int depth = 0;

    const auto sign_is_unary = [this](std::size_t position) {
        if (position == 0) return true;
        std::size_t previous = position;
        while (previous > 0 &&
               infiltratr_ascii_is_space(static_cast<unsigned char>(
                   state_.expression[previous - 1]))) {
            --previous;
        }
        if (previous == 0) return true;
        const char before = state_.expression[previous - 1];
        return before == 'e' || before == 'E' ||
               before == '+' || before == '-' ||
               before == '*' || before == '/' ||
               before == '^' || before == '(';
    };

    for (std::size_t i = 0; i < state_.expression.size(); ++i) {
        const char ch = state_.expression[i];
        if (ch == '(') {
            ++depth;
            continue;
        }
        if (ch == ')') {
            if (depth > 0) --depth;
            continue;
        }
        if (depth != 0 ||
            (ch != '+' && ch != '-' && ch != '*' &&
             ch != '/' && ch != '^')) {
            continue;
        }
        if ((ch == '+' || ch == '-') && sign_is_unary(i)) continue;

        if (point <= i && point >= segment_start) {
            segment_end = i;
            break;
        }
        segment_start = i + 1U;
    }

    while (segment_start < segment_end &&
           infiltratr_ascii_is_space(static_cast<unsigned char>(
               state_.expression[segment_start]))) {
        ++segment_start;
    }
    while (segment_end > segment_start &&
           infiltratr_ascii_is_space(static_cast<unsigned char>(
               state_.expression[segment_end - 1U]))) {
        --segment_end;
    }

    if (segment_start < segment_end) {
        state_.expression.erase(
            segment_start, segment_end - segment_start);
    }

    refresh_evaluation_cache();
    state_.result = "0";
    state_.fault = false;
    set_status("READY");
    return segment_start;
}

void Controller::unary_transform(Command command) {
    double value = 0.0;
    if (!current_value(value)) return;

    if (command == Command::Negate) {
        value = -value;
        state_.expression = calculator::serialize_value(value);
        if (state_.expression.empty()) {
            set_status("CALCULATION ERROR", true);
            return;
        }
        state_.result = format_display(value);
        refresh_evaluation_cache();
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

    state_.expression = calculator::serialize_value(transformed.value);
    if (state_.expression.empty()) {
        set_status("CALCULATION ERROR", true);
        return;
    }
    state_.result = format_display(transformed.value);
    refresh_evaluation_cache();
    set_status(state_.mode == Mode::Scientific
                   ? scientific_status_text()
                   : "READY");
}

void Controller::scientific_transform(Command command) {
    ScientificValue value;
    if (!current_scientific_value(value)) return;

    command = effective_scientific_command(command);
    const std::string operand =
        scientific_value_expression(value);

    std::string expression;
    switch (command) {
    case Command::Negate:
        expression = "-(" + operand + ")";
        break;
    case Command::Square:
        expression = "square(" + operand + ")";
        break;
    case Command::Cube:
        expression = "cube(" + operand + ")";
        break;
    case Command::SquareRoot:
        expression = "sqrt(" + operand + ")";
        break;
    case Command::CubeRoot:
        expression = "cbrt(" + operand + ")";
        break;
    case Command::Reciprocal:
        expression = "1/(" + operand + ")";
        break;
    case Command::Sin: expression = "sin(" + operand + ")"; break;
    case Command::Cos: expression = "cos(" + operand + ")"; break;
    case Command::Tan: expression = "tan(" + operand + ")"; break;
    case Command::Asin: expression = "asin(" + operand + ")"; break;
    case Command::Acos: expression = "acos(" + operand + ")"; break;
    case Command::Atan: expression = "atan(" + operand + ")"; break;
    case Command::Sinh: expression = "sinh(" + operand + ")"; break;
    case Command::Cosh: expression = "cosh(" + operand + ")"; break;
    case Command::Tanh: expression = "tanh(" + operand + ")"; break;
    case Command::Asinh: expression = "asinh(" + operand + ")"; break;
    case Command::Acosh: expression = "acosh(" + operand + ")"; break;
    case Command::Atanh: expression = "atanh(" + operand + ")"; break;
    case Command::Ln: expression = "ln(" + operand + ")"; break;
    case Command::Log10: expression = "log(" + operand + ")"; break;
    case Command::Exp: expression = "exp(" + operand + ")"; break;
    case Command::TwoPower: expression = "exp2(" + operand + ")"; break;
    case Command::TenPower: expression = "exp10(" + operand + ")"; break;
    case Command::Abs: expression = "abs(" + operand + ")"; break;
    case Command::Floor: expression = "floor(" + operand + ")"; break;
    case Command::Ceil: expression = "ceil(" + operand + ")"; break;
    default:
        return;
    }

    const ScientificResult transformed =
        calculator::evaluate_scientific(
            expression, session_.scientific_variables(),
            session_.functions(), state_.angle_unit,
            scientific_digits_);
    if (!transformed.ok) {
        set_status(
            transformed.error == "division by zero"
                ? "DIVISION BY ZERO"
                : "DOMAIN ERROR",
            true);
        return;
    }

    state_.expression =
        scientific_value_expression(transformed.value);
    state_.result =
        format_scientific_result(transformed.value);
    refresh_evaluation_cache();
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

    refresh_evaluation_cache();
    if (!state_.expression.empty()) calculate_programmer(false);
    else set_status(programmer_status_text());
}

// Standard preview tolerates a trailing binary operator so an in-progress
// mathematical expression still displays the valid prefix. The preview uses
// the same precedence-aware Standard evaluator as Equals and never records
// history or mutates Session state.
void Controller::update_standard_preview() {
    if (state_.mode != Mode::Standard || state_.expression.empty()) return;

    if (real_cache_.ok) {
        state_.result = format_display(real_cache_.value);
        set_status("READY");
        return;
    }

    std::string preview = state_.expression;
    while (!preview.empty() &&
           infiltratr_ascii_is_space(static_cast<unsigned char>(preview.back()))) {
        preview.pop_back();
    }
    if (preview.empty()) return;

    const char last = preview.back();
    if (last == '+' || last == '*' || last == '/' || last == '^') {
        preview.pop_back();
    } else if (last == '-' && preview.size() > 1U) {
        const char previous = preview[preview.size() - 2U];
        if (infiltratr_ascii_is_digit(static_cast<unsigned char>(previous)) ||
            previous == '.' || previous == '%') {
            preview.pop_back();
        }
    }

    if (preview.empty()) return;
    const Result result = evaluate_standard(preview);
    if (result.ok) {
        state_.result = format_display(result.value);
        set_status("READY");
    }
}

// Scientific live preview uses Session's side-effect-free path. Assignments
// and function definitions are therefore never committed before Equals.
void Controller::update_scientific_preview() {
    if (state_.mode != Mode::Scientific || state_.expression.empty()) return;
    if (!scientific_cache_.ok) return;

    if (!scientific_cache_.display.empty()) {
        state_.result = scientific_cache_.display;
    } else {
        state_.result = format_scientific_result(scientific_cache_.value);
    }
    set_status(scientific_status_text());
}

bool Controller::current_number_has_decimal() const {
    for (auto it = state_.expression.rbegin(); it != state_.expression.rend(); ++it) {
        const char ch = *it;
        if (ch == '.') return true;
        if (!infiltratr_ascii_is_digit(static_cast<unsigned char>(ch))) break;
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
    if (state_.mode == Mode::Programmer) return programmer_cache_.ok;
    if (state_.mode == Mode::Scientific) {
        return scientific_cache_.ok &&
               scientific_cache_.display.empty();
    }
    return real_cache_.ok && real_cache_.display.empty();
}

bool Controller::expression_can_calculate() const {
    if (state_.expression.empty()) return false;
    if (state_.mode == Mode::Programmer) return programmer_cache_.ok;
    if (state_.mode == Mode::Scientific) return scientific_cache_.ok;
    return real_cache_.ok;
}

// Enablement is a product rule, not a widget convenience. Keeping it here
// ensures GTK, Win32 and SwiftUI expose the same legal command set for the same
// Controller state.
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
        return !state_.expression.empty();
    default:
        return true;
    }
}

// Dispatch is the authoritative transition table for calculator commands.
 // Platform shells supply intent plus cursor position and render the resulting
 // ViewState; they do not duplicate mode-specific state transitions.
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
            ScientificValue value;
            if (current_scientific_value(value)) {
                state_.result =
                    format_scientific_result(value);
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
    case Command::ClearEntry: {
        const std::size_t previous_size = state_.expression.size();
        const std::size_t next_cursor = clear_entry(cursor);
        return {next_cursor,
                state_.expression.size() != previous_size};
    }
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
        if (state_.mode == Mode::Scientific) {
            return insert(
                scientific_value_expression(
                    session_.memory_recall_scientific()),
                cursor);
        }
        if (!session_.memory_binary64_available()) {
            set_status("MEMORY RANGE ERROR", true);
            return {normalized_cursor(cursor), false};
        }
        return insert(
            calculator::serialize_value(session_.memory_recall()),
            cursor);
    case Command::MemoryStore:
    case Command::MemoryAdd:
    case Command::MemorySubtract: {
        if (state_.mode == Mode::Scientific) {
            ScientificValue value;
            if (current_scientific_value(value)) {
                if (effective == Command::MemoryStore) {
                    session_.memory_store_scientific(value);
                } else if (effective == Command::MemoryAdd) {
                    session_.memory_add_scientific(
                        value, scientific_digits_);
                } else {
                    session_.memory_subtract_scientific(
                        value, scientific_digits_);
                }
                set_status(effective == Command::MemoryStore
                               ? "MEMORY STORED"
                               : "MEMORY UPDATED");
            }
        } else {
            double value = 0.0;
            if (current_value(value)) {
                bool stored = false;
                if (effective == Command::MemoryStore) {
                    stored = session_.memory_store(value);
                } else if (effective == Command::MemoryAdd) {
                    stored = session_.memory_add(value);
                } else {
                    stored = session_.memory_subtract(value);
                }
                if (stored) {
                    set_status(effective == Command::MemoryStore
                                   ? "MEMORY STORED"
                                   : "MEMORY UPDATED");
                } else {
                    set_status("MEMORY RANGE ERROR", true);
                }
            }
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
            refresh_evaluation_cache();
            update_standard_preview();
            return {state_.expression.size(), true};
        }
        return insert(text, cursor);
    }
    return {normalized_cursor(cursor), false};
}

} // namespace calculator::ui
