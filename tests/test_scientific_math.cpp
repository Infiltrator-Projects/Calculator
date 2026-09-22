/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/scientific.hpp"

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

calculator::ScientificResult eval(
    const std::string& expression,
    calculator::AngleUnit unit = calculator::AngleUnit::Radians,
    unsigned digits = 100,
    const calculator::ScientificVariables& variables = {},
    const calculator::Functions& functions = {}) {
    return calculator::evaluate_scientific(
        expression, variables, functions, unit, digits);
}

std::string text(
    const calculator::ScientificResult& result,
    unsigned digits = 100) {
    if (!result.ok) return "ERROR:" + result.error;
    return calculator::format_scientific_value(result.value, digits);
}

void expect_text(
    const std::string& expression,
    const std::string& expected,
    unsigned digits = 100) {
    const auto result = eval(expression, calculator::AngleUnit::Radians, digits);
    if (!result.ok) {
        fail(expression + " -> " + result.error);
        return;
    }
    const std::string actual =
        calculator::format_scientific_value(result.value, digits);
    if (actual != expected) {
        fail(expression + " expected " + expected + " got " + actual);
    }
}

void expect_error(const std::string& expression) {
    const auto result = eval(expression);
    if (result.ok || result.error.empty()) {
        fail(expression + " should fail");
    }
}

bool component_below_power_of_ten(
    const std::string& component, int negative_power) {
    if (component == "0" || component == "-0") return true;

    const std::size_t exponent_position =
        component.find_first_of("eE");
    if (exponent_position != std::string::npos) {
        try {
            const int exponent =
                std::stoi(component.substr(exponent_position + 1U));
            return exponent < -negative_power;
        } catch (...) {
            return false;
        }
    }

    try {
        const long double value = std::stold(component);
        const long double tolerance =
            std::pow(10.0L, -static_cast<long double>(negative_power));
        return std::fabs(value) < tolerance;
    } catch (...) {
        return false;
    }
}

void expect_near_zero(
    const std::string& expression,
    int trustworthy_digits = 80) {
    const auto result = eval(expression, calculator::AngleUnit::Radians, 100);
    if (!result.ok ||
        !component_below_power_of_ten(
            result.value.real, trustworthy_digits) ||
        !component_below_power_of_ten(
            result.value.imag, trustworthy_digits)) {
        fail(expression + " identity residual too large: " + text(result, 30));
    }
}

} // namespace

int main() {
    // Grammar and exact arithmetic contracts.
    expect_text("200+200/2", "300");
    expect_text("2+3*4", "14");
    expect_text("(2+3)*4", "20");
    expect_text("10-3-2", "5");
    expect_text("10-(3-2)", "9");
    expect_text("8/4*2", "4");
    expect_text("8/(4*2)", "1");
    expect_text("2^3^2", "512");
    expect_text("(2^3)^2", "64");
    expect_text("-2^2", "-4");
    expect_text("(-2)^2", "4");
    expect_text("2^-2", "0.25");
    expect_text("2^-2^2", "0.0625");
    expect_text("1--2", "3");
    expect_text("3*-2", "-6");

    // Postfix, implicit multiplication and exact complex algebra.
    expect_text("50%", "0.5");
    expect_text("5!", "120");
    expect_text("3!^2", "36");
    expect_text("2(3+4)", "14");
    expect_text("(2+3)4", "20");
    expect_text("i*i", "-1");
    expect_text("(1+i)*(1-i)", "2");
    expect_text("(1+i)^2", "2i");
    expect_text("sqrt(-1)", "i");
    const auto branch_result = eval("ln(-1)");
    if (!branch_result.ok || branch_result.value.real != "0" ||
        branch_result.value.imag.rfind(
            "3.141592653589793238462643383279", 0) != 0U) {
        fail("ln(-1) must use the +pi principal branch");
    }
    expect_text("(-1)^0.5", "i");
    expect_text("cbrt(-8)", "-2");
    expect_text("root3(-8)", "-2");
    expect_text("log2(32)", "5");
    expect_text("|3+4*i|", "5");
    expect_text("conj(3+4*i)", "3 - 4i");
    expect_text("real(3+4*i)", "3");
    expect_text("imag(3+4*i)", "4");
    expect_text("abs(3+4*i)", "5");

    // Real-only rounding semantics are explicit and deterministic.
    expect_text("floor(2.9)", "2");
    expect_text("floor(-2.1)", "-3");
    expect_text("ceil(2.1)", "3");
    expect_text("ceil(-2.9)", "-2");
    expect_text("round(2.5)", "3");
    expect_text("round(-2.5)", "-3");
    expect_text("int(-2.9)", "-2");
    expect_text("frac(-2.75)", "-0.75");
    expect_text("sgn(0)", "0");
    expect_text("sgn(-2)", "-1");
    expect_text("sgn(3+4*i)", "0.6 + 0.8i");

    // Unicode/convenience spellings are syntax aliases only.
    expect_text("π-π", "0");
    expect_text("τ-2*pi", "0");
    expect_text("₃√−8", "-2");
    expect_text("log₂ 32", "5");
    expect_text("√9", "3");

    // Angle-unit semantics.
    auto result = eval(
        "sin(30)", calculator::AngleUnit::Degrees, 100);
    if (!result.ok || text(result) != "0.5")
        fail("sin(30 degrees) must be 0.5");
    result = eval(
        "sin(50)", calculator::AngleUnit::Gradians, 100);
    if (!result.ok || text(result) !=
            "0.7071067811865475244008443621048490392848359376884740365883398689953662392310535194251931026619645724") {
        // The exact decimal is irrational; require the independently familiar
        // leading digits here and leave full precision to MPFR/MPC CI.
        if (!result.ok ||
            result.value.real.rfind("0.707106781186547524400844362104", 0) != 0U) {
            fail("sin(50 grad) must match sqrt(2)/2");
        }
    }
    result = eval(
        "asin(0.5)", calculator::AngleUnit::Degrees, 100);
    if (!result.ok || result.value.real.rfind("30", 0) != 0U)
        fail("asin(0.5) degrees must be 30");
    result = eval(
        "asin(0.5)", calculator::AngleUnit::Gradians, 100);
    if (!result.ok || result.value.real.rfind("33.333333333333", 0) != 0U)
        fail("asin(0.5) gradians must be 100/3");

    // High-precision identities. These are not the independent oracle; they
    // catch internal algebra/angle inconsistencies on every supported runner.
    expect_near_zero("sin(1)^2+cos(1)^2-1");
    expect_near_zero("exp(ln(2))-2");
    expect_near_zero("ln(exp(0.75))-0.75");
    expect_near_zero("square(sqrt(2))-2");
    expect_near_zero("exp(i*pi)+1");
    expect_near_zero(
        "real((3+4*i)*conj(3+4*i))-abs(3+4*i)^2");

    // User functions are mathematical factoring, not a hidden decimal-rounding
    // boundary inside one expression.
    calculator::Functions functions;
    functions["f"] = {{"x"}, "sin(x)+ln(x+2)", {}};
    functions["g"] = {{"x"}, "f(x)*f(x)", {}};
    result = eval(
        "f(1.23456789)-(sin(1.23456789)+ln(3.23456789))",
        calculator::AngleUnit::Radians, 80, {}, functions);
    if (!result.ok || result.value.real != "0" || result.value.imag != "0") {
        fail("custom function factoring introduced precision loss");
    }
    result = eval(
        "g(0.75)-square(sin(0.75)+ln(2.75))",
        calculator::AngleUnit::Radians, 80, {}, functions);
    if (!result.ok || result.value.real != "0" || result.value.imag != "0") {
        fail("nested custom function factoring introduced precision loss");
    }

    // ScientificValue is the computational state boundary. Re-expression of a
    // retained value must be stable at its configured precision.
    result = eval("(sqrt(2)+i*pi)/7", calculator::AngleUnit::Radians, 120);
    if (!result.ok) {
        fail("complex state fixture failed");
    } else {
        const auto round_trip = eval(
            calculator::scientific_value_expression(result.value),
            calculator::AngleUnit::Radians, 120);
        if (!round_trip.ok ||
            round_trip.value.real != result.value.real ||
            round_trip.value.imag != result.value.imag) {
            fail("ScientificValue expression round-trip changed state");
        }

        const std::string before_real = result.value.real;
        const std::string before_imag = result.value.imag;
        (void)calculator::format_scientific_display(
            result.value, calculator::ScientificDisplayFormat::Fixed,
            3, true, true);
        if (result.value.real != before_real ||
            result.value.imag != before_imag) {
            fail("display formatting mutated ScientificValue state");
        }
    }

    // Requested precision clamps are part of the public numeric contract.
    const auto low = eval("1/7", calculator::AngleUnit::Radians, 1);
    const auto min = eval("1/7", calculator::AngleUnit::Radians, 16);
    if (!low.ok || !min.ok || low.value.real != min.value.real)
        fail("Scientific precision must clamp below 16 digits");
    const auto high = eval("1/7", calculator::AngleUnit::Radians, 5000);
    const auto max = eval("1/7", calculator::AngleUnit::Radians, 1000);
    if (!high.ok || !max.ok || high.value.real != max.value.real)
        fail("Scientific precision must clamp above 1000 digits");
    if (max.value.real.size() < 950U)
        fail("1000-digit Scientific result retained too few digits");

    // Explicit bridge to binary64 must test numeric imaginary zero, not a
    // particular textual spelling of zero.
    double bridge = 0.0;
    if (!calculator::scientific_value_to_double({"1.25", "0.0"}, bridge) ||
        bridge != 1.25) {
        fail("numeric zero imaginary component should bridge to binary64");
    }
    if (calculator::scientific_value_to_double({"1.25", "1e-100"}, bridge))
        fail("non-zero imaginary component must not bridge to binary64");

    // Domain, syntax and resource boundaries.
    expect_error("");
    expect_error("1/");
    expect_error("(1+2");
    expect_error("|1+2");
    expect_error("1/0");
    expect_error("10 mod 0");
    expect_error("(1+i) mod 2");
    expect_error("(-1)!");
    expect_error("2.5!");
    expect_error("100001!");
    expect_error("root0(2)");
    expect_error("log1(2)");
    expect_error("tan(pi/2)");
    {
        const auto degree_pole = eval(
            "tan(90)", calculator::AngleUnit::Degrees, 100);
        if (degree_pole.ok) fail("tan(90 degrees) must be undefined");
        const auto grad_pole = eval(
            "tan(100)", calculator::AngleUnit::Gradians, 100);
        if (grad_pole.ok) fail("tan(100 gradians) must be undefined");
    }
    expect_error("unknown(2)");

    std::string nested(300, '(');
    nested += "1";
    nested.append(300, ')');
    result = eval(nested);
    if (result.ok || result.error != "expression nesting too deep")
        fail("parser nesting limit must fail deterministically");

    calculator::Functions recursive;
    recursive["loop"] = {{"x"}, "loop(x)", {}};
    result = eval(
        "loop(1)", calculator::AngleUnit::Radians, 80, {}, recursive);
    if (result.ok || result.error != "function recursion too deep")
        fail("custom function recursion limit must fail deterministically");

    if (failures != 0) {
        std::cerr << failures
                  << " Scientific forensic mathematical test(s) failed\n";
        return 1;
    }

    std::cout
        << "Scientific forensic mathematical tests passed: grammar, "
           "precedence, complex algebra, angle units, identities, precision "
           "boundaries, state isolation and deterministic failures.\n";
    return 0;
}
