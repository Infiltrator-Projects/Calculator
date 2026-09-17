/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/calculator.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

static void expect_value(const char* expression, double expected) {
    const auto result = infiltrator::calc::evaluate(expression);
    assert(result.ok);
    assert(std::abs(result.value - expected) < 1e-12);
}

static void expect_error(const char* expression) {
    const auto result = infiltrator::calc::evaluate(expression);
    assert(!result.ok);
    assert(!result.error.empty());
}

int main() {
    expect_value("2 + 3 * 4", 14.0);
    expect_value("(2 + 3) * 4", 20.0);
    expect_value("2^3^2", 512.0);
    expect_value("-4 + 10", 6.0);
    expect_value("2.5 * 4", 10.0);
    expect_error("");
    expect_error("1 / 0");
    expect_error("(1 + 2");
    expect_error("1 + foo");
    std::cout << "calculator core tests passed\n";
    return 0;
}
