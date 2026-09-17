/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>

namespace infiltrator::calc {

struct Result {
    bool ok = false;
    double value = 0.0;
    std::string error;
};

Result evaluate(const std::string& expression);

} // namespace infiltrator::calc
