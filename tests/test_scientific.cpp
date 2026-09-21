/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/scientific.hpp"

#include <iostream>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

calculator::ScientificResult evaluate(
    const std::string& expression,
    calculator::AngleUnit angle = calculator::AngleUnit::Radians,
    unsigned digits = 80) {
    static const calculator::ScientificVariables variables;
    static const calculator::Functions functions;
    return calculator::evaluate_scientific(
        expression, variables, functions, angle, digits);
}
}

int main() {
    using calculator::format_scientific_value;

    auto result = evaluate("0.1+0.2");
    check(result.ok, "exact decimal addition evaluates");
    check(format_scientific_value(result.value, 80) == "0.3",
          "0.1 + 0.2 must not expose binary64 rounding");

    result = evaluate("1/7");
    check(result.ok, "high precision division evaluates");
    const std::string seventh =
        format_scientific_value(result.value, 80);
    check(seventh.size() > 60U,
          "scientific value retains substantially more than binary64");
    check(seventh.rfind("0.142857142857142857142857142857", 0) == 0U,
          "1/7 high precision prefix");

    result = evaluate("sqrt(-1)");
    check(result.ok, "negative square root promotes to complex");
    check(format_scientific_value(result.value, 80) == "i",
          "sqrt(-1) is i");

    result = evaluate("(1+i)^2");
    check(result.ok, "complex powers evaluate");
    check(result.value.imag.rfind("2", 0) == 0U,
          "(1+i)^2 has imaginary component 2");

    result = evaluate("sin(pi/6)");
    check(result.ok, "transcendental expression evaluates");
    check(format_scientific_value(result.value, 80) == "0.5",
          "sin(pi/6) high precision result");

    result = evaluate("sin(30)", calculator::AngleUnit::Degrees);
    check(result.ok, "degree trigonometry evaluates");
    check(format_scientific_value(result.value, 80) == "0.5",
          "sin(30 degrees)");

    result = evaluate("1e1000*1e1000");
    check(result.ok, "large exponent arithmetic evaluates");
    check(format_scientific_value(result.value, 80) == "1e+2000",
          "large finite result is not binary64 overflow");

    result = evaluate("1e-1000/1e1000");
    check(result.ok, "tiny exponent arithmetic evaluates");
    check(format_scientific_value(result.value, 80) == "1e-2000",
          "tiny finite result is not binary64 underflow");

    result = evaluate("₃√−8");
    check(result.ok, "Mint unicode root spelling evaluates");
    check(format_scientific_value(result.value, 80) == "-2",
          "odd root preserves real negative result");

    result = evaluate("log₂ 32");
    check(result.ok, "Mint unicode logarithm spelling evaluates");
    check(format_scientific_value(result.value, 80) == "5",
          "log base 2 result");

    calculator::ScientificVariables variables;
    variables["z"] = {"1.25", "-2.5"};
    const calculator::Functions functions;
    result = calculator::evaluate_scientific(
        "z*2", variables, functions,
        calculator::AngleUnit::Radians, 80);
    check(result.ok, "complex variable evaluates without binary64");
    check(result.value.real == "2.5" && result.value.imag == "-5",
          "complex variable components preserved");

    if (failures != 0) {
        std::cerr << failures
                  << " scientific precision test(s) failed\n";
        return 1;
    }
    std::cout << "scientific precision tests passed\n";
    return 0;
}
