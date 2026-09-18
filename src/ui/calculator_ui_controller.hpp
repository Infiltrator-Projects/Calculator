/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "../core/programmer.hpp"
#include "../core/session.hpp"
#include "calculator_ui_contract.hpp"

#include <cstddef>
#include <string>

namespace infiltrator::calc::ui {

struct ViewState {
    Mode mode = Mode::Standard;
    std::string expression;
    std::string result = "0";
    std::string status = "READY";
    bool fault = false;
    bool degrees = true;
    ProgrammerBase programmer_base = ProgrammerBase::Decimal;
    IntegerWidth programmer_width = IntegerWidth::Bits64;
    bool programmer_signed = false;
};

struct DispatchResult {
    std::size_t cursor = 0;
    bool expression_changed = false;
};

class Controller {
public:
    static constexpr std::size_t kEnd = static_cast<std::size_t>(-1);

    const ViewState& state() const noexcept { return state_; }

    Session& session() noexcept { return session_; }
    const Session& session() const noexcept { return session_; }

    void set_expression(std::string expression);
    void set_mode(Mode mode);
    void clear_history() noexcept;

    DispatchResult dispatch(Command command, std::size_t cursor = kEnd);

private:
    std::size_t normalized_cursor(std::size_t cursor) const noexcept;
    DispatchResult insert(std::string_view text, std::size_t cursor);
    DispatchResult backspace(std::size_t cursor);

    bool current_value(double& value);
    bool current_programmer_value(std::uint64_t& value);

    void calculate();
    void calculate_programmer();
    void clear_calculation();
    void unary_transform(Command command);
    void scientific_transform(Command command);
    void programmer_mode_change(Command command);

    void set_status(std::string text, bool fault = false);
    std::string programmer_status_text() const;

    Session session_;
    ViewState state_;
};

} // namespace infiltrator::calc::ui
