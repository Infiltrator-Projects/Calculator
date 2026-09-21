/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/scientific.hpp"

#include <mpc.h>
#include <mpfr.h>

#include <iostream>
#include <string>
#include <string_view>

namespace {

constexpr mpfr_prec_t kOracleBits = 4096;
constexpr unsigned kCalculatorDigits = 100;
constexpr unsigned kToleranceDigits = 85;
int failures = 0;

void fail(std::string_view label, std::string_view detail) {
    std::cerr << "FAIL: " << label << ": " << detail << '\n';
    ++failures;
}

bool close_component(
    const std::string& actual_text,
    mpfr_srcptr expected) {
    mpfr_t actual;
    mpfr_t difference;
    mpfr_t scale;
    mpfr_t tolerance;
    mpfr_inits2(
        kOracleBits, actual, difference, scale, tolerance,
        static_cast<mpfr_ptr>(nullptr));

    const bool parsed =
        mpfr_set_str(
            actual, actual_text.c_str(), 10, MPFR_RNDN) == 0;
    bool close = false;
    if (parsed) {
        mpfr_sub(difference, actual, expected, MPFR_RNDN);
        mpfr_abs(difference, difference, MPFR_RNDN);
        mpfr_abs(scale, expected, MPFR_RNDN);
        if (mpfr_cmp_ui(scale, 1U) < 0) {
            mpfr_set_ui(scale, 1U, MPFR_RNDN);
        }

        mpfr_ui_pow_ui(
            tolerance, 10U, kToleranceDigits, MPFR_RNDN);
        mpfr_ui_div(
            tolerance, 1U, tolerance, MPFR_RNDN);
        mpfr_mul(
            tolerance, tolerance, scale, MPFR_RNDN);
        close = mpfr_cmp(difference, tolerance) <= 0;
    }

    mpfr_clears(
        tolerance, scale, difference, actual,
        static_cast<mpfr_ptr>(nullptr));
    return close;
}

bool check_complex(
    std::string_view label,
    const calculator::ScientificResult& actual,
    mpc_srcptr expected) {
    if (!actual.ok) {
        fail(label, actual.error);
        return false;
    }

    const bool real_ok =
        close_component(actual.value.real, mpc_realref(expected));
    const bool imag_ok =
        close_component(actual.value.imag, mpc_imagref(expected));
    if (!real_ok || !imag_ok) {
        std::cerr << "FAIL: " << label
                  << ": Calculator = "
                  << calculator::format_scientific_value(
                         actual.value, kCalculatorDigits)
                  << '\n';
        ++failures;
        return false;
    }
    return true;
}

void check_real_expression(
    std::string_view label,
    const std::string& expression,
    void (*reference)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t),
    const char* input) {
    const auto actual = calculator::evaluate_scientific(
        expression, {}, {}, calculator::AngleUnit::Radians,
        kCalculatorDigits);
    if (!actual.ok) {
        fail(label, actual.error);
        return;
    }
    if (actual.value.imag != "0") {
        fail(label, "unexpected complex result");
        return;
    }

    mpfr_t x;
    mpfr_t expected;
    mpfr_inits2(
        kOracleBits, x, expected,
        static_cast<mpfr_ptr>(nullptr));
    if (mpfr_set_str(x, input, 10, MPFR_RNDN) != 0) {
        fail(label, "oracle input parse failed");
    } else {
        reference(expected, x, MPFR_RNDN);
        if (!close_component(actual.value.real, expected)) {
            fail(label, "outside MPFR precision tolerance");
        }
    }
    mpfr_clears(
        expected, x, static_cast<mpfr_ptr>(nullptr));
}

void mpfr_log_adapter(
    mpfr_ptr output, mpfr_srcptr input, mpfr_rnd_t rounding) {
    mpfr_log(output, input, rounding);
}

void mpfr_sin_adapter(
    mpfr_ptr output, mpfr_srcptr input, mpfr_rnd_t rounding) {
    mpfr_sin(output, input, rounding);
}

void mpfr_sqrt_adapter(
    mpfr_ptr output, mpfr_srcptr input, mpfr_rnd_t rounding) {
    mpfr_sqrt(output, input, rounding);
}

void mpfr_exp_adapter(
    mpfr_ptr output, mpfr_srcptr input, mpfr_rnd_t rounding) {
    mpfr_exp(output, input, rounding);
}

} // namespace

int main() {
    check_real_expression(
        "sin(1)", "sin(1)", mpfr_sin_adapter, "1");
    check_real_expression(
        "ln(2)", "ln(2)", mpfr_log_adapter, "2");
    check_real_expression(
        "sqrt(2)", "sqrt(2)", mpfr_sqrt_adapter, "2");
    check_real_expression(
        "exp(1)", "exp(1)", mpfr_exp_adapter, "1");

    mpc_t input;
    mpc_t exponent;
    mpc_t expected;
    mpc_init2(input, kOracleBits);
    mpc_init2(exponent, kOracleBits);
    mpc_init2(expected, kOracleBits);

    mpc_set_d_d(input, -2.0, 0.0, MPC_RNDNN);
    mpc_sqrt(expected, input, MPC_RNDNN);
    check_complex(
        "sqrt(-2)",
        calculator::evaluate_scientific(
            "sqrt(-2)", {}, {},
            calculator::AngleUnit::Radians,
            kCalculatorDigits),
        expected);

    mpc_set_d_d(input, 1.0, 1.0, MPC_RNDNN);
    mpc_exp(expected, input, MPC_RNDNN);
    check_complex(
        "exp(1+i)",
        calculator::evaluate_scientific(
            "exp(1+i)", {}, {},
            calculator::AngleUnit::Radians,
            kCalculatorDigits),
        expected);

    mpc_set_d_d(input, 1.0, 1.0, MPC_RNDNN);
    mpc_set_d_d(exponent, 2.5, -1.0, MPC_RNDNN);
    mpc_pow(expected, input, exponent, MPC_RNDNN);
    check_complex(
        "(1+i)^(2.5-i)",
        calculator::evaluate_scientific(
            "(1+i)^(2.5-i)", {}, {},
            calculator::AngleUnit::Radians,
            kCalculatorDigits),
        expected);

    mpc_clear(expected);
    mpc_clear(exponent);
    mpc_clear(input);

    if (failures != 0) {
        std::cerr << failures
                  << " high-precision oracle test(s) failed\n";
        return 1;
    }

    std::cout
        << "MPFR/MPC independent high-precision oracle checks passed\n";
    return 0;
}
