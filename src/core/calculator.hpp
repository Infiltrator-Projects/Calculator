/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>
#include <unordered_map>

namespace calculator {

using Variables = std::unordered_map<std::string, double>;

struct Result {
    bool ok = false;
    double value = 0.0;
    std::string error;
};

enum class AngleUnit {
    Radians,
    Degrees,
    Gradians
};

enum class RealFunction {
    Square,
    Cube,
    SquareRoot,
    Reciprocal,
    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Sinh,
    Cosh,
    Tanh,
    Asinh,
    Acosh,
    Atanh,
    Cbrt,
    Ln,
    Log10,
    Exp,
    TwoPower,
    TenPower,
    Abs,
    Floor,
    Ceil
};

// UI display contract for the real-number domain. Uses locale-independent
// general notation with 15 significant digits so every platform presents the
// same binary64 result.
std::string format_value(double value);
std::string format_scientific_value(double value);

Result evaluate(const std::string& expression);
Result evaluate(const std::string& expression, const Variables& variables);

// Standard calculator semantics: apply binary operations from left to right.
// Contextual percentages follow conventional desktop-calculator behaviour
// (100 + 10% -> 110, 100 * 10% -> 10).
Result evaluate_immediate(const std::string& expression);

// Canonical real-valued unary/scientific transform used by both expression
// evaluation and interactive controls. AngleUnit affects only trigonometric
// and inverse-trigonometric functions.
Result apply_real_function(RealFunction function, double value,
                           AngleUnit angle_unit = AngleUnit::Radians);

} // namespace calculator
