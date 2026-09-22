/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "../core/programmer.hpp"
#include "../core/session.hpp"
#include "calculator_ui_contract.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace calculator::ui {

struct ViewState {
    Mode mode = Mode::Standard;
    std::string expression;
    std::string result = "0";
    std::string status = "READY";
    bool fault = false;
    AngleUnit angle_unit = AngleUnit::Degrees;
    bool scientific_second = false;
    bool scientific_hyperbolic = false;
    bool scientific_notation = false;
    ProgrammerBase programmer_base = ProgrammerBase::Decimal;
    IntegerWidth programmer_width = IntegerWidth::Bits64;
    bool programmer_signed = false;
};

enum class ResultFormat {
    Automatic,
    Fixed,
    Scientific,
    Engineering
};

struct DisplayPreferences {
    ResultFormat format = ResultFormat::Automatic;
    unsigned decimal_places = 9;
    bool group_thousands = false;
    bool trailing_zeroes = false;
};

struct AdditionalResult {
    std::string label;
    std::string value;
};

struct DispatchResult {
    // Cursor position the platform shell should restore after dispatch.
    std::size_t cursor = 0;
    bool expression_changed = false;
};

// Authoritative desktop interaction state machine. GTK and Win32 render
// ViewState and dispatch Command values; platform shells must not reproduce
// calculator state transitions or command semantics independently.
class Controller {
public:
    // Sentinel meaning "operate at the current end of the expression".
    static constexpr std::size_t kEnd = static_cast<std::size_t>(-1);

    // Borrowed view of controller-owned state; callers must not retain it
    // across a mutating Controller operation.
    const ViewState& state() const noexcept { return state_; }

    // Direct setters are state mutations: they refresh cached evaluation and
    // derived presentation state so shells never need to evaluate independently.
    void set_expression(std::string expression);
    void set_mode(Mode mode);
    void set_angle_unit(AngleUnit unit);
    void set_programmer_context(
        ProgrammerBase base, IntegerWidth width, bool signed_display);
    // History is ordered newest-first at the indexed/recall API boundary.
    // history_revision() changes only when history content changes and allows
    // native shells to avoid rebuilding unchanged history surfaces.
    std::string history_text(
        std::size_t limit = 50,
        std::string_view newline = "\n") const;
    std::size_t history_count() const noexcept;
    std::uint64_t history_revision() const noexcept;
    std::optional<HistoryEntry> history_entry(
        std::size_t index_from_newest) const;
    bool recall_history(std::size_t index_from_newest);
    void clear_history() noexcept;
    // Derived representation surfaces are generated from the current cached
    // result; they never cause platform-specific re-evaluation.
    std::vector<AdditionalResult> additional_results() const;
    std::string additional_results_text() const;
    std::string programmer_representations_text() const;
    std::vector<bool> programmer_bits() const;
    bool toggle_programmer_bit(unsigned bit);
    std::string function_definitions_text() const;
    bool load_function_definitions_text(std::string_view text);
    std::string variables_text() const;
    bool load_variables_text(std::string_view text);

    // Display preferences affect presentation only; they do not change the
    // underlying Standard or Scientific numeric representation.
    const DisplayPreferences& display_preferences() const noexcept {
        return display_preferences_;
    }
    void set_display_preferences(DisplayPreferences preferences);

    // button_label() and command_enabled() are authoritative UI semantics for
    // every shell. dispatch() is the sole command state-transition entry point;
    // its returned cursor is expressed in the shared expression byte index.
    std::string button_label(
        Command command, std::string_view fallback) const;
    bool command_enabled(Command command) const;
    DispatchResult dispatch(Command command, std::size_t cursor = kEnd);

private:
    std::size_t normalized_cursor(std::size_t cursor) const noexcept;
    DispatchResult insert(std::string_view text, std::size_t cursor);
    DispatchResult backspace(std::size_t cursor);

    bool current_value(double& value);
    bool current_scientific_value(ScientificValue& value);
    bool current_programmer_value(std::uint64_t& value);

    void calculate();
    void calculate_standard();
    void calculate_programmer(bool record_history = true);
    void clear_calculation();
    std::size_t clear_entry(std::size_t cursor);
    void refresh_evaluation_cache();
    void update_standard_preview();
    void unary_transform(Command command);
    void scientific_transform(Command command);
    void programmer_mode_change(Command command);

    void set_status(std::string text, bool fault = false);
    std::string scientific_status_text() const;
    std::string programmer_status_text() const;
    std::string format_scientific_result(
        const ScientificValue& value) const;
    std::string format_display(double value) const;
    Command effective_scientific_command(Command command) const;
    bool current_number_has_decimal() const;
    bool has_unmatched_open_parenthesis() const;
    bool expression_has_value() const;
    bool expression_can_calculate() const;
    bool expression_ends_with_binary_operator() const;

    Session session_;
    ViewState state_;
    DisplayPreferences display_preferences_;
    Result real_cache_;
    ScientificResult scientific_cache_;
    ProgrammerResult programmer_cache_;
    unsigned scientific_digits_ = kScientificDefaultDigits;
};

} // namespace calculator::ui
