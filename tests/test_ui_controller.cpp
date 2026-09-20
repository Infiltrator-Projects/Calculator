/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/ui/calculator_ui_controller.hpp"

#include <cmath>
#include <iostream>
#include <string>

using namespace calculator::ui;

namespace {
int failures = 0;

void check(bool condition, const char* expression) {
    if (!condition) {
        std::cerr << "FAIL: " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression)
} // namespace

int main() {
    Controller controller;

    CHECK(controller.state().mode == Mode::Standard);
    CHECK(controller.state().result == "0");
    CHECK(controller.state().status == "READY");

    CHECK(!controller.command_enabled(Command::Equals));
    CHECK(!controller.command_enabled(Command::MemoryRecall));
    CHECK(!controller.command_enabled(Command::MemoryClear));

    controller.dispatch(Command::Digit3);
    CHECK(controller.command_enabled(Command::Add));
    controller.dispatch(Command::Add);
    controller.dispatch(Command::Digit3);
    CHECK(controller.state().result == "6");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().expression == "3+3");
    CHECK(controller.state().result == "6");

    controller.dispatch(Command::Clear);
    CHECK(controller.state().expression.empty());
    CHECK(controller.state().result == "0");

    controller.dispatch(Command::Digit2);
    controller.dispatch(Command::Add);
    controller.dispatch(Command::Digit3);
    controller.dispatch(Command::Multiply);
    controller.dispatch(Command::Digit4);
    CHECK(controller.state().result == "20");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "20");

    controller.dispatch(Command::Clear);
    controller.set_expression("100");
    controller.dispatch(Command::Add);
    controller.dispatch(Command::Digit1);
    controller.dispatch(Command::Digit0);
    controller.dispatch(Command::Percent);
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "110");

    controller.set_expression("sqrt(9)");
    CHECK(!controller.command_enabled(Command::Equals));
    controller.dispatch(Command::Equals);
    CHECK(!controller.state().fault);

    controller.set_mode(Mode::Scientific);
    controller.set_expression("sqrt(9)");
    CHECK(controller.command_enabled(Command::Equals));
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "3");
    controller.set_mode(Mode::Standard);

    controller.set_expression("6");
    CHECK(controller.command_enabled(Command::MemoryAdd));
    controller.dispatch(Command::MemoryStore);
    CHECK(controller.command_enabled(Command::MemoryRecall));
    controller.dispatch(Command::MemoryAdd);
    CHECK(controller.command_enabled(Command::MemoryClear));
    controller.dispatch(Command::Clear);
    controller.dispatch(Command::MemoryRecall);
    CHECK(controller.state().expression == "12");
    controller.dispatch(Command::MemoryClear);
    CHECK(!controller.command_enabled(Command::MemoryRecall));

    controller.set_mode(Mode::Scientific);
    CHECK(controller.state().status == "SCIENTIFIC · DEG");
    controller.set_expression("2+3*4");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "14");
    controller.set_expression("30");
    controller.dispatch(Command::Sin);
    CHECK(std::fabs(std::stod(controller.state().result) - 0.5) < 1e-12);

    CHECK(controller.button_label(Command::Sin, "sin") == "sin");
    controller.dispatch(Command::ToggleSecond);
    CHECK(controller.state().scientific_second);
    CHECK(controller.button_label(Command::Sin, "sin") == "asin");
    controller.set_expression("1");
    controller.dispatch(Command::Sin);
    CHECK(std::fabs(std::stod(controller.state().result) - 90.0) < 1e-10);

    controller.dispatch(Command::ToggleHyperbolic);
    CHECK(controller.state().scientific_hyperbolic);
    CHECK(controller.button_label(Command::Sin, "sin") == "asinh");
    controller.dispatch(Command::ToggleSecond);
    CHECK(!controller.state().scientific_second);
    CHECK(controller.button_label(Command::Sin, "sin") == "sinh");

    controller.dispatch(Command::CycleAngleUnit);
    CHECK(controller.state().angle_unit == calculator::AngleUnit::Radians);
    CHECK(controller.button_label(Command::CycleAngleUnit, "DEG") == "RAD");
    controller.dispatch(Command::CycleAngleUnit);
    CHECK(controller.state().angle_unit == calculator::AngleUnit::Gradians);
    CHECK(controller.button_label(Command::CycleAngleUnit, "DEG") == "GRAD");
    controller.dispatch(Command::CycleAngleUnit);
    CHECK(controller.state().angle_unit == calculator::AngleUnit::Degrees);

    controller.dispatch(Command::ToggleScientificNotation);
    CHECK(controller.state().scientific_notation);
    controller.set_expression("1000");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result.find("e+03") != std::string::npos);

    controller.set_mode(Mode::Programmer);
    controller.dispatch(Command::BaseBin);
    CHECK(!controller.command_enabled(Command::Digit2));
    CHECK(!controller.command_enabled(Command::Digit9));
    CHECK(!controller.command_enabled(Command::HexA));
    controller.dispatch(Command::BaseHex);
    CHECK(controller.state().programmer_base ==
           calculator::ProgrammerBase::Hexadecimal);
    CHECK(controller.command_enabled(Command::Digit9));
    CHECK(controller.command_enabled(Command::HexA));
    controller.set_expression("F+1");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "10");
    controller.dispatch(Command::AllClear);
    controller.dispatch(Command::Width8);
    controller.set_expression("59");
    controller.dispatch(Command::RotateLeft);
    controller.dispatch(Command::Digit3);
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "CA");

    controller.dispatch(Command::AllClear);
    controller.dispatch(Command::HexC);
    CHECK(controller.state().expression == "C");

    controller.set_mode(Mode::Standard);
    controller.set_expression("123");
    auto backspace = controller.dispatch(Command::Backspace, 2);
    CHECK(controller.state().expression == "13");
    CHECK(backspace.cursor == 1);

    controller.clear_history();
    CHECK(controller.history_text() == "No calculations yet.");
    CHECK(controller.history_count() == 0);

    controller.set_mode(Mode::Standard);
    controller.set_expression("7+5");
    controller.dispatch(Command::Equals);
    CHECK(controller.history_count() == 1);
    CHECK(controller.history_entry(0).has_value());
    CHECK(controller.history_entry(0)->kind ==
          calculator::HistoryKind::Standard);
    CHECK(controller.history_entry(0)->output == "12");

    controller.set_mode(Mode::Standard);
    controller.set_expression("12345");
    const auto standard_details = controller.additional_results();
    CHECK(standard_details.size() == 3);
    CHECK(standard_details[0].label == "Decimal");
    CHECK(standard_details[0].value == "12345");
    CHECK(standard_details[1].label == "Scientific");
    CHECK(standard_details[1].value.find("e+04") != std::string::npos);
    CHECK(standard_details[2].label == "Engineering");
    CHECK(standard_details[2].value == "12.345e+03");

    controller.set_mode(Mode::Scientific);
    controller.set_expression("sqrt(81)");
    const std::string scientific_details =
        controller.additional_results_text();
    CHECK(scientific_details.find("Decimal  9") != std::string::npos);
    CHECK(scientific_details.find("Engineering  9e+00") !=
          std::string::npos);

    controller.set_expression("5e-324");
    const auto subnormal_details = controller.additional_results();
    CHECK(subnormal_details.size() == 3);
    CHECK(subnormal_details[2].label == "Engineering");
    CHECK(subnormal_details[2].value.find("e-324") != std::string::npos);
    CHECK(subnormal_details[2].value.find("inf") == std::string::npos);
    CHECK(subnormal_details[2].value.find("nan") == std::string::npos);

    controller.set_mode(Mode::Programmer);
    controller.dispatch(Command::BaseHex);
    controller.dispatch(Command::Width8);
    controller.dispatch(Command::ToggleSigned);
    controller.set_expression("FF");
    controller.dispatch(Command::Equals);
    CHECK(controller.history_count() == 2);
    CHECK(controller.history_entry(0)->kind ==
          calculator::HistoryKind::Programmer);
    CHECK(controller.history_entry(0)->output == "-1");

    controller.set_mode(Mode::Standard);
    CHECK(controller.recall_history(0));
    CHECK(controller.state().mode == Mode::Programmer);
    CHECK(controller.state().programmer_base ==
          calculator::ProgrammerBase::Hexadecimal);
    CHECK(controller.state().programmer_width ==
          calculator::IntegerWidth::Bits8);
    CHECK(controller.state().programmer_signed);
    CHECK(controller.state().expression == "FF");
    CHECK(controller.state().result == "-1");

    CHECK(controller.recall_history(1));
    CHECK(controller.state().mode == Mode::Standard);
    CHECK(controller.state().expression == "7+5");
    CHECK(controller.state().result == "12");
    CHECK(!controller.recall_history(99));

    controller.set_mode(Mode::Programmer);
    controller.dispatch(Command::BaseHex);
    controller.dispatch(Command::Width8);
    controller.set_expression("FF");
    const std::string representations =
        controller.programmer_representations_text();
    CHECK(controller.additional_results().size() == 4);
    CHECK(representations == controller.additional_results_text());
    CHECK(representations.find("HEX  FF") != std::string::npos);
    CHECK(representations.find("OCT  377") != std::string::npos);
    CHECK(representations.find("BIN  1111 1111") != std::string::npos);

    if (failures != 0) {
        std::cerr << failures << " UI controller test(s) failed\n";
        return 1;
    }
    std::cout << "UI controller tests passed\n";
    return 0;
}
