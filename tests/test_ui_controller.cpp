/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/ui/calculator_ui_controller.hpp"

#include <cassert>
#include <cmath>
#include <string>

using namespace infiltrator::calc::ui;

int main() {
    Controller controller;

    assert(controller.state().mode == Mode::Standard);
    assert(controller.state().result == "0");
    assert(controller.state().status == "READY");

    controller.dispatch(Command::Digit3);
    controller.dispatch(Command::Add);
    controller.dispatch(Command::Digit3);
    controller.dispatch(Command::Equals);
    assert(controller.state().expression == "3+3");
    assert(controller.state().result == "6");

    controller.dispatch(Command::Clear);
    assert(controller.state().expression.empty());
    assert(controller.state().result == "0");

    controller.set_expression("6");
    controller.dispatch(Command::MemoryAdd);
    controller.dispatch(Command::Clear);
    controller.dispatch(Command::MemoryRecall);
    assert(controller.state().expression == "6");

    controller.set_mode(Mode::Scientific);
    assert(controller.state().status == "SCIENTIFIC · DEGREES");
    controller.set_expression("30");
    controller.dispatch(Command::Sin);
    assert(std::fabs(std::stod(controller.state().result) - 0.5) < 1e-12);

    controller.dispatch(Command::ToggleDegrees);
    assert(!controller.state().degrees);
    assert(controller.state().status == "SCIENTIFIC · RADIANS");

    controller.set_mode(Mode::Programmer);
    controller.dispatch(Command::BaseHex);
    assert(controller.state().programmer_base ==
           infiltrator::calc::ProgrammerBase::Hexadecimal);
    controller.set_expression("F+1");
    controller.dispatch(Command::Equals);
    assert(controller.state().result == "10");

    controller.dispatch(Command::AllClear);
    controller.dispatch(Command::HexC);
    assert(controller.state().expression == "C");

    controller.set_mode(Mode::Standard);
    controller.set_expression("123");
    auto backspace = controller.dispatch(Command::Backspace, 2);
    assert(controller.state().expression == "13");
    assert(backspace.cursor == 1);

    controller.clear_history();
    assert(controller.session().history().empty());

    return 0;
}
