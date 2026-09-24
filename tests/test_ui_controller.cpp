/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/ui/calculator_ui_controller.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
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

std::uint64_t double_bits(double value) {
    std::uint64_t result = 0U;
    static_assert(sizeof(result) == sizeof(value));
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
} // namespace

int main() {
    Controller controller;

    CHECK(controller.state().mode == Mode::Standard);
    CHECK(controller.state().result == "0");
    CHECK(controller.state().status == "READY");

    CHECK(!controller.command_enabled(Command::Equals));
    CHECK(!controller.command_enabled(Command::MemoryRecall));
    CHECK(!controller.command_enabled(Command::MemoryClear));
    CHECK(!controller.command_enabled(Command::ClearEntry));

    controller.dispatch(Command::Digit3);
    CHECK(controller.command_enabled(Command::ClearEntry));
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

    controller.set_expression("12+34");
    auto clear_entry = controller.dispatch(Command::ClearEntry);
    CHECK(controller.state().expression == "12+");
    CHECK(controller.state().result == "0");
    CHECK(controller.state().status == "READY");
    CHECK(clear_entry.cursor == 3);
    CHECK(clear_entry.expression_changed);
    controller.dispatch(Command::Digit5);
    CHECK(controller.state().expression == "12+5");
    CHECK(controller.state().result == "17");

    controller.set_expression("1+2e-3");
    controller.dispatch(Command::ClearEntry);
    CHECK(controller.state().expression == "1+");
    CHECK(controller.state().result == "0");

    controller.set_expression("10+(2+3)");
    controller.dispatch(Command::ClearEntry);
    CHECK(controller.state().expression == "10+");
    CHECK(controller.state().result == "0");

    controller.set_expression("987");
    controller.dispatch(Command::ClearEntry);
    CHECK(controller.state().expression.empty());
    CHECK(controller.state().result == "0");
    CHECK(!controller.command_enabled(Command::ClearEntry));

    controller.set_expression("12+34*56");
    clear_entry = controller.dispatch(Command::ClearEntry, 4);
    CHECK(controller.state().expression == "12+*56");
    CHECK(clear_entry.cursor == 3);
    controller.dispatch(Command::Digit5, clear_entry.cursor);
    CHECK(controller.state().expression == "12+5*56");

    controller.dispatch(Command::Clear);
    controller.dispatch(Command::Digit2);
    controller.dispatch(Command::Add);
    controller.dispatch(Command::Digit3);
    controller.dispatch(Command::Multiply);
    controller.dispatch(Command::Digit4);
    CHECK(controller.state().result == "14");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "14");

    // Regression: Standard displays and evaluates an infix mathematical
    // expression, so multiplication/division bind before addition/subtraction.
    controller.set_expression("200+200/2");
    CHECK(controller.state().result == "300");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "300");

    controller.dispatch(Command::Clear);
    controller.set_expression("100");
    controller.dispatch(Command::Add);
    controller.dispatch(Command::Digit1);
    controller.dispatch(Command::Digit0);
    controller.dispatch(Command::Percent);
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "100.1");

    controller.set_expression("sqrt(9)");
    CHECK(!controller.command_enabled(Command::Equals));
    controller.dispatch(Command::Equals);
    CHECK(!controller.state().fault);

    controller.set_mode(Mode::Scientific);
    controller.set_expression("sqrt(9)");
    CHECK(controller.command_enabled(Command::Equals));
    CHECK(controller.state().result == "3");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "3");

    controller.set_expression("sqr");
    CHECK(controller.completion_candidates("sqr").size() == 1U);
    const auto completed = controller.complete_expression();
    CHECK(completed.expression_changed);
    CHECK(controller.state().expression == "sqrt(");
    CHECK(completed.cursor == controller.state().expression.size());

    controller.set_expression("SQR");
    CHECK(controller.completion_candidates("SQR").size() == 1U);
    const auto case_completed = controller.complete_expression();
    CHECK(case_completed.expression_changed);
    CHECK(controller.state().expression == "sqrt(");

    controller.set_expression("1/7");
    CHECK(controller.state().result.size() > 40U);
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result.size() > 40U);
    CHECK(controller.state().result.rfind(
        "0.142857142857142857142857", 0) == 0U);

    controller.set_expression("-1");
    controller.dispatch(Command::SquareRoot);
    CHECK(controller.state().result == "i");
    CHECK(controller.state().expression == "i");

    controller.set_expression("1e1000*1e1000");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "1e+2000");

    const std::uint64_t before_definition_history =
        controller.history_revision();
    controller.set_expression("inc(x)=x+1");
    CHECK(controller.command_enabled(Command::Equals));
    CHECK(!controller.command_enabled(Command::MemoryStore));
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "Function defined: inc");
    CHECK(controller.history_revision() == before_definition_history + 1U);
    controller.set_expression("inc(2)");
    CHECK(controller.command_enabled(Command::Equals));
    CHECK(controller.command_enabled(Command::MemoryStore));
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "3");

    controller.set_expression("saved=7");
    CHECK(controller.command_enabled(Command::Equals));
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "7");
    controller.set_expression("saved*2");
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "14");

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

    // Computational state must never be rebuilt from rounded presentation.
    // Unary transforms preserve the exact binary64 value even when the visible
    // result is deliberately rounded by display preferences.
    calculator::ui::DisplayPreferences forensic_fixed{};
    forensic_fixed.format = calculator::ui::ResultFormat::Fixed;
    forensic_fixed.decimal_places = 3;
    forensic_fixed.group_thousands = true;
    forensic_fixed.trailing_zeroes = true;
    controller.set_display_preferences(forensic_fixed);
    controller.set_expression("2");
    controller.dispatch(Command::SquareRoot);
    const double sqrt_two = std::sqrt(2.0);
    const auto sqrt_state =
        calculator::evaluate_standard(controller.state().expression);
    CHECK(sqrt_state.ok);
    CHECK(double_bits(sqrt_state.value) == double_bits(sqrt_two));
    CHECK(controller.state().result == "1.414");
    CHECK(controller.state().expression.find(',') == std::string::npos);

    controller.dispatch(Command::Square);
    const double squared_sqrt_two = sqrt_two * sqrt_two;
    const auto squared_state =
        calculator::evaluate_standard(controller.state().expression);
    CHECK(squared_state.ok);
    CHECK(double_bits(squared_state.value) ==
          double_bits(squared_sqrt_two));
    CHECK(controller.state().result == "2.000");

    controller.set_expression("1000000");
    controller.dispatch(Command::Square);
    CHECK(controller.state().result == "1,000,000,000,000.000");
    CHECK(controller.state().expression.find(',') == std::string::npos);
    CHECK(calculator::evaluate_standard(
              controller.state().expression).ok);

    // Memory recall is also a computational-state boundary: a binary64 value
    // must round-trip exactly rather than through the 15-digit display format.
    controller.set_display_preferences({});
    controller.set_expression("1/7");
    const auto seventh =
        calculator::evaluate_standard(controller.state().expression);
    CHECK(seventh.ok);
    controller.dispatch(Command::MemoryStore);
    controller.dispatch(Command::Clear);
    controller.dispatch(Command::MemoryRecall);
    const auto recalled_seventh =
        calculator::evaluate_standard(controller.state().expression);
    CHECK(recalled_seventh.ok);
    CHECK(double_bits(recalled_seventh.value) ==
          double_bits(seventh.value));
    controller.dispatch(Command::Reciprocal);
    CHECK(controller.state().result == "7");
    controller.dispatch(Command::MemoryClear);

    controller.set_expression(
        calculator::serialize_value(
            std::numeric_limits<double>::max()));
    controller.dispatch(Command::MemoryStore);
    controller.dispatch(Command::MemoryAdd);
    CHECK(controller.state().fault);
    CHECK(controller.state().status == "MEMORY RANGE ERROR");
    controller.dispatch(Command::Clear);
    CHECK(controller.command_enabled(Command::MemoryRecall));
    controller.dispatch(Command::MemoryRecall);
    const auto preserved_max =
        calculator::evaluate_standard(controller.state().expression);
    CHECK(preserved_max.ok);
    CHECK(double_bits(preserved_max.value) ==
          double_bits(std::numeric_limits<double>::max()));
    controller.dispatch(Command::MemoryClear);

    // A Scientific value outside binary64 remains valid memory, but Standard
    // must not silently substitute zero or another approximate value.
    controller.set_mode(Mode::Scientific);
    controller.set_expression("1e1000");
    controller.dispatch(Command::MemoryStore);
    CHECK(controller.command_enabled(Command::MemoryRecall));
    controller.set_mode(Mode::Standard);
    CHECK(controller.command_enabled(Command::MemoryClear));
    CHECK(!controller.command_enabled(Command::MemoryRecall));
    controller.set_expression("5");
    controller.dispatch(Command::MemoryStore);
    CHECK(controller.command_enabled(Command::MemoryRecall));
    controller.dispatch(Command::MemoryClear);

    controller.set_mode(Mode::Scientific);

    // Scientific computational state is also independent of display rounding.
    // Fixed/grouped presentation must not feed back through expression state,
    // unary operations or the shared memory register.
    controller.set_display_preferences(forensic_fixed);
    controller.set_angle_unit(calculator::AngleUnit::Radians);
    controller.set_expression("2");
    const auto scientific_sqrt_expected =
        calculator::evaluate_scientific(
            "sqrt(2)", {}, {}, calculator::AngleUnit::Radians,
            calculator::kScientificDefaultDigits);
    CHECK(scientific_sqrt_expected.ok);
    controller.dispatch(Command::SquareRoot);
    CHECK(controller.state().result == "1.414");
    CHECK(controller.state().expression.find(',') == std::string::npos);
    const auto scientific_sqrt_actual =
        calculator::evaluate_scientific(
            controller.state().expression, {}, {},
            calculator::AngleUnit::Radians,
            calculator::kScientificDefaultDigits);
    CHECK(scientific_sqrt_actual.ok);
    CHECK(scientific_sqrt_actual.value.real ==
          scientific_sqrt_expected.value.real);
    CHECK(scientific_sqrt_actual.value.imag ==
          scientific_sqrt_expected.value.imag);

    controller.set_expression("1/7");
    const auto scientific_seventh =
        calculator::evaluate_scientific(
            "1/7", {}, {}, calculator::AngleUnit::Radians,
            calculator::kScientificDefaultDigits);
    CHECK(scientific_seventh.ok);
    controller.dispatch(Command::MemoryStore);
    controller.dispatch(Command::Clear);
    controller.dispatch(Command::MemoryRecall);
    const auto scientific_recalled =
        calculator::evaluate_scientific(
            controller.state().expression, {}, {},
            calculator::AngleUnit::Radians,
            calculator::kScientificDefaultDigits);
    CHECK(scientific_recalled.ok);
    CHECK(scientific_recalled.value.real ==
          scientific_seventh.value.real);
    CHECK(scientific_recalled.value.imag ==
          scientific_seventh.value.imag);

    const auto scientific_reciprocal_expected =
        calculator::evaluate_scientific(
            "1/(" + calculator::scientific_value_expression(
                scientific_seventh.value) + ")",
            {}, {}, calculator::AngleUnit::Radians,
            calculator::kScientificDefaultDigits);
    CHECK(scientific_reciprocal_expected.ok);
    controller.dispatch(Command::Reciprocal);
    const auto scientific_reciprocal_actual =
        calculator::evaluate_scientific(
            controller.state().expression, {}, {},
            calculator::AngleUnit::Radians,
            calculator::kScientificDefaultDigits);
    CHECK(scientific_reciprocal_actual.ok);
    CHECK(scientific_reciprocal_actual.value.real ==
          scientific_reciprocal_expected.value.real);
    CHECK(scientific_reciprocal_actual.value.imag ==
          scientific_reciprocal_expected.value.imag);
    controller.dispatch(Command::MemoryClear);
    controller.set_display_preferences({});
    controller.set_angle_unit(calculator::AngleUnit::Radians);

    CHECK(controller.state().status == "SCIENTIFIC · RAD");
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

    controller.set_mode(Mode::Scientific);
    controller.set_expression("12345.5");
    fixed.trailing_zeroes = true;
    controller.set_display_preferences(fixed);
    controller.dispatch(Command::Equals);
    CHECK(controller.state().result == "12,345.500");

    fixed.trailing_zeroes = false;
    controller.set_display_preferences(fixed);
    CHECK(controller.state().result == "12,345.5");

    controller.set_display_preferences(scientific);
    CHECK(controller.state().result.find("1.2346e+04") !=
          std::string::npos);

    controller.set_display_preferences({});

    controller.set_mode(Mode::Programmer);
    controller.set_programmer_context(
        calculator::ProgrammerBase::Octal,
        calculator::IntegerWidth::Bits16, true);
    CHECK(controller.state().programmer_base == calculator::ProgrammerBase::Octal);
    CHECK(controller.state().programmer_width == calculator::IntegerWidth::Bits16);
    CHECK(controller.state().programmer_signed);
    controller.set_programmer_context(
        calculator::ProgrammerBase::Decimal,
        calculator::IntegerWidth::Bits64, false);
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

    const std::uint64_t before_history_delete =
        controller.history_revision();
    CHECK(controller.delete_history(0));
    CHECK(controller.history_count() == 1U);
    CHECK(controller.history_revision() == before_history_delete + 1U);
    CHECK(controller.history_entry(0)->input == "7+5");
    CHECK(!controller.delete_history(99));

    controller.set_mode(Mode::Programmer);
    controller.dispatch(Command::BaseHex);
    controller.dispatch(Command::Width8);
    controller.set_expression("");

    const std::string zero_representations =
        controller.programmer_representations_text();
    CHECK(zero_representations.find("HEX  0") != std::string::npos);
    CHECK(zero_representations.find("DEC  0") != std::string::npos);
    CHECK(zero_representations.find("OCT  0") != std::string::npos);
    CHECK(zero_representations.find("BIN  0000 0000") != std::string::npos);

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
    controller.set_programmer_context(
        calculator::ProgrammerBase::Hexadecimal,
        calculator::IntegerWidth::Bits32, false);
    controller.set_expression("12345678");
    CHECK(controller.swap_programmer_endianness());
    CHECK(controller.state().expression == "78563412");
    CHECK(controller.state().result == "7856 3412");
    controller.set_mode(Mode::Standard);
    CHECK(!controller.toggle_programmer_bit(0U));

    // Scientific computation precision is an explicit shared setting, distinct
    // from display decimal places.
    controller.set_mode(Mode::Scientific);
    controller.set_scientific_digits(80U);
    CHECK(controller.scientific_digits() == 80U);
    controller.set_expression("1/7");
    CHECK(controller.state().result.size() > 70U);
    controller.set_scientific_digits(1U);
    CHECK(controller.scientific_digits() == calculator::kScientificMinDigits);
    controller.set_scientific_digits(5000U);
    CHECK(controller.scientific_digits() == calculator::kScientificMaxDigits);
    controller.set_scientific_digits(80U);

    // Persistence is one versioned Controller-owned document. Loading validates
    // the full document before replacing variables/functions/preferences.
    DisplayPreferences persisted_preferences{};
    persisted_preferences.format = ResultFormat::Fixed;
    persisted_preferences.decimal_places = 6U;
    persisted_preferences.group_thousands = true;
    persisted_preferences.trailing_zeroes = true;
    controller.set_display_preferences(persisted_preferences);
    CHECK(controller.load_function_definitions_text("twice(x)=x*2\n"));
    CHECK(controller.load_variables_text("persisted=123.5\n"));
    controller.set_expression("");
    controller.set_angle_unit(calculator::AngleUnit::Gradians);
    controller.set_programmer_context(
        calculator::ProgrammerBase::Hexadecimal,
        calculator::IntegerWidth::Bits32, true);
    controller.set_mode(Mode::Programmer);
    controller.set_history_limit(500U);
    CHECK(controller.history_limit() == 500U);
    const std::string persistent_state = controller.persistent_state_text();
    CHECK(persistent_state.find("INFILTRATOR_CALCULATOR_STATE 2") == 0U);

    Controller restored;
    CHECK(restored.load_persistent_state_text(persistent_state));
    CHECK(restored.scientific_digits() == 80U);
    CHECK(restored.display_preferences().format == ResultFormat::Fixed);
    CHECK(restored.display_preferences().decimal_places == 6U);
    CHECK(restored.display_preferences().group_thousands);
    CHECK(restored.display_preferences().trailing_zeroes);
    CHECK(restored.state().mode == Mode::Programmer);
    CHECK(restored.state().angle_unit == calculator::AngleUnit::Gradians);
    CHECK(restored.state().programmer_base ==
          calculator::ProgrammerBase::Hexadecimal);
    CHECK(restored.state().programmer_width ==
          calculator::IntegerWidth::Bits32);
    CHECK(restored.state().programmer_signed);
    CHECK(restored.history_limit() == 500U);
    CHECK(restored.variables_text().find("persisted=123.5") !=
          std::string::npos);
    CHECK(restored.function_definitions_text().find("twice(x)=x*2") !=
          std::string::npos);

    // Existing v1 documents remain readable during the migration.
    const std::string legacy_state =
        "INFILTRATOR_CALCULATOR_STATE 1\n"
        "scientific-digits=50\n"
        "result-format=0\n"
        "decimal-places=9\n"
        "group-thousands=0\n"
        "trailing-zeroes=0\n"
        "variables-bytes=0\n"
        "\n"
        "functions-bytes=0\n";
    Controller legacy_restored;
    CHECK(legacy_restored.load_persistent_state_text(legacy_state));
    CHECK(legacy_restored.scientific_digits() == 50U);
    CHECK(legacy_restored.state().mode == Mode::Standard);

    const std::string before_bad_state = restored.persistent_state_text();
    CHECK(!restored.load_persistent_state_text(
        "INFILTRATOR_CALCULATOR_STATE 2\nmode=9\n"));
    CHECK(restored.persistent_state_text() == before_bad_state);

    // Oversize typed/pasted expressions fail at the shared Controller boundary
    // instead of reaching a parser or creating platform-specific limits.
    CHECK(!restored.set_expression(
        std::string(Controller::kMaxExpressionBytes + 1U, '1')));
    CHECK(restored.state().fault);
    CHECK(restored.state().status == "INPUT LIMIT");
    CHECK(restored.state().expression.empty());

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
