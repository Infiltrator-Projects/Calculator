/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "calculator.hpp"

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace calculator {

enum class HistoryKind {
    Standard,
    Scientific,
    Programmer
};

struct HistoryContext {
    unsigned programmer_base = 10;
    unsigned programmer_width = 64;
    bool programmer_signed = false;
};

struct HistoryEntry {
    HistoryKind kind = HistoryKind::Scientific;
    HistoryContext context{};
    std::string input;
    std::string output;
    bool ok = false;
};

// Process-local calculator state shared across evaluations. Session owns
// variables, memory-set state and bounded history; it delegates all mathematical
// semantics to the calculation engines.
class Session {
public:
    // A zero limit means unbounded history, matching mature desktop
    // calculator behaviour. Tests and embedders may still request a bound.
    explicit Session(std::size_t history_limit = 0);

    // Supports direct assignment as name=expression. The identifier must begin
    // with an alphabetic character or '_' and '=' immediately follows the name.
    // Every evaluation, including an error, is recorded in bounded history.
    Result evaluate(const std::string& input);

    void memory_clear();
    void memory_store(double value);
    void memory_add(double value);
    void memory_subtract(double value);
    double memory_recall() const noexcept;

    // Distinguishes "never set/cleared" from a legitimate stored numeric zero.
    bool memory_empty() const noexcept;

    void record_history(
        std::string input, const Result& result,
        HistoryKind kind = HistoryKind::Scientific,
        HistoryContext context = {});
    void record_history_text(
        std::string input, std::string output, bool ok,
        HistoryKind kind, HistoryContext context = {});

    void set_variable(std::string name, double value);
    std::optional<double> variable(const std::string& name) const;
    const Variables& variables() const noexcept;

    const std::deque<HistoryEntry>& history() const noexcept;
    std::size_t history_count() const noexcept;
    std::optional<HistoryEntry> history_from_newest(
        std::size_t index) const;
    std::string history_text(
        std::size_t limit = 50,
        std::string_view newline = "\n") const;
    void clear_history() noexcept;

private:
    std::size_t history_limit_;
    double memory_ = 0.0;
    bool memory_set_ = false;
    Variables variables_;
    std::deque<HistoryEntry> history_;
};

} // namespace calculator
