/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/calculator.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

int failures = 0;

void fail(const std::string& message) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

void expect_value(const char* expression, double expected) {
    const auto result = calculator::evaluate(expression);
    if (!result.ok) {
        fail(std::string(expression) + " -> " + result.error);
        return;
    }

    const double tolerance = 1e-12 * std::max(1.0, std::abs(expected));
    if (std::abs(result.value - expected) > tolerance) {
        fail(std::string(expression) + " wrong value");
    }
}

void expect_error(const char* expression) {
    const auto result = calculator::evaluate(expression);
    if (result.ok || result.error.empty()) {
        fail(std::string(expression) + " should fail");
    }
}

void expect_immediate(const char* expression, double expected) {
    const auto result = calculator::evaluate_immediate(expression);
    if (!result.ok) {
        fail(std::string("immediate ") + expression + " -> " + result.error);
        return;
    }

    const double tolerance = 1e-12 * std::max(1.0, std::abs(expected));
    if (std::abs(result.value - expected) > tolerance) {
        fail(std::string("immediate ") + expression + " wrong value");
    }
}

} // namespace

int main() {
    // Precedence, associativity and decimal literal handling.
    expect_value("2 + 3 * 4", 14);
    expect_value("(2 + 3) * 4", 20);
    expect_value("2^3^2", 512);
    expect_value("-4 + 10", 6);
    expect_value("2.5 * 4", 10);
    expect_value("-2^2", -4);
    expect_value("(-2)^2", 4);
    expect_value("2^-2", 0.25);
    expect_value("  6 / 3  ", 2);
    expect_value("1e3 + 2", 1002);
    expect_value(".5 + .25", 0.75);
    expect_value("--5", 5);

    // Postfix operators, constants and mathematical functions.
    expect_value("50%", 0.5);
    expect_value("200 * 10%", 20);
    expect_value("pi", 3.14159265358979323846);
    expect_value("e", 2.71828182845904523536);
    expect_value("sin(pi/2)", 1);
    expect_value("cos(0)", 1);
    expect_value("tan(0)", 0);
    expect_value("asin(1)", 3.14159265358979323846 / 2);
    expect_value("acos(1)", 0);
    expect_value("atan(1)", 3.14159265358979323846 / 4);
    expect_value("sinh(0)", 0);
    expect_value("cosh(0)", 1);
    expect_value("tanh(0)", 0);
    expect_value("asinh(0)", 0);
    expect_value("acosh(1)", 0);
    expect_value("atanh(0)", 0);
    expect_value("square(12)", 144);
    expect_value("cube(3)", 27);
    expect_value("sqrt(81)", 9);
    expect_value("cbrt(27)", 3);
    expect_value("ln(e)", 1);
    expect_value("log(1000)", 3);
    expect_value("exp(0)", 1);
    expect_value("exp2(10)", 1024);
    expect_value("exp10(3)", 1000);
    expect_value("abs(-12.5)", 12.5);
    expect_value("floor(2.9)", 2);
    expect_value("ceil(2.1)", 3);
    expect_value("5!", 120);
    expect_value("0!", 1);
    expect_value("3!^2", 36);
    expect_value("sqrt(16)+log(100)", 6);

    // Standard-mode immediate semantics are deliberately left-to-right.
    expect_immediate("2+3*4", 20);
    expect_immediate("100+10%", 110);
    expect_immediate("100-10%", 90);
    expect_immediate("100*10%", 10);
    expect_immediate("100/10%", 1000);
    expect_immediate("2^3+1", 9);

    // Syntax, conversion and mathematical-domain failures.
    expect_error("");
    expect_error("1 / 0");
    expect_error("(1 + 2");
    expect_error("1 + foo");
    expect_error("1 +");
    expect_error("nan");
    expect_error("0x1p2");
    expect_error("1e9999");
    expect_error("1e-9999");
    expect_error("sqrt(-1)");
    expect_error("ln(0)");
    expect_error("acos(2)");
    expect_error("(-1)!");
    expect_error("2.5!");
    expect_error("171!");
    expect_error("madeup(1)");

    const auto sin_degrees = calculator::apply_real_function(
        calculator::RealFunction::Sin, 30.0, calculator::AngleUnit::Degrees);
    if (!sin_degrees.ok || std::abs(sin_degrees.value - 0.5) > 1e-12) {
        fail("shared degree sine transform wrong");
    }
    const auto sin_gradians = calculator::apply_real_function(
        calculator::RealFunction::Sin, 100.0, calculator::AngleUnit::Gradians);
    if (!sin_gradians.ok || std::abs(sin_gradians.value - 1.0) > 1e-12) {
        fail("shared gradian sine transform wrong");
    }
    const auto asin_gradians = calculator::apply_real_function(
        calculator::RealFunction::Asin, 1.0, calculator::AngleUnit::Gradians);
    if (!asin_gradians.ok || std::abs(asin_gradians.value - 100.0) > 1e-12) {
        fail("shared gradian inverse-sine transform wrong");
    }
    const auto reciprocal_zero = calculator::apply_real_function(
        calculator::RealFunction::Reciprocal, 0.0);
    if (reciprocal_zero.ok || reciprocal_zero.error != "division by zero") {
        fail("shared reciprocal zero contract wrong");
    }
    const auto invalid_root = calculator::apply_real_function(
        calculator::RealFunction::SquareRoot, -1.0);
    if (invalid_root.ok || invalid_root.error != "function domain error") {
        fail("shared square-root domain contract wrong");
    }

    std::string deeply_nested(300, '(');
    deeply_nested += "1";
    deeply_nested.append(300, ')');
    const auto deep_result = calculator::evaluate(deeply_nested);
    if (deep_result.ok || deep_result.error != "expression nesting too deep") {
        fail("deeply nested expression should fail deterministically");
    }

    // All graphical shells consume the same display-formatting contract.
    if (calculator::format_value(1.0 / 3.0) !=
        "0.333333333333333") {
        fail("shared value formatter wrong output");
    }
    if (calculator::format_scientific_value(1000.0).find("e+03") ==
        std::string::npos) {
        fail("scientific formatter wrong output");
    }
    if (calculator::format_engineering_value(12345.0) !=
        "12.345e+03") {
        fail("engineering formatter positive exponent wrong output");
    }
    if (calculator::format_engineering_value(0.00123) !=
        "1.23e-03") {
        fail("engineering formatter negative exponent wrong output");
    }
    if (calculator::format_engineering_value(0.0) != "0e+00") {
        fail("engineering formatter zero wrong output");
    }
    if (calculator::format_engineering_value(-0.00123) !=
        "-1.23e-03") {
        fail("engineering formatter negative value wrong output");
    }
    const std::string denormal_engineering =
        calculator::format_engineering_value(
            std::numeric_limits<double>::denorm_min());
    if (denormal_engineering.find("e-324") == std::string::npos ||
        denormal_engineering.find("inf") != std::string::npos ||
        denormal_engineering.find("nan") != std::string::npos) {
        fail("engineering formatter subnormal boundary wrong output");
    }
    const std::string maximum_engineering =
        calculator::format_engineering_value(
            std::numeric_limits<double>::max());
    if (maximum_engineering.find("e+306") == std::string::npos ||
        maximum_engineering.find("inf") != std::string::npos) {
        fail("engineering formatter maximum finite boundary wrong output");
    }
    if (calculator::format_engineering_value(999.9999999996) !=
        "1e+03") {
        fail("engineering formatter decimal carry boundary wrong output");
    }

    expect_value("9 mod 5", 4.0);
    expect_value("2**3", 8.0);
    expect_value("2(3+1)", 8.0);
    expect_value("(3+1)2", 8.0);
    expect_value("2pi", 2.0 * 3.14159265358979323846);
    expect_value("frac(3.25)", 0.25);
    expect_value("int(-3.75)", -3.0);
    expect_value("round(2.6)", 3.0);
    expect_value("sgn(-9)", -1.0);

    // GNOME/Mint-class keyboard syntax accepted by the shared parser.
    expect_value("7−3×2", 1.0);
    expect_value("8÷4", 2.0);
    expect_value("2π", 2.0 * 3.14159265358979323846);
    expect_value("τ/2", 3.14159265358979323846);
    expect_value("√81", 9.0);
    expect_value("|−12.5|", 12.5);
    expect_value("5²", 25.0);
    expect_value("2¹⁰", 1024.0);
    expect_value("2⁻³", 0.125);
    expect_value("sin 0", 0.0);
    expect_value("sqrt 49", 7.0);
    expect_value("sin⁻¹ 0.5", 3.14159265358979323846 / 6.0);
    expect_value("sinh⁻¹ 0", 0.0);
    expect_error("|1+2");
    expect_error("abs⁻¹ 2");

    calculator::Functions custom_functions;
    custom_functions["triple"] = {{"x"}, "x*3", "Triple a value"};
    const auto triple = calculator::evaluate(
        "triple(7)", calculator::Variables{}, custom_functions);
    if(!triple.ok || std::abs(triple.value-21.0)>1e-12)
        fail("direct custom-function evaluation failed");

    const auto random_value = calculator::evaluate("rand");
    if (!random_value.ok || random_value.value < 0.0 ||
        random_value.value >= 1.0) {
        fail("rand must produce a value in [0,1)");
    }

if (failures != 0) {
        std::cerr << failures << " calculator test(s) failed\n";
        return 1;
    }

    std::cout << "calculator core tests passed\n";
    return 0;
}
