/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "session.hpp"

#include <infiltratr/token.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace calculator {
namespace {

std::string trim_copy(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1U]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

bool valid_identifier(const std::string& name) {
    if (name.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(name.front())) && name.front() != '_') return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

bool reserved_identifier(const std::string& name) {
    if (name == "_" || name == "rand" ||
        is_builtin_function_name(name)) return true;
    for (const auto& constant : constant_catalog()) {
        if (constant.name == name || constant.alias == name) return true;
    }
    return false;
}

std::optional<std::string> assignment_name(const std::string& input, std::string& expression) {
    std::size_t left = 0;
    while (left < input.size() && std::isspace(static_cast<unsigned char>(input[left]))) ++left;
    std::size_t name_end = left;
    while (name_end < input.size() &&
           (std::isalnum(static_cast<unsigned char>(input[name_end])) || input[name_end] == '_')) ++name_end;
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
        if (assignment) variables_[*assignment] = result.value;
        variables_["_"] = result.value;
    }

    record_history(input, result, HistoryKind::Scientific);
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
    if (history_limit_ != 0U) {
        while (history_.size() > history_limit_) history_.pop_front();
    }
}

void Session::memory_clear() {
    memory_ = 0.0;
    memory_set_ = false;
}
void Session::memory_store(double value) {
    memory_ = value;
    memory_set_ = true;
}
void Session::memory_add(double value) {
    memory_ += value;
    memory_set_ = true;
}
void Session::memory_subtract(double value) {
    memory_ -= value;
    memory_set_ = true;
}
double Session::memory_recall() const noexcept { return memory_; }
bool Session::memory_empty() const noexcept { return !memory_set_; }

void Session::set_variable(std::string name, double value) {
    if (valid_identifier(name) && !reserved_identifier(name)) {
        variables_[std::move(name)] = value;
    }
}

std::optional<double> Session::variable(const std::string& name) const {
    const auto it = variables_.find(name);
    if (it == variables_.end()) return std::nullopt;
    return it->second;
}

const Variables& Session::variables() const noexcept { return variables_; }

std::string Session::variables_text() const {
    std::vector<std::string> names;
    names.reserve(variables_.size());
    for (const auto& item : variables_) {
        if (item.first != "_") names.push_back(item.first);
    }
    std::sort(names.begin(), names.end());

    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (const auto& name : names) {
        const auto it = variables_.find(name);
        if (it == variables_.end() || !std::isfinite(it->second)) continue;
        out << name << '=' << it->second << '\n';
    }
    return out.str();
}

bool Session::load_variables_text(std::string_view text) {
    Variables loaded;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t newline = text.find('\n', begin);
        const std::size_t end =
            newline == std::string_view::npos ? text.size() : newline;
        const std::string line = trim_copy(text.substr(begin, end - begin));
        if (!line.empty()) {
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos) return false;
            const std::string name = trim_copy(
                std::string_view(line).substr(0, equals));
            const std::string value_text = trim_copy(
                std::string_view(line).substr(equals + 1U));
            if (!valid_identifier(name) || reserved_identifier(name) ||
                name == "_" || value_text.empty()) {
                return false;
            }

            const char* cursor = value_text.data();
            double value = 0.0;
            if (!infiltratr_parse_double_token(&cursor, true, &value) ||
                cursor != value_text.data() + value_text.size() ||
                !std::isfinite(value)) {
                return false;
            }
            loaded[name] = value;
        }
        if (newline == std::string_view::npos) break;
        begin = newline + 1U;
    }

    const auto last = variables_.find("_");
    std::optional<double> last_value;
    if (last != variables_.end()) last_value = last->second;
    variables_ = std::move(loaded);
    if (last_value) variables_["_"] = *last_value;
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

void Session::clear_history() noexcept { history_.clear(); }

} // namespace calculator
