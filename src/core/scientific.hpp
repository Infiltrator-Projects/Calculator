/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "calculator.hpp"

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

namespace calculator {

// Scientific mode keeps values as decimal text at the session boundary so the
// public C++ interface never exposes a third-party multiprecision ABI. The
// implementation reconstructs these components directly into the high-
// precision backend; no binary64 conversion occurs on that path.
struct ScientificValue {
    std::string real = "0";
    std::string imag = "0";
};

using ScientificVariables =
    std::unordered_map<std::string, ScientificValue>;

struct ScientificResult {
    bool ok = false;
    ScientificValue value{};
    std::string error;
    std::string display;
};

constexpr unsigned kScientificDefaultDigits = 50;
constexpr unsigned kScientificMaxDigits = 1000;

enum class ScientificDisplayFormat {
    General,
    Fixed,
    Scientific,
    Engineering
};

// Process-lifetime catalogue of identifiers accepted as Scientific functions.
const std::array<std::string_view, 31>&
scientific_function_catalog() noexcept;

// Evaluate the Scientific grammar entirely in the maintained multiprecision
// real/complex domain. decimal_digits is clamped to the supported precision
// interval; ordinary syntax/domain failures are reported in ScientificResult.
ScientificResult evaluate_scientific(
    const std::string& expression,
    const ScientificVariables& variables = ScientificVariables{},
    const Functions& functions = Functions{},
    AngleUnit angle_unit = AngleUnit::Radians,
    unsigned decimal_digits = kScientificDefaultDigits);

// Produce a lossless expression spelling suitable for feeding the value back
// into the Scientific parser without a binary64 round-trip.
std::string scientific_value_expression(const ScientificValue& value);

// Presentation helpers never change the stored ScientificValue. Formatting is
// locale-independent and operates directly on its decimal components.
std::string format_scientific_value(
    const ScientificValue& value,
    unsigned decimal_digits = kScientificDefaultDigits,
    bool scientific_notation = false);
std::string format_engineering_value(
    const ScientificValue& value,
    unsigned significant_digits = 13);
std::string format_scientific_display(
    const ScientificValue& value,
    ScientificDisplayFormat format,
    unsigned precision,
    bool trailing_zeroes,
    bool group_thousands);

// Explicit bridge to binary64 for callers that genuinely require it. Returns
// false for non-real, non-finite or out-of-range values rather than silently
// discarding an imaginary component or overflow.
bool scientific_value_to_double(
    const ScientificValue& value, double& output) noexcept;

// Exact textual construction from the supplied binary64 value; this does not
// imply that later Scientific arithmetic is limited to binary64 precision.
ScientificValue scientific_value_from_double(double value);

} // namespace calculator
