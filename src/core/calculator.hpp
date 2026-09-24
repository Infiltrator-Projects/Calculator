/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace calculator {

using Variables = std::unordered_map<std::string, double>;

struct FunctionDefinition {
    std::vector<std::string> parameters;
    std::string expression;
    std::string description;
};
using Functions = std::unordered_map<std::string, FunctionDefinition>;

enum class ConstantKind {
    Decimal,
    Pi,
    Euler,
    Tau,
    Phi,
    ReducedPlanck
};

struct ConstantInfo {
    std::string_view name;
    std::string_view alias;
    double value = 0.0;
    std::string_view unit;
    ConstantKind kind = ConstantKind::Decimal;
    // Canonical decimal spelling for decimal-defined constants. Mathematical
    // constants deliberately leave this empty so Scientific can construct
    // them at the requested precision.
    std::string_view exact_decimal;
};

// Process-lifetime catalogue of Calculator-owned constants. Returned views
// refer to static storage and remain valid for the life of the process.
const std::array<ConstantInfo, 17>& constant_catalog() noexcept;

// True only for names reserved by the Calculator expression grammar. This is
// the canonical guard used by variable/function definition paths.
bool is_builtin_function_name(std::string_view name) noexcept;

struct Result {
    bool ok = false;
    double value = 0.0;
    std::string error;
    // Optional user-facing result text for successful non-numeric session
    // operations such as defining a reusable function.
    std::string display;
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
// general notation with 15 significant digits. This is presentation only and
// must never be fed back into computation as a substitute for the binary64
// value.
std::string format_value(double value);
std::string format_scientific_value(double value);
std::string format_engineering_value(double value);

// Canonical finite-binary64 state serialization. The returned decimal is the
// shortest locale-independent representation that round-trips to the exact
// same binary64 value through Calculator/Common parsing. Returns an empty
// string for a non-finite input.
std::string serialize_value(double value);

// Evaluate the mathematical expression grammar in the binary64 domain.
// Overloads progressively add variable/function scope; failures are returned
// through Result rather than thrown for ordinary syntax/domain errors.
Result evaluate(const std::string& expression);
Result evaluate(const std::string& expression, const Variables& variables);
Result evaluate(const std::string& expression, const Variables& variables,
                const Functions& functions,
                AngleUnit angle_unit = AngleUnit::Radians);

// Standard-mode arithmetic uses the same conventional mathematical operator
// precedence as the expression engine, but rejects named constants, variables
// and functions. Postfix '%' has its literal mathematical meaning: divide the
// preceding value by 100.
Result evaluate_standard(const std::string& expression);

// Canonical real-valued unary/scientific transform used by both expression
// evaluation and interactive controls. AngleUnit affects only trigonometric
// and inverse-trigonometric functions.
Result apply_real_function(RealFunction function, double value,
                           AngleUnit angle_unit = AngleUnit::Radians);

} // namespace calculator
