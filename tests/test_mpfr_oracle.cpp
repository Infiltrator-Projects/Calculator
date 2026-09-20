/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/calculator.hpp"

#include <mpfr.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

int failures = 0;

void fail(std::string_view name, double input, double actual, double expected) {
    std::cerr << "FAIL: " << name << "(" << input << ") = " << actual
              << ", MPFR reference " << expected << '\n';
    ++failures;
}

bool close_to_reference(double actual, double expected) {
    if (actual == expected) return true;
    if (!std::isfinite(actual) || !std::isfinite(expected)) return false;

    const double scale = std::max(1.0, std::fabs(expected));
    const double tolerance =
        32.0 * std::numeric_limits<double>::epsilon() * scale;
    return std::fabs(actual - expected) <= tolerance;
}

enum class OracleFunction {
    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Sqrt,
    Cbrt,
    Ln,
    Log10,
    Exp
};

double mpfr_reference(OracleFunction function, double input) {
    mpfr_t x;
    mpfr_t y;
    mpfr_init2(x, 256);
    mpfr_init2(y, 256);
    mpfr_set_d(x, input, MPFR_RNDN);

    switch (function) {
    case OracleFunction::Sin: mpfr_sin(y, x, MPFR_RNDN); break;
    case OracleFunction::Cos: mpfr_cos(y, x, MPFR_RNDN); break;
    case OracleFunction::Tan: mpfr_tan(y, x, MPFR_RNDN); break;
    case OracleFunction::Asin: mpfr_asin(y, x, MPFR_RNDN); break;
    case OracleFunction::Acos: mpfr_acos(y, x, MPFR_RNDN); break;
    case OracleFunction::Atan: mpfr_atan(y, x, MPFR_RNDN); break;
    case OracleFunction::Sqrt: mpfr_sqrt(y, x, MPFR_RNDN); break;
    case OracleFunction::Cbrt: mpfr_cbrt(y, x, MPFR_RNDN); break;
    case OracleFunction::Ln: mpfr_log(y, x, MPFR_RNDN); break;
    case OracleFunction::Log10: mpfr_log10(y, x, MPFR_RNDN); break;
    case OracleFunction::Exp: mpfr_exp(y, x, MPFR_RNDN); break;
    }

    const double result = mpfr_get_d(y, MPFR_RNDN);
    mpfr_clear(y);
    mpfr_clear(x);
    return result;
}

calculator::RealFunction calculator_function(OracleFunction function) {
    using calculator::RealFunction;
    switch (function) {
    case OracleFunction::Sin: return RealFunction::Sin;
    case OracleFunction::Cos: return RealFunction::Cos;
    case OracleFunction::Tan: return RealFunction::Tan;
    case OracleFunction::Asin: return RealFunction::Asin;
    case OracleFunction::Acos: return RealFunction::Acos;
    case OracleFunction::Atan: return RealFunction::Atan;
    case OracleFunction::Sqrt: return RealFunction::SquareRoot;
    case OracleFunction::Cbrt: return RealFunction::Cbrt;
    case OracleFunction::Ln: return RealFunction::Ln;
    case OracleFunction::Log10: return RealFunction::Log10;
    case OracleFunction::Exp: return RealFunction::Exp;
    }
    return RealFunction::Abs;
}

std::string_view name(OracleFunction function) {
    switch (function) {
    case OracleFunction::Sin: return "sin";
    case OracleFunction::Cos: return "cos";
    case OracleFunction::Tan: return "tan";
    case OracleFunction::Asin: return "asin";
    case OracleFunction::Acos: return "acos";
    case OracleFunction::Atan: return "atan";
    case OracleFunction::Sqrt: return "sqrt";
    case OracleFunction::Cbrt: return "cbrt";
    case OracleFunction::Ln: return "ln";
    case OracleFunction::Log10: return "log10";
    case OracleFunction::Exp: return "exp";
    }
    return "unknown";
}

template <std::size_t N>
void check_values(OracleFunction function, const std::array<double, N>& values) {
    for (double input : values) {
        const auto actual = calculator::apply_real_function(
            calculator_function(function), input,
            calculator::AngleUnit::Radians);
        if (!actual.ok) {
            std::cerr << "FAIL: " << name(function) << "(" << input
                      << ") unexpectedly failed: " << actual.error << '\n';
            ++failures;
            continue;
        }

        const double expected = mpfr_reference(function, input);
        if (!close_to_reference(actual.value, expected)) {
            fail(name(function), input, actual.value, expected);
        }
    }
}

} // namespace

int main() {
    constexpr std::array<double, 11> general{
        -10.0, -3.0, -1.0, -0.5, -0.1,
        0.0, 0.1, 0.5, 1.0, 3.0, 10.0
    };
    constexpr std::array<double, 9> inverse{
        -1.0, -0.9, -0.5, -0.1, 0.0,
        0.1, 0.5, 0.9, 1.0
    };
    constexpr std::array<double, 8> positive{
        1.0e-12, 1.0e-6, 0.1, 0.5,
        1.0, 2.0, 10.0, 1.0e6
    };
    constexpr std::array<double, 9> exponent{
        -20.0, -10.0, -1.0, -0.1, 0.0,
        0.1, 1.0, 10.0, 20.0
    };

    check_values(OracleFunction::Sin, general);
    check_values(OracleFunction::Cos, general);
    check_values(OracleFunction::Tan, general);
    check_values(OracleFunction::Atan, general);
    check_values(OracleFunction::Asin, inverse);
    check_values(OracleFunction::Acos, inverse);
    check_values(OracleFunction::Sqrt, positive);
    check_values(OracleFunction::Cbrt, general);
    check_values(OracleFunction::Ln, positive);
    check_values(OracleFunction::Log10, positive);
    check_values(OracleFunction::Exp, exponent);

    if (failures != 0) {
        std::cerr << failures << " MPFR oracle test(s) failed\n";
        return 1;
    }

    std::cout << "MPFR high-precision oracle checks passed\n";
    return 0;
}
