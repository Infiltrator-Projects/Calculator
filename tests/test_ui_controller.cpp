/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/ui/calculator_ui_controller.hpp"

#include <chrono>
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
    controller.set_angle_unit(calculator::AngleUnit::Gradians);
    CHECK(controller.state().angle_unit == calculator::AngleUnit::Gradians);
    CHECK(controller.state().status == "SCIENTIFIC · GRAD");
    controller.set_angle_unit(calculator::AngleUnit::Degrees);
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

    controller.set_expression("sin(30)");
    controller.dispatch(Command::Equals);
    CHECK(std::fabs(std::stod(controller.state().result) - 0.5) < 1e-12);
    controller.dispatch(Command::CycleAngleUnit);
    controller.set_expression("sin(pi/2)");
    controller.dispatch(Command::Equals);
    CHECK(std::fabs(std::stod(controller.state().result) - 1.0) < 1e-12);
    controller.dispatch(Command::CycleAngleUnit);
    controller.dispatch(Command::CycleAngleUnit);
    CHECK(controller.state().angle_unit == calculator::AngleUnit::Degrees);

    controller.dispatch(Command::ToggleScientificNotation);
    CHECK(controller.state().scientific_notation);
    controller.set_expression("1000");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result.find("e+03") != std::string::npos);
    controller.dispatch(Command::Clear);
    CHECK(!controller.state().scientific_notation);
    CHECK(controller.state().result == "0");
    CHECK(controller.state().status.find("F-E") == std::string::npos);

    controller.set_mode(Mode::Standard);
    controller.set_expression("12345.5");
    calculator::ui::DisplayPreferences fixed{};
    fixed.format = calculator::ui::ResultFormat::Fixed;
    fixed.decimal_places = 3;
    fixed.group_thousands = true;
    fixed.trailing_zeroes = true;
    controller.set_display_preferences(fixed);
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "12,345.500");

    fixed.trailing_zeroes = false;
    controller.set_display_preferences(fixed);
    CHECK(controller.state().result == "12,345.5");

    calculator::ui::DisplayPreferences scientific{};
    scientific.format = calculator::ui::ResultFormat::Scientific;
    scientific.decimal_places = 4;
    scientific.trailing_zeroes = true;
    controller.set_display_preferences(scientific);
    CHECK(controller.state().result.find("1.2346e+04") != std::string::npos);

    controller.set_display_preferences({});

    controller.set_mode(Mode::Programmer);
    controller.set_programmer_context(
        calculator::ProgrammerBase::Octal,
        calculator::IntegerWidth::Bits16, true);
    CHECK(controller.state().programmer_base == calculator::ProgrammerBase::Octal);
    CHECK(controller.state().programmer_width == calculator::IntegerWidth::Bits16);
    CHECK(controller.state().programmer_signed);
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

    CHECK(controller.load_variables_text("saved=42\n"));
    CHECK(controller.variables_text().find("saved=42") != std::string::npos);

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

    controller.dispatch(Command::AllClear);
    controller.dispatch(Command::Width8);
    controller.dispatch(Command::BaseHex);
    CHECK(controller.programmer_bits().size() == 8U);
    CHECK(controller.toggle_programmer_bit(7U));
    CHECK(controller.state().expression == "80");
    CHECK(controller.programmer_bits()[7]);
    CHECK(controller.toggle_programmer_bit(0U));
    CHECK(controller.state().expression == "81");
    CHECK(controller.programmer_bits()[0]);
    CHECK(!controller.toggle_programmer_bit(8U));
    controller.set_mode(Mode::Standard);
    CHECK(!controller.toggle_programmer_bit(0U));

    // Keep the hot input path comfortably inside an interactive frame budget.
    // The threshold is deliberately much looser than normal native C++ cost:
    // it detects an order-of-magnitude regression without timing micro-noise.
    controller.set_mode(Mode::Standard);
    constexpr int latency_rounds = 2000;
    for (int warmup = 0; warmup < 20; ++warmup) {
        controller.dispatch(Command::Clear);
        controller.dispatch(Command::Digit1);
        controller.dispatch(Command::Digit2);
        controller.dispatch(Command::Digit3);
        controller.dispatch(Command::Digit4);
        controller.dispatch(Command::Digit5);
        controller.dispatch(Command::Digit6);
        controller.dispatch(Command::Digit7);
    }
    const auto latency_start = std::chrono::steady_clock::now();
    for (int round = 0; round < latency_rounds; ++round) {
        controller.dispatch(Command::Clear);
        controller.dispatch(Command::Digit1);
        controller.dispatch(Command::Digit2);
        controller.dispatch(Command::Digit3);
        controller.dispatch(Command::Digit4);
        controller.dispatch(Command::Digit5);
        controller.dispatch(Command::Digit6);
        controller.dispatch(Command::Digit7);
    }
    const auto latency_elapsed =
        std::chrono::steady_clock::now() - latency_start;
    const double latency_ms =
        std::chrono::duration<double, std::milli>(latency_elapsed).count() /
        static_cast<double>(latency_rounds * 8);
    CHECK(latency_ms < 5.0);
    std::cout << "Average controller keystroke latency: "
              << latency_ms << " ms\n";

    if (failures != 0) {
        std::cerr << failures << " UI controller test(s) failed\n";
        return 1;
    }
    std::cout << "UI controller tests passed\n";
    return 0;
}
