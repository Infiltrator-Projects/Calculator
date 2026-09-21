/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "calculator.hpp"

#include <string>
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

ScientificResult evaluate_scientific(
    const std::string& expression,
    const ScientificVariables& variables = ScientificVariables{},
    const Functions& functions = Functions{},
    AngleUnit angle_unit = AngleUnit::Radians,
    unsigned decimal_digits = kScientificDefaultDigits);

std::string scientific_value_expression(const ScientificValue& value);
std::string format_scientific_value(
    const ScientificValue& value,
    unsigned decimal_digits = kScientificDefaultDigits,
    bool scientific_notation = false);

bool scientific_value_to_double(
    const ScientificValue& value, double& output) noexcept;
ScientificValue scientific_value_from_double(double value);

} // namespace calculator
