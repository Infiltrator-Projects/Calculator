/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "session.hpp"

#include <cctype>
#include <utility>

namespace calculator {
namespace {

bool valid_identifier(const std::string& name) {
    if (name.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(name.front())) && name.front() != '_') return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

bool reserved_identifier(const std::string& name) {
    if (name == "_" || name == "rand") return true;
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

} // namespace

Session::Session(std::size_t history_limit) : history_limit_(history_limit) {
    if (history_limit_ == 0) history_limit_ = 1;
}

Result Session::evaluate(const std::string& input) {
    std::string expression;
    const auto assignment = assignment_name(input, expression);

    if (assignment && reserved_identifier(*assignment)) {
        Result result{false, 0.0, "cannot assign reserved constant"};
        record_history(input, result, HistoryKind::Scientific);
        return result;
    }

    Result result = calculator::evaluate(assignment ? expression : input, variables_);

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
        ? calculator::format_value(result.value)
        : ("Error: " + result.error);
    record_history_text(
        std::move(input), output, result.ok, kind, context);
}

void Session::record_history_text(std::string input, std::string output,
                                  bool ok, HistoryKind kind,
                                  HistoryContext context) {
    history_.push_back(
        {kind, context, std::move(input), std::move(output), ok});
    while (history_.size() > history_limit_) history_.pop_front();
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
