/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>
#include <unordered_map>

namespace infiltrator::calc {

using Variables = std::unordered_map<std::string, double>;

struct Result {
    bool ok = false;
    double value = 0.0;
    std::string error;
};

Result evaluate(const std::string& expression);
Result evaluate(const std::string& expression, const Variables& variables);

// Standard calculator semantics: apply binary operations from left to right.
// Contextual percentages follow conventional desktop-calculator behaviour
// (100 + 10% -> 110, 100 * 10% -> 10).
Result evaluate_immediate(const std::string& expression);

} // namespace infiltrator::calc
