/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator_ui_controller.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <system_error>
#include <utility>

namespace calculator::ui {

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
    clear_standard_repeat();
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
        state_.angle_unit = entry->context.scientific_angle_unit;
        scientific_digits_ = std::clamp(
            entry->context.scientific_digits,
            kScientificMinDigits, kScientificMaxDigits);
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

bool Controller::delete_history(std::size_t index_from_newest) {
    return session_.erase_history_from_newest(index_from_newest);
}

void Controller::clear_history() noexcept {
    session_.clear_history();
}

bool Controller::recall_memory(std::size_t index_from_newest) {
    clear_standard_repeat();
    const auto entry = session_.memory_entry(index_from_newest);
    if (!entry || state_.mode == Mode::Programmer) return false;

    if (state_.mode == Mode::Scientific) {
        state_.expression =
            calculator::scientific_value_expression(entry->scientific);
    } else {
        if (!entry->binary64_valid) {
            set_status("MEMORY RANGE ERROR", true);
            return false;
        }
        state_.expression = calculator::serialize_value(entry->binary64);
    }

    state_.fault = false;
    refresh_evaluation_cache();
    if (state_.mode == Mode::Scientific) {
        update_scientific_preview();
        set_status(scientific_status_text());
    } else {
        update_standard_preview();
        set_status("MEMORY RECALL");
    }
    return true;
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
        << "history-limit=" << session_.history_limit() << '\n'
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
    std::size_t history_limit = session_.history_limit();

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
            programmer_signed > 1U ||
            !read_line(line) ||
            !parse_size(line, "history-limit=", history_limit) ||
            history_limit > kMaxHistoryEntries) {
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
    if (state_v2) session_.set_history_limit(history_limit);

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

} // namespace calculator::ui
