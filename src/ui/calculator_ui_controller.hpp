/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "../core/programmer.hpp"
#include "../core/session.hpp"
#include "calculator_ui_contract.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

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

    const ViewState& state() const noexcept { return state_; }

    void set_expression(std::string expression);
    void set_mode(Mode mode);
    std::string history_text(
        std::size_t limit = 50,
        std::string_view newline = "\n") const;
    void clear_history() noexcept;

    std::string button_label(
        Command command, std::string_view fallback) const;
    bool command_enabled(Command command) const;
    DispatchResult dispatch(Command command, std::size_t cursor = kEnd);

private:
    std::size_t normalized_cursor(std::size_t cursor) const noexcept;
    DispatchResult insert(std::string_view text, std::size_t cursor);
    DispatchResult backspace(std::size_t cursor);

    bool current_value(double& value);
    bool current_programmer_value(std::uint64_t& value);

    void calculate();
    void calculate_standard();
    void calculate_programmer();
    void clear_calculation();
    void update_standard_preview();
    void unary_transform(Command command);
    void scientific_transform(Command command);
    void programmer_mode_change(Command command);

    void set_status(std::string text, bool fault = false);
    std::string scientific_status_text() const;
    std::string programmer_status_text() const;
    std::string format_real(double value) const;
    Command effective_scientific_command(Command command) const;
    bool current_number_has_decimal() const;
    bool has_unmatched_open_parenthesis() const;
    bool expression_has_value() const;
    bool expression_ends_with_binary_operator() const;

    Session session_;
    ViewState state_;
};

} // namespace calculator::ui
