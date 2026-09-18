/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "calculator.hpp"

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace infiltrator::calc {

struct HistoryEntry {
    std::string input;
    Result result;
};

class Session {
public:
    explicit Session(std::size_t history_limit = 100);

    Result evaluate(const std::string& input);

    void memory_clear();
    void memory_add(double value);
    void memory_subtract(double value);
    double memory_recall() const noexcept;
    bool memory_empty() const noexcept;

    void record_history(std::string input, Result result);

    void set_variable(std::string name, double value);
    std::optional<double> variable(const std::string& name) const;
    const Variables& variables() const noexcept;

    const std::deque<HistoryEntry>& history() const noexcept;
    void clear_history() noexcept;

private:
    std::size_t history_limit_;
    double memory_ = 0.0;
    bool memory_set_ = false;
    Variables variables_;
    std::deque<HistoryEntry> history_;
};

} // namespace infiltrator::calc
