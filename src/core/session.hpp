/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "calculator.hpp"
#include "scientific.hpp"

#include <cstddef>
#include <cstdint>
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
// variables, user functions, memory-set state and history; it delegates all
// mathematical semantics to the calculation engines. History is unbounded by
// default and may be explicitly bounded for embedders/tests.
class Session {
public:
    // A zero limit means unbounded history, matching mature desktop
    // calculator behaviour. Tests and embedders may still request a bound.
    explicit Session(std::size_t history_limit = 0);

    // Supports direct assignment as name=expression. The identifier must begin
    // with an alphabetic character or '_' and '=' immediately follows the name.
    // Every evaluation, including an error, is recorded in bounded history.
    // Side-effect-free validation/evaluation using the same variables,
    // functions and assignment/definition grammar as evaluate().
    Result preview(
        const std::string& input,
        AngleUnit angle_unit = AngleUnit::Radians) const;
    Result evaluate(
        const std::string& input,
        AngleUnit angle_unit = AngleUnit::Radians);

    ScientificResult preview_scientific(
        const std::string& input,
        AngleUnit angle_unit = AngleUnit::Radians,
        unsigned decimal_digits = kScientificDefaultDigits) const;
    ScientificResult evaluate_scientific(
        const std::string& input,
        AngleUnit angle_unit = AngleUnit::Radians,
        unsigned decimal_digits = kScientificDefaultDigits);

    // Memory is one logical register with binary64 and Scientific views.
    // Storing through either domain marks it present; memory_empty() is about
    // presence, not whether the numerical value happens to be zero.
    void memory_clear();
    bool memory_store(double value);
    bool memory_add(double value);
    bool memory_subtract(double value);
    double memory_recall() const noexcept;
    bool memory_binary64_available() const noexcept;
    void memory_store_scientific(ScientificValue value);
    void memory_add_scientific(
        const ScientificValue& value,
        unsigned decimal_digits = kScientificDefaultDigits);
    void memory_subtract_scientific(
        const ScientificValue& value,
        unsigned decimal_digits = kScientificDefaultDigits);
    ScientificValue memory_recall_scientific() const;

    // Distinguishes "never set/cleared" from a legitimate stored numeric zero.
    bool memory_empty() const noexcept;

    void record_history(
        std::string input, const Result& result,
        HistoryKind kind = HistoryKind::Scientific,
        HistoryContext context = {});
    void record_history_text(
        std::string input, std::string output, bool ok,
        HistoryKind kind, HistoryContext context = {});

    // Variable/function text forms are Calculator-owned persistence formats.
    // Load operations validate into temporary state and commit only a fully
    // valid document, so malformed persistence cannot partially replace state.
    void set_variable(std::string name, double value);
    std::optional<double> variable(const std::string& name) const;
    const Variables& variables() const noexcept;
    const ScientificVariables& scientific_variables() const noexcept;
    std::string variables_text() const;
    bool load_variables_text(std::string_view text);

    std::optional<FunctionDefinition> function(const std::string& name) const;
    const Functions& functions() const noexcept;
    bool remove_function(const std::string& name);
    std::string function_definitions_text() const;
    bool load_function_definitions_text(std::string_view text);

    // History entries retain rendered output plus mode-specific context so
    // recall can restore Programmer radix/width/signed state without re-parsing
    // old display text. References remain valid only until Session mutates.
    const std::deque<HistoryEntry>& history() const noexcept;
    std::size_t history_count() const noexcept;
    std::optional<HistoryEntry> history_from_newest(
        std::size_t index) const;
    std::string history_text(
        std::size_t limit = 50,
        std::string_view newline = "\n") const;
    bool erase_history_from_newest(std::size_t index);
    void clear_history() noexcept;
    std::uint64_t history_revision() const noexcept;

private:
    std::size_t history_limit_;
    std::uint64_t history_revision_ = 0;
    double memory_ = 0.0;
    ScientificValue scientific_memory_{};
    bool memory_set_ = false;
    bool memory_binary64_valid_ = false;
    Variables variables_;
    ScientificVariables scientific_variables_;
    Functions functions_;
    std::deque<HistoryEntry> history_;
};

} // namespace calculator
