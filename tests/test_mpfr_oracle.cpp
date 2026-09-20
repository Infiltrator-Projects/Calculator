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

    const double scale =
        std::max(std::fabs(actual), std::fabs(expected));
    const double relative_tolerance =
        32.0 * std::numeric_limits<double>::epsilon() * scale;
    const double subnormal_tolerance =
        32.0 * std::numeric_limits<double>::denorm_min();
    return std::fabs(actual - expected) <=
           std::max(relative_tolerance, subnormal_tolerance);
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
    Exp,
    TwoPower,
    TenPower,
    Sinh,
    Cosh,
    Tanh,
    Asinh,
    Acosh,
    Atanh
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
    case OracleFunction::TwoPower: mpfr_exp2(y, x, MPFR_RNDN); break;
    case OracleFunction::TenPower: mpfr_ui_pow(y, 10UL, x, MPFR_RNDN); break;
    case OracleFunction::Sinh: mpfr_sinh(y, x, MPFR_RNDN); break;
    case OracleFunction::Cosh: mpfr_cosh(y, x, MPFR_RNDN); break;
    case OracleFunction::Tanh: mpfr_tanh(y, x, MPFR_RNDN); break;
    case OracleFunction::Asinh: mpfr_asinh(y, x, MPFR_RNDN); break;
    case OracleFunction::Acosh: mpfr_acosh(y, x, MPFR_RNDN); break;
    case OracleFunction::Atanh: mpfr_atanh(y, x, MPFR_RNDN); break;
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
    case OracleFunction::TwoPower: return RealFunction::TwoPower;
    case OracleFunction::TenPower: return RealFunction::TenPower;
    case OracleFunction::Sinh: return RealFunction::Sinh;
    case OracleFunction::Cosh: return RealFunction::Cosh;
    case OracleFunction::Tanh: return RealFunction::Tanh;
    case OracleFunction::Asinh: return RealFunction::Asinh;
    case OracleFunction::Acosh: return RealFunction::Acosh;
    case OracleFunction::Atanh: return RealFunction::Atanh;
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
    case OracleFunction::TwoPower: return "exp2";
    case OracleFunction::TenPower: return "exp10";
    case OracleFunction::Sinh: return "sinh";
    case OracleFunction::Cosh: return "cosh";
    case OracleFunction::Tanh: return "tanh";
    case OracleFunction::Asinh: return "asinh";
    case OracleFunction::Acosh: return "acosh";
    case OracleFunction::Atanh: return "atanh";
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
    constexpr std::array<double, 9> power_exponent{
        -300.0, -100.0, -10.0, -1.0, 0.0,
        1.0, 10.0, 100.0, 300.0
    };
    constexpr std::array<double, 7> acosh_domain{
        1.0, 1.000001, 1.1, 2.0, 10.0, 1.0e3, 1.0e6
    };
    constexpr std::array<double, 9> atanh_domain{
        -0.99, -0.9, -0.5, -0.1, 0.0,
        0.1, 0.5, 0.9, 0.99
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
    check_values(OracleFunction::TwoPower, power_exponent);
    check_values(OracleFunction::TenPower, power_exponent);
    check_values(OracleFunction::Sinh, general);
    check_values(OracleFunction::Cosh, general);
    check_values(OracleFunction::Tanh, general);
    check_values(OracleFunction::Asinh, general);
    check_values(OracleFunction::Acosh, acosh_domain);
    check_values(OracleFunction::Atanh, atanh_domain);

    if (failures != 0) {
        std::cerr << failures << " MPFR oracle test(s) failed\n";
        return 1;
    }

    std::cout << "MPFR high-precision oracle checks passed\n";
    return 0;
}
