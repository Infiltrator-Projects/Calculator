/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "session.hpp"

#include <infiltratr/core.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace calculator {
namespace {

std::string trim_copy(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           infiltratr_ascii_is_space(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           infiltratr_ascii_is_space(static_cast<unsigned char>(text[end - 1U]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

bool valid_identifier(const std::string& name) {
    if (name.empty()) return false;
    if (!infiltratr_ascii_is_alpha(static_cast<unsigned char>(name.front())) && name.front() != '_') return false;
    for (char c : name) {
        if (!infiltratr_ascii_is_alnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

bool reserved_identifier(const std::string& name) {
    if (name == "_" || name == "rand" || name == "i" ||
        name == "conj" || name == "real" || name == "imag" ||
        is_builtin_function_name(name)) return true;
    for (const auto& constant : constant_catalog()) {
        if (constant.name == name || constant.alias == name) return true;
    }
    return false;
}

std::optional<std::string> assignment_name(const std::string& input, std::string& expression) {
    std::size_t left = 0;
    while (left < input.size() && infiltratr_ascii_is_space(static_cast<unsigned char>(input[left]))) ++left;
    std::size_t name_end = left;
    while (name_end < input.size() &&
           (infiltratr_ascii_is_alnum(static_cast<unsigned char>(input[name_end])) || input[name_end] == '_')) ++name_end;
    if (name_end == left || name_end >= input.size() || input[name_end] != '=') return std::nullopt;

    std::string name = input.substr(left, name_end - left);
    if (!valid_identifier(name)) return std::nullopt;
    expression = input.substr(name_end + 1);
    return name;
}


struct ParsedFunctionDefinition {
    std::string name;
    FunctionDefinition definition;
};

std::optional<ParsedFunctionDefinition> function_definition(
    const std::string& input, std::string& error) {
    const std::size_t equals = input.find('=');
    if (equals == std::string::npos) return std::nullopt;

    const std::string left = trim_copy(
        std::string_view(input).substr(0, equals));
    const std::size_t open = left.find('(');
    if (open == std::string::npos) return std::nullopt;
    const std::size_t close = left.rfind(')');
    if (close == std::string::npos || close != left.size() - 1U ||
        open == 0U || open > close) {
        error = "invalid function definition";
        return std::nullopt;
    }

    ParsedFunctionDefinition parsed;
    parsed.name = trim_copy(std::string_view(left).substr(0, open));
    if (!valid_identifier(parsed.name) || reserved_identifier(parsed.name)) {
        error = "invalid or reserved function name";
        return std::nullopt;
    }

    const std::string parameters_text =
        left.substr(open + 1U, close - open - 1U);
    std::size_t cursor = 0;
    while (cursor < parameters_text.size()) {
        const std::size_t separator = parameters_text.find(';', cursor);
        const std::size_t end =
            separator == std::string::npos
                ? parameters_text.size() : separator;
        const std::string parameter = trim_copy(
            std::string_view(parameters_text).substr(cursor, end - cursor));
        if (!valid_identifier(parameter) || reserved_identifier(parameter)) {
            error = "invalid or reserved function parameter";
            return std::nullopt;
        }
        if (std::find(
                parsed.definition.parameters.begin(),
                parsed.definition.parameters.end(),
                parameter) != parsed.definition.parameters.end()) {
            error = "duplicate function parameter";
            return std::nullopt;
        }
        parsed.definition.parameters.push_back(parameter);
        if (separator == std::string::npos) break;
        cursor = separator + 1U;
        if (cursor == parameters_text.size()) {
            error = "empty function parameter";
            return std::nullopt;
        }
    }

    std::string right = input.substr(equals + 1U);
    const std::size_t description = right.find('@');
    if (description != std::string::npos) {
        parsed.definition.description = trim_copy(
            std::string_view(right).substr(description + 1U));
        right.resize(description);
    }
    parsed.definition.expression = trim_copy(right);
    if (parsed.definition.expression.empty()) {
        error = "empty function expression";
        return std::nullopt;
    }

    return parsed;
}

} // namespace

Session::Session(std::size_t history_limit) : history_limit_(history_limit) {}

// Preview and evaluate intentionally share grammar/context but differ in side
// effects. Preview is used for command enablement/live display and must never
// define functions, assign variables, update "_", or append history.
Result Session::preview(
    const std::string& input, AngleUnit angle_unit) const {
    std::string definition_error;
    const auto definition = function_definition(input, definition_error);
    if (!definition_error.empty()) {
        return {false, 0.0, definition_error};
    }
    if (definition) {
        return {
            true, 0.0, {},
            "Function defined: " + definition->name};
    }

    std::string expression;
    const auto assignment = assignment_name(input, expression);
    if (assignment && reserved_identifier(*assignment)) {
        return {false, 0.0, "cannot assign reserved constant"};
    }

    return calculator::evaluate(
        assignment ? expression : input,
        variables_, functions_, angle_unit);
}

Result Session::evaluate(
    const std::string& input, AngleUnit angle_unit) {
    std::string definition_error;
    const auto definition = function_definition(input, definition_error);
    if (!definition_error.empty()) {
        Result result{false, 0.0, definition_error};
        record_history(input, result, HistoryKind::Scientific);
        return result;
    }
    if (definition) {
        functions_[definition->name] = definition->definition;
        Result result{
            true, 0.0, {},
            "Function defined: " + definition->name};
        record_history(input, result, HistoryKind::Scientific);
        return result;
    }

    std::string expression;
    const auto assignment = assignment_name(input, expression);

    if (assignment && reserved_identifier(*assignment)) {
        Result result{false, 0.0, "cannot assign reserved constant"};
        record_history(input, result, HistoryKind::Scientific);
        return result;
    }

    Result result = calculator::evaluate(
        assignment ? expression : input, variables_, functions_, angle_unit);

    if (result.ok) {
        const ScientificValue precise =
            scientific_value_from_double(result.value);
        if (assignment) {
            variables_[*assignment] = result.value;
            scientific_variables_[*assignment] = precise;
        }
        variables_["_"] = result.value;
        scientific_variables_["_"] = precise;
    }

    record_history(input, result, HistoryKind::Scientific);
    return result;
}


// The Scientific pair mirrors the binary64 pair above, but assignments and
// memory/history preserve ScientificValue text so precision is not silently
// reduced at the Session boundary.
ScientificResult Session::preview_scientific(
    const std::string& input, AngleUnit angle_unit,
    unsigned decimal_digits) const {
    std::string definition_error;
    const auto definition =
        function_definition(input, definition_error);
    if (!definition_error.empty()) {
        return {false, {}, definition_error, {}};
    }
    if (definition) {
        return {
            true, {}, {},
            "Function defined: " + definition->name};
    }

    std::string expression;
    const auto assignment = assignment_name(input, expression);
    if (assignment && reserved_identifier(*assignment)) {
        return {
            false, {}, "cannot assign reserved constant", {}};
    }

    return calculator::evaluate_scientific(
        assignment ? expression : input,
        scientific_variables_, functions_,
        angle_unit, decimal_digits);
}

ScientificResult Session::evaluate_scientific(
    const std::string& input, AngleUnit angle_unit,
    unsigned decimal_digits) {
    std::string definition_error;
    const auto definition =
        function_definition(input, definition_error);
    if (!definition_error.empty()) {
        ScientificResult result{
            false, {}, definition_error, {}};
        record_history_text(
            input, "Error: " + result.error, false,
            HistoryKind::Scientific);
        return result;
    }
    if (definition) {
        functions_[definition->name] = definition->definition;
        ScientificResult result{
            true, {}, {},
            "Function defined: " + definition->name};
        record_history_text(
            input, result.display, true,
            HistoryKind::Scientific);
        return result;
    }

    std::string expression;
    const auto assignment = assignment_name(input, expression);
    if (assignment && reserved_identifier(*assignment)) {
        ScientificResult result{
            false, {}, "cannot assign reserved constant", {}};
        record_history_text(
            input, "Error: " + result.error, false,
            HistoryKind::Scientific);
        return result;
    }

    ScientificResult result = calculator::evaluate_scientific(
        assignment ? expression : input,
        scientific_variables_, functions_,
        angle_unit, decimal_digits);

    if (result.ok) {
        if (assignment) {
            scientific_variables_[*assignment] = result.value;
            double approximate = 0.0;
            if (scientific_value_to_double(
                    result.value, approximate)) {
                variables_[*assignment] = approximate;
            } else {
                variables_.erase(*assignment);
            }
        }

        scientific_variables_["_"] = result.value;
        double approximate = 0.0;
        if (scientific_value_to_double(
                result.value, approximate)) {
            variables_["_"] = approximate;
        } else {
            variables_.erase("_");
        }
    }

    const std::string output = result.ok
        ? (result.display.empty()
            ? format_scientific_value(
                result.value, decimal_digits)
            : result.display)
        : ("Error: " + result.error);
    record_history_text(
        input, output, result.ok,
        HistoryKind::Scientific);
    return result;
}

void Session::record_history(std::string input, const Result& result,
                             HistoryKind kind, HistoryContext context) {
    const std::string output = result.ok
        ? (result.display.empty()
            ? calculator::format_value(result.value)
            : result.display)
        : ("Error: " + result.error);
    record_history_text(
        std::move(input), output, result.ok, kind, context);
}

void Session::record_history_text(std::string input, std::string output,
                                  bool ok, HistoryKind kind,
                                  HistoryContext context) {
    history_.push_back(
        {kind, context, std::move(input), std::move(output), ok});
    ++history_revision_;
    if (history_limit_ != 0U) {
        while (history_.size() > history_limit_) history_.pop_front();
    }
}

void Session::memory_clear() {
    memory_ = 0.0;
    scientific_memory_ = {};
    memory_set_ = false;
    memory_binary64_valid_ = false;
}

bool Session::memory_store(double value) {
    if (!std::isfinite(value)) return false;

    memory_ = value;
    scientific_memory_ = scientific_value_from_double(value);
    memory_set_ = true;
    memory_binary64_valid_ = true;
    return true;
}

bool Session::memory_add(double value) {
    if (!std::isfinite(value) ||
        (memory_set_ && !memory_binary64_valid_)) {
        return false;
    }

    const double left = memory_set_ ? memory_ : 0.0;
    const double result = left + value;
    if (!std::isfinite(result)) return false;

    memory_ = result;
    scientific_memory_ = scientific_value_from_double(result);
    memory_set_ = true;
    memory_binary64_valid_ = true;
    return true;
}

bool Session::memory_subtract(double value) {
    if (!std::isfinite(value) ||
        (memory_set_ && !memory_binary64_valid_)) {
        return false;
    }

    const double left = memory_set_ ? memory_ : 0.0;
    const double result = left - value;
    if (!std::isfinite(result)) return false;

    memory_ = result;
    scientific_memory_ = scientific_value_from_double(result);
    memory_set_ = true;
    memory_binary64_valid_ = true;
    return true;
}

double Session::memory_recall() const noexcept { return memory_; }

bool Session::memory_binary64_available() const noexcept {
    return memory_set_ && memory_binary64_valid_;
}

void Session::memory_store_scientific(ScientificValue value) {
    scientific_memory_ = std::move(value);
    double approximate = 0.0;
    memory_binary64_valid_ = scientific_value_to_double(
        scientific_memory_, approximate);
    memory_ = memory_binary64_valid_ ? approximate : 0.0;
    memory_set_ = true;
}

void Session::memory_add_scientific(
    const ScientificValue& value, unsigned decimal_digits) {
    const ScientificValue left =
        memory_set_ ? scientific_memory_ : ScientificValue{};
    const auto result = calculator::evaluate_scientific(
        "(" + scientific_value_expression(left) + ")+(" +
            scientific_value_expression(value) + ")",
        {}, {}, AngleUnit::Radians, decimal_digits);
    if (result.ok) memory_store_scientific(result.value);
}

void Session::memory_subtract_scientific(
    const ScientificValue& value, unsigned decimal_digits) {
    const ScientificValue left =
        memory_set_ ? scientific_memory_ : ScientificValue{};
    const auto result = calculator::evaluate_scientific(
        "(" + scientific_value_expression(left) + ")-(" +
            scientific_value_expression(value) + ")",
        {}, {}, AngleUnit::Radians, decimal_digits);
    if (result.ok) memory_store_scientific(result.value);
}

ScientificValue Session::memory_recall_scientific() const {
    return scientific_memory_;
}
bool Session::memory_empty() const noexcept { return !memory_set_; }

void Session::set_variable(std::string name, double value) {
    if (valid_identifier(name) && !reserved_identifier(name)) {
        variables_[name] = value;
        scientific_variables_[std::move(name)] =
            scientific_value_from_double(value);
    }
}

std::optional<double> Session::variable(const std::string& name) const {
    const auto it = variables_.find(name);
    if (it == variables_.end()) return std::nullopt;
    return it->second;
}

const Variables& Session::variables() const noexcept { return variables_; }

const ScientificVariables&
Session::scientific_variables() const noexcept {
    return scientific_variables_;
}

std::string Session::variables_text() const {
    std::vector<std::string> names;
    names.reserve(scientific_variables_.size());
    for (const auto& item : scientific_variables_) {
        if (item.first != "_") names.push_back(item.first);
    }
    std::sort(names.begin(), names.end());

    std::ostringstream out;
    for (const auto& name : names) {
        const auto it = scientific_variables_.find(name);
        if (it == scientific_variables_.end()) continue;
        out << name << '='
            << scientific_value_expression(it->second)
            << '\n';
    }
    return out.str();
}

// Persistence loading is transactional. Parse and validate the entire file
// into temporary maps first; only a fully valid document replaces live state.
 // The volatile "_" previous-result binding is deliberately preserved.
bool Session::load_variables_text(std::string_view text) {
    ScientificVariables loaded_scientific;
    Variables loaded_legacy;

    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t newline = text.find('\n', begin);
        const std::size_t end =
            newline == std::string_view::npos
                ? text.size() : newline;
        const std::string line =
            trim_copy(text.substr(begin, end - begin));
        if (!line.empty()) {
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos) return false;
            const std::string name = trim_copy(
                std::string_view(line).substr(0, equals));
            const std::string value_text = trim_copy(
                std::string_view(line).substr(equals + 1U));
            if (!valid_identifier(name) ||
                reserved_identifier(name) ||
                name == "_" || value_text.empty()) {
                return false;
            }

            const ScientificResult parsed =
                calculator::evaluate_scientific(
                    value_text, loaded_scientific, functions_,
                    AngleUnit::Radians, kScientificDefaultDigits);
            if (!parsed.ok || !parsed.display.empty()) return false;

            loaded_scientific[name] = parsed.value;
            double approximate = 0.0;
            if (scientific_value_to_double(
                    parsed.value, approximate)) {
                loaded_legacy[name] = approximate;
            }
        }
        if (newline == std::string_view::npos) break;
        begin = newline + 1U;
    }

    const auto precise_last = scientific_variables_.find("_");
    const bool has_precise_last =
        precise_last != scientific_variables_.end();
    ScientificValue precise_last_value{};
    if (has_precise_last) {
        precise_last_value = precise_last->second;
    }

    const auto legacy_last = variables_.find("_");
    const bool has_legacy_last = legacy_last != variables_.end();
    const double legacy_last_value =
        has_legacy_last ? legacy_last->second : 0.0;

    scientific_variables_ = std::move(loaded_scientific);
    variables_ = std::move(loaded_legacy);

    if (has_precise_last) {
        scientific_variables_["_"] =
            std::move(precise_last_value);
    }
    if (has_legacy_last) {
        variables_["_"] = legacy_last_value;
    }
    return true;
}

std::optional<FunctionDefinition> Session::function(
    const std::string& name) const {
    const auto it = functions_.find(name);
    if (it == functions_.end()) return std::nullopt;
    return it->second;
}
const Functions& Session::functions() const noexcept { return functions_; }
bool Session::remove_function(const std::string& name) {
    return functions_.erase(name) != 0U;
}

std::string Session::function_definitions_text() const {
    std::vector<std::string> names;
    names.reserve(functions_.size());
    for (const auto& item : functions_) names.push_back(item.first);
    std::sort(names.begin(), names.end());

    auto single_line = [](std::string value) {
        for (char& ch : value) {
            if (ch == '\n' || ch == '\r') ch = ' ';
        }
        return value;
    };

    std::ostringstream out;
    for (const auto& name : names) {
        const auto it = functions_.find(name);
        if (it == functions_.end()) continue;
        const auto& definition = it->second;
        out << name << '(';
        for (std::size_t index = 0; index < definition.parameters.size(); ++index) {
            if (index != 0U) out << ';';
            out << definition.parameters[index];
        }
        out << ")=" << single_line(definition.expression);
        if (!definition.description.empty()) {
            out << '@' << single_line(definition.description);
        }
        out << '\n';
    }
    return out.str();
}

// Function persistence follows the same all-or-nothing rule as variables so a
// damaged file cannot leave a partially loaded function namespace.
bool Session::load_function_definitions_text(std::string_view text) {
    Functions loaded;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t newline = text.find('\n', begin);
        const std::size_t end =
            newline == std::string_view::npos ? text.size() : newline;
        const std::string line = trim_copy(text.substr(begin, end - begin));
        if (!line.empty()) {
            std::string error;
            const auto parsed = function_definition(line, error);
            if (!parsed || !error.empty()) return false;
            loaded[parsed->name] = parsed->definition;
        }
        if (newline == std::string_view::npos) break;
        begin = newline + 1U;
    }
    functions_ = std::move(loaded);
    return true;
}

const std::deque<HistoryEntry>& Session::history() const noexcept { return history_; }
std::size_t Session::history_count() const noexcept { return history_.size(); }

std::optional<HistoryEntry> Session::history_from_newest(
    std::size_t index) const {
    if (index >= history_.size()) return std::nullopt;
    return history_[history_.size() - 1U - index];
}

std::string Session::history_text(std::size_t limit,
                                  std::string_view newline) const {
    if (history_.empty()) return "No calculations yet.";

    std::string text;
    std::size_t shown = 0;
    for (auto it = history_.rbegin();
         it != history_.rend() && shown < limit;
         ++it, ++shown) {
        text += it->input;
        text.append(newline.data(), newline.size());
        text += "  = ";
        text += it->output;
        text.append(newline.data(), newline.size());
        text.append(newline.data(), newline.size());
    }
    return text;
}

void Session::clear_history() noexcept {
    if (history_.empty()) return;
    history_.clear();
    ++history_revision_;
}

std::uint64_t Session::history_revision() const noexcept {
    return history_revision_;
}

} // namespace calculator
