/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator_ui_controller.hpp"

#include <infiltratr/core.h>
#include <infiltratr/utf8.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <system_error>
#include <utility>

namespace calculator::ui {

bool Controller::set_expression(std::string expression) {
    // Native shells synchronise their text before every command. Identical
    // text is not an edit: preserve the committed result, including rand and
    // expressions whose assignment or '_' binding changed at Equals.
    if (expression == state_.expression) return true;
    if (expression != state_.expression) clear_standard_repeat();
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

    if (!infiltratr_utf8_validate(expression.data(), expression.size()) ||
        expression.find('\0') != std::string::npos) {
        state_.expression.clear();
        real_cache_ = {};
        scientific_cache_ = {};
        programmer_cache_ = {};
        clear_standard_repeat();
        state_.result = "Error: expression must be valid UTF-8 text";
        set_status("INPUT ERROR", true);
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
    if (mode == state_.mode) return;
    if (mode != state_.mode) clear_standard_repeat();
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
    if (unit == state_.angle_unit) {
        if (state_.mode == Mode::Scientific) set_status(scientific_status_text());
        return;
    }
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


void Controller::set_status(std::string text, bool fault) {
    state_.status = std::move(text);
    state_.fault = fault;
}

std::size_t Controller::normalized_cursor(std::size_t cursor) const noexcept {
    std::size_t point = cursor == kEnd ? state_.expression.size()
                                     : std::min(cursor, state_.expression.size());
    // Public cursors are UTF-8 byte offsets; edits snap to a scalar boundary.
    while (point > 0U && point < state_.expression.size() &&
           (static_cast<unsigned char>(state_.expression[point]) & 0xc0U) == 0x80U)
        --point;
    return point;
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
    clear_standard_repeat();
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
    clear_standard_repeat();
    const std::size_t position = normalized_cursor(cursor);
    if (position == 0) return {0, false};

    std::size_t begin = position - 1U;
    while (begin > 0U &&
           (static_cast<unsigned char>(state_.expression[begin]) & 0xc0U) == 0x80U)
        --begin;
    state_.expression.erase(begin, position - begin);
    state_.fault = false;
    refresh_evaluation_cache();
    if (state_.mode == Mode::Standard) update_standard_preview();
    else if (state_.mode == Mode::Scientific) update_scientific_preview();
    return {begin, true};
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

    state_.result = group_programmer_digits(
        format_programmer(
            result.value, state_.programmer_base,
            state_.programmer_width, state_.programmer_signed),
        state_.programmer_base);
    if (record_history) {
        session_.record_history_text(
            state_.expression, state_.result, true,
            HistoryKind::Programmer, context);
    }
    set_status(programmer_status_text());
}

void Controller::clear_standard_repeat() noexcept {
    standard_repeat_valid_ = false;
    standard_repeat_operator_ = 0;
    standard_repeat_rhs_.clear();
    standard_repeat_expression_.clear();
}

void Controller::capture_standard_repeat() {
    clear_standard_repeat();
    if (state_.mode != Mode::Standard || state_.expression.empty()) return;

    const auto sign_is_unary = [this](std::size_t position) {
        if (position == 0U) return true;
        std::size_t previous = position;
        while (previous > 0U &&
               infiltratr_ascii_is_space(static_cast<unsigned char>(
                   state_.expression[previous - 1U]))) {
            --previous;
        }
        if (previous == 0U) return true;
        const char before = state_.expression[previous - 1U];
        return before == 'e' || before == 'E' ||
               before == '+' || before == '-' ||
               before == '*' || before == '/' ||
               before == '^' || before == '(';
    };

    std::size_t best = std::string::npos;
    int best_precedence = 99;
    int depth = 0;
    for (std::size_t i = 0U; i < state_.expression.size(); ++i) {
        const char ch = state_.expression[i];
        if (ch == '(') {
            ++depth;
            continue;
        }
        if (ch == ')') {
            if (depth > 0) --depth;
            continue;
        }
        if (depth != 0) continue;

        int precedence = 99;
        if (ch == '+' || ch == '-') precedence = 1;
        else if (ch == '*' || ch == '/') precedence = 2;
        else if (ch == '^') precedence = 3;
        else continue;

        if ((ch == '+' || ch == '-') && sign_is_unary(i)) continue;

        // For left-associative operators the rightmost operator at the
        // weakest precedence is the parse-tree root. Exponentiation is
        // right-associative, so retain its first root candidate.
        if (precedence < best_precedence ||
            (precedence == best_precedence && ch != '^')) {
            best = i;
            best_precedence = precedence;
        }
    }

    if (best == std::string::npos) return;
    std::string rhs = state_.expression.substr(best + 1U);
    const auto first = rhs.find_first_not_of(" \\t\\r\\n");
    if (first == std::string::npos) return;
    const auto last = rhs.find_last_not_of(" \\t\\r\\n");
    rhs = rhs.substr(first, last - first + 1U);

    standard_repeat_valid_ = true;
    standard_repeat_operator_ = state_.expression[best];
    standard_repeat_rhs_ = std::move(rhs);
    standard_repeat_expression_ = state_.expression;
}

bool Controller::apply_standard_repeat() {
    if (!standard_repeat_valid_ ||
        state_.mode != Mode::Standard ||
        state_.expression != standard_repeat_expression_ ||
        !real_cache_.ok || !real_cache_.display.empty()) {
        return false;
    }

    const std::string left = calculator::serialize_value(real_cache_.value);
    if (left.empty()) return false;
    std::string repeated = left;
    repeated.push_back(standard_repeat_operator_);
    repeated.push_back('(');
    repeated += standard_repeat_rhs_;
    repeated.push_back(')');
    if (repeated.size() > kMaxExpressionBytes) return false;

    state_.expression = std::move(repeated);
    refresh_evaluation_cache();
    return real_cache_.ok && real_cache_.display.empty();
}

void Controller::calculate_standard() {
    const bool repeated = apply_standard_repeat();

    session_.record_history(
        state_.expression, real_cache_, HistoryKind::Standard);

    if (!real_cache_.ok) {
        state_.result = "Error: " + real_cache_.error;
        clear_standard_repeat();
        set_status("CALCULATION ERROR", true);
        return;
    }

    state_.result = format_display(real_cache_.value);
    if (repeated) {
        standard_repeat_expression_ = state_.expression;
    } else {
        capture_standard_repeat();
    }
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
    // Session has just committed variables and '_'. Re-evaluating the same
    // text now would describe the next calculation rather than this result.
    scientific_cache_ = result;
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
    clear_standard_repeat();
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
    clear_standard_repeat();
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
    state_.result = real_cache_.ok ? format_display(real_cache_.value) : "0";
    state_.fault = false;
    set_status("READY");
    return segment_start;
}

void Controller::unary_transform(Command command) {
    clear_standard_repeat();
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
        if (command == Command::ByteSwap) {
            const bool changed = swap_programmer_endianness();
            return {state_.expression.size(), changed};
        }

        const std::string_view text = insertion_text(command);
        if (!text.empty()) return insert(text, cursor);
        return {normalized_cursor(cursor), false};
    }

    if (state_.mode == Mode::Scientific) {
        switch (command) {
        case Command::CycleAngleUnit:
            if (state_.angle_unit == AngleUnit::Degrees) {
                set_angle_unit(AngleUnit::Radians);
            } else if (state_.angle_unit == AngleUnit::Radians) {
                set_angle_unit(AngleUnit::Gradians);
            } else {
                set_angle_unit(AngleUnit::Degrees);
            }
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
                bool stored = true;
                if (effective == Command::MemoryStore) {
                    session_.memory_store_scientific(value);
                } else if (effective == Command::MemoryAdd) {
                    stored = session_.memory_add_scientific(
                        value, scientific_digits_);
                } else {
                    stored = session_.memory_subtract_scientific(
                        value, scientific_digits_);
                }
                if (stored) {
                    set_status(effective == Command::MemoryStore
                                   ? "MEMORY STORED" : "MEMORY UPDATED");
                } else {
                    set_status("MEMORY RANGE ERROR", true);
                }
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
            clear_standard_repeat();
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
