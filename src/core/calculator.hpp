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

} // namespace infiltrator::calc
