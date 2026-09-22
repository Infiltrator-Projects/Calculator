/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/calculator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

namespace {

int failures = 0;

void fail(const std::string& message) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

std::uint64_t bits(double value) {
    std::uint64_t result = 0U;
    static_assert(sizeof(result) == sizeof(value));
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

bool same_bits(double left, double right) {
    return bits(left) == bits(right);
}

void expect_exact(const std::string& expression, double expected) {
    const auto actual = calculator::evaluate_standard(expression);
    if (!actual.ok) {
        fail(expression + " -> " + actual.error);
        return;
    }
    if (!same_bits(actual.value, expected)) {
        fail(expression + " produced a different binary64 value");
    }
}

void expect_close(const std::string& expression, double expected) {
    const auto actual = calculator::evaluate_standard(expression);
    if (!actual.ok) {
        fail(expression + " -> " + actual.error);
        return;
    }
    const double tolerance =
        8.0 * std::numeric_limits<double>::epsilon() *
        std::max(1.0, std::fabs(expected));
    if (std::fabs(actual.value - expected) > tolerance) {
        fail(expression + " produced the wrong numeric value");
    }
}

void expect_error(const std::string& expression) {
    const auto result = calculator::evaluate_standard(expression);
    if (result.ok || result.error.empty()) {
        fail(expression + " should fail");
    }
}

double apply(char op, double left, double right, bool& valid) {
    valid = true;
    switch (op) {
    case '+': return left + right;
    case '-': return left - right;
    case '*': return left * right;
    case '/':
        if (right == 0.0) {
            valid = false;
            return 0.0;
        }
        return left / right;
    default:
        valid = false;
        return 0.0;
    }
}

int precedence(char op) {
    return (op == '*' || op == '/') ? 2 : 1;
}

void check_three_term_precedence() {
    constexpr std::array<double, 7> values{
        -9.0, -3.0, -1.0, 1.0, 2.0, 5.0, 11.0
    };
    constexpr std::array<char, 4> operators{'+', '-', '*', '/'};

    for (double a : values) {
        for (double b : values) {
            for (double c : values) {
                for (char first : operators) {
                    for (char second : operators) {
                        bool valid = true;
                        double expected = 0.0;
                        if (precedence(second) > precedence(first)) {
                            bool inner_valid = true;
                            const double inner = apply(
                                second, b, c, inner_valid);
                            if (!inner_valid) continue;
                            expected = apply(
                                first, a, inner, valid);
                        } else {
                            bool inner_valid = true;
                            const double inner = apply(
                                first, a, b, inner_valid);
                            if (!inner_valid) continue;
                            expected = apply(
                                second, inner, c, valid);
                        }
                        if (!valid || !std::isfinite(expected)) continue;

                        const std::string expression =
                            calculator::serialize_value(a) + first +
                            calculator::serialize_value(b) + second +
                            calculator::serialize_value(c);
                        expect_exact(expression, expected);

                        bool left_valid = true;
                        const double left_inner =
                            apply(first, a, b, left_valid);
                        if (left_valid && std::isfinite(left_inner)) {
                            bool grouped_valid = true;
                            const double grouped = apply(
                                second, left_inner, c, grouped_valid);
                            if (grouped_valid && std::isfinite(grouped)) {
                                expect_exact(
                                    "(" + calculator::serialize_value(a) +
                                    first + calculator::serialize_value(b) +
                                    ")" + second +
                                    calculator::serialize_value(c),
                                    grouped);
                            }
                        }

                        bool right_valid = true;
                        const double right_inner =
                            apply(second, b, c, right_valid);
                        if (right_valid && std::isfinite(right_inner)) {
                            bool grouped_valid = true;
                            const double grouped = apply(
                                first, a, right_inner, grouped_valid);
                            if (grouped_valid && std::isfinite(grouped)) {
                                expect_exact(
                                    calculator::serialize_value(a) + first +
                                    "(" + calculator::serialize_value(b) +
                                    second + calculator::serialize_value(c) +
                                    ")",
                                    grouped);
                            }
                        }
                    }
                }
            }
        }
    }
}

void check_round_trip_serialization() {
    const std::array<double, 18> boundaries{
        0.0,
        -0.0,
        0.1,
        -0.1,
        1.0 / 3.0,
        -1.0 / 7.0,
        std::numeric_limits<double>::denorm_min(),
        -std::numeric_limits<double>::denorm_min(),
        std::numeric_limits<double>::min(),
        -std::numeric_limits<double>::min(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::lowest(),
        std::nextafter(1.0, 0.0),
        std::nextafter(1.0, 2.0),
        std::nextafter(0.0, 1.0),
        std::nextafter(0.0, -1.0),
        std::nextafter(
            std::numeric_limits<double>::max(), 0.0),
        std::nextafter(
            std::numeric_limits<double>::lowest(), 0.0)
    };

    for (double value : boundaries) {
        const std::string text = calculator::serialize_value(value);
        if (text.empty()) {
            fail("finite boundary value failed to serialize");
            continue;
        }
        const auto parsed = calculator::evaluate_standard(text);
        if (!parsed.ok || !same_bits(parsed.value, value)) {
            fail("finite boundary value did not round-trip exactly: " + text);
        }
    }

    std::uint64_t state = UINT64_C(0x8f3c6d9a2b4175e1);
    std::size_t checked = 0U;
    while (checked < 50000U) {
        // xorshift64* gives a deterministic broad sampling of binary64 bit
        // patterns without depending on the C library random implementation.
        state ^= state >> 12U;
        state ^= state << 25U;
        state ^= state >> 27U;
        const std::uint64_t raw =
            state * UINT64_C(2685821657736338717);
        double value = 0.0;
        std::memcpy(&value, &raw, sizeof(value));
        if (!std::isfinite(value)) continue;

        const std::string text = calculator::serialize_value(value);
        const auto parsed = calculator::evaluate_standard(text);
        if (text.empty() || !parsed.ok ||
            !same_bits(parsed.value, value)) {
            fail("random finite binary64 value failed exact decimal round-trip");
            return;
        }
        ++checked;
    }
}

} // namespace

int main() {
    // Conventional arithmetic precedence and associativity.
    expect_exact("200+200/2", 300.0);
    expect_exact("2+3*4", 14.0);
    expect_exact("(2+3)*4", 20.0);
    expect_exact("10-3-2", 5.0);
    expect_exact("10-(3-2)", 9.0);
    expect_exact("8/4*2", 4.0);
    expect_exact("8/(4*2)", 1.0);
    expect_exact("1--2", 3.0);
    expect_exact("3*-2", -6.0);

    // Exponentiation is right-associative and binds above unary signs.
    expect_exact("2^3^2", 512.0);
    expect_exact("(2^3)^2", 64.0);
    expect_exact("-2^2", -4.0);
    expect_exact("(-2)^2", 4.0);
    expect_exact("2^-2", 0.25);
    expect_exact("2^-2^2", 0.0625);

    // Postfix operators and implicit multiplication retain their mathematical
    // meaning even though Standard exposes a deliberately smaller keypad.
    expect_exact("50%", 0.5);
    expect_close("100+10%", 100.1);
    expect_exact("200*10%", 20.0);
    expect_exact("5!", 120.0);
    expect_exact("3!^2", 36.0);
    expect_exact("2(3+4)", 14.0);
    expect_exact("(2+3)4", 20.0);

    // Standard does not silently gain Scientific named terms.
    expect_error("pi");
    expect_error("e");
    expect_error("sqrt(9)");
    expect_error("9 mod 5");

    // Deterministic failure boundaries.
    expect_error("");
    expect_error("1+");
    expect_error("/2");
    expect_error("(1+2");
    expect_error("1/0");
    expect_error("0^-1");
    expect_error("(-1)^0.5");
    expect_error("2.5!");
    expect_error("171!");
    expect_error("1e309");
    expect_error("nan");
    expect_error("inf");
    expect_error("0x1p2");

    // Finite arithmetic overflow must fail rather than leak Inf/NaN; arithmetic
    // underflow is ordinary IEEE-754 binary64 behaviour and may round to zero.
    expect_error("1e308*10");
    expect_exact("1e-300*1e-300", 0.0);

    check_three_term_precedence();
    check_round_trip_serialization();

    if (failures != 0) {
        std::cerr << failures
                  << " Standard mathematical forensic test(s) failed\n";
        return 1;
    }
    std::cout
        << "Standard mathematical forensic tests passed: precedence, "
           "associativity, domains, postfix semantics and 50,000 exact "
           "binary64 decimal round-trips.\n";
    return 0;
}
