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
    return name == "pi" || name == "e";
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
        record_history(input, result);
        return result;
    }

    Result result = calculator::evaluate(assignment ? expression : input, variables_);

    if (result.ok && assignment) {
        variables_[*assignment] = result.value;
    }

    record_history(input, result);
    return result;
}

void Session::record_history(std::string input, Result result) {
    history_.push_back({std::move(input), std::move(result)});
    while (history_.size() > history_limit_) history_.pop_front();
}

void Session::memory_clear() {
    memory_ = 0.0;
    memory_set_ = false;
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
void Session::clear_history() noexcept { history_.clear(); }

} // namespace calculator
