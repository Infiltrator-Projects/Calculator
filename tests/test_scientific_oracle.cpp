/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/scientific.hpp"

#include <mpc.h>
#include <mpfr.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr mpfr_prec_t kOracleBits = 8192;
int failures = 0;
std::size_t comparisons = 0;

class MpfrValue {
public:
    MpfrValue() { mpfr_init2(value_, kOracleBits); }
    ~MpfrValue() { mpfr_clear(value_); }
    MpfrValue(const MpfrValue&) = delete;
    MpfrValue& operator=(const MpfrValue&) = delete;
    mpfr_ptr get() { return value_; }
    mpfr_srcptr get() const { return value_; }
private:
    mpfr_t value_;
};

class MpcValue {
public:
    MpcValue() { mpc_init2(value_, kOracleBits); }
    ~MpcValue() { mpc_clear(value_); }
    MpcValue(const MpcValue&) = delete;
    MpcValue& operator=(const MpcValue&) = delete;
    mpc_ptr get() { return value_; }
    mpc_srcptr get() const { return value_; }
private:
    mpc_t value_;
};

void fail(std::string_view label, std::string_view detail) {
    std::cerr << "FAIL: " << label << ": " << detail << '\n';
    ++failures;
}

bool set_real(mpfr_ptr value, const std::string& text) {
    return mpfr_set_str(value, text.c_str(), 10, MPFR_RNDN) == 0;
}

bool set_complex(
    mpc_ptr value,
    const std::string& real,
    const std::string& imag) {
    if (!set_real(mpc_realref(value), real) ||
        !set_real(mpc_imagref(value), imag)) {
        return false;
    }
    return true;
}

std::string complex_expression(
    const std::string& real,
    const std::string& imag) {
    if (imag == "0") return real;
    return "(" + real + ")+(" + imag + ")*i";
}

bool close_component(
    const std::string& actual_text,
    mpfr_srcptr expected,
    unsigned digits,
    unsigned guard_digits = 20U) {
    MpfrValue actual;
    MpfrValue difference;
    MpfrValue scale;
    MpfrValue tolerance;

    if (!set_real(actual.get(), actual_text)) return false;

    mpfr_sub(
        difference.get(), actual.get(), expected, MPFR_RNDN);
    mpfr_abs(difference.get(), difference.get(), MPFR_RNDN);
    mpfr_abs(scale.get(), expected, MPFR_RNDN);
    if (mpfr_cmp_ui(scale.get(), 1U) < 0) {
        mpfr_set_ui(scale.get(), 1U, MPFR_RNDN);
    }

    const unsigned trustworthy =
        digits > guard_digits ? digits - guard_digits : 6U;
    mpfr_ui_pow_ui(
        tolerance.get(), 10U, trustworthy, MPFR_RNDN);
    mpfr_ui_div(
        tolerance.get(), 1U, tolerance.get(), MPFR_RNDN);
    mpfr_mul(
        tolerance.get(), tolerance.get(), scale.get(), MPFR_RNDN);
    return mpfr_cmp(difference.get(), tolerance.get()) <= 0;
}

void check_result(
    std::string_view label,
    const calculator::ScientificResult& actual,
    mpc_srcptr expected,
    unsigned digits,
    unsigned guard_digits = 20U) {
    ++comparisons;
    if (!actual.ok) {
        fail(label, actual.error);
        return;
    }

    const bool real_ok = close_component(
        actual.value.real, mpc_realref(expected),
        digits, guard_digits);
    const bool imag_ok = close_component(
        actual.value.imag, mpc_imagref(expected),
        digits, guard_digits);
    if (!real_ok || !imag_ok) {
        std::cerr << "FAIL: " << label
                  << " calculator="
                  << calculator::format_scientific_value(
                         actual.value, std::min(digits, 80U))
                  << '\n';
        ++failures;
    }
}

calculator::ScientificResult evaluate(
    const std::string& expression,
    unsigned digits = 100,
    calculator::AngleUnit unit = calculator::AngleUnit::Radians,
    const calculator::Functions& functions = {}) {
    return calculator::evaluate_scientific(
        expression, {}, functions, unit, digits);
}

using MpcUnary = int (*)(mpc_ptr, mpc_srcptr, mpc_rnd_t);

void check_mpc_unary(
    std::string_view function,
    const std::string& real,
    const std::string& imag,
    MpcUnary oracle,
    unsigned digits = 100) {
    MpcValue input;
    MpcValue expected;
    if (!set_complex(input.get(), real, imag)) {
        fail(function, "oracle input parse failed");
        return;
    }
    oracle(expected.get(), input.get(), MPC_RNDNN);
    const std::string expression =
        std::string(function) + "(" +
        complex_expression(real, imag) + ")";
    check_result(
        expression,
        evaluate(expression, digits),
        expected.get(), digits);
}

void check_binary(
    char op,
    const std::string& lr,
    const std::string& li,
    const std::string& rr,
    const std::string& ri,
    unsigned digits = 100) {
    MpcValue left;
    MpcValue right;
    MpcValue expected;
    if (!set_complex(left.get(), lr, li) ||
        !set_complex(right.get(), rr, ri)) {
        fail("binary", "oracle input parse failed");
        return;
    }

    if (op == '/' &&
        mpfr_zero_p(mpc_realref(right.get())) &&
        mpfr_zero_p(mpc_imagref(right.get()))) {
        return;
    }

    switch (op) {
    case '+':
        mpc_add(expected.get(), left.get(), right.get(), MPC_RNDNN);
        break;
    case '-':
        mpc_sub(expected.get(), left.get(), right.get(), MPC_RNDNN);
        break;
    case '*':
        mpc_mul(expected.get(), left.get(), right.get(), MPC_RNDNN);
        break;
    case '/':
        mpc_div(expected.get(), left.get(), right.get(), MPC_RNDNN);
        break;
    default:
        fail("binary", "unknown operator");
        return;
    }

    const std::string expression =
        "(" + complex_expression(lr, li) + ")" + op +
        "(" + complex_expression(rr, ri) + ")";
    check_result(
        expression, evaluate(expression, digits),
        expected.get(), digits, 15U);
}

std::string milli_text(int value) {
    const bool negative = value < 0;
    const unsigned magnitude =
        static_cast<unsigned>(negative ? -value : value);
    std::ostringstream out;
    if (negative) out << '-';
    out << magnitude / 1000U;
    const unsigned fraction = magnitude % 1000U;
    if (fraction != 0U) {
        out << '.' << std::setw(3) << std::setfill('0') << fraction;
        std::string text = out.str();
        while (!text.empty() && text.back() == '0') text.pop_back();
        return text;
    }
    return out.str();
}

void check_binary_fuzz() {
    std::uint64_t state = UINT64_C(0x4d595df4d0f33173);
    auto next = [&state]() {
        state ^= state >> 12U;
        state ^= state << 25U;
        state ^= state >> 27U;
        return state * UINT64_C(2685821657736338717);
    };

    constexpr std::array<char, 4> operations{'+', '-', '*', '/'};
    for (unsigned i = 0; i < 240U; ++i) {
        const int ar = static_cast<int>(next() % 10001U) - 5000;
        const int ai = static_cast<int>(next() % 10001U) - 5000;
        int br = static_cast<int>(next() % 10001U) - 5000;
        int bi = static_cast<int>(next() % 10001U) - 5000;
        if (br == 0 && bi == 0) br = 1;

        for (const char op : operations) {
            check_binary(
                op,
                milli_text(ar), milli_text(ai),
                milli_text(br), milli_text(bi));
        }
    }
}

void check_power(
    const std::string& br,
    const std::string& bi,
    const std::string& er,
    const std::string& ei,
    unsigned digits = 100) {
    MpcValue base;
    MpcValue exponent;
    MpcValue expected;
    if (!set_complex(base.get(), br, bi) ||
        !set_complex(exponent.get(), er, ei)) {
        fail("power", "oracle input parse failed");
        return;
    }
    mpc_pow(expected.get(), base.get(), exponent.get(), MPC_RNDNN);
    const std::string expression =
        "(" + complex_expression(br, bi) + ")^(" +
        complex_expression(er, ei) + ")";
    check_result(
        expression, evaluate(expression, digits),
        expected.get(), digits);
}

void check_function_matrix() {
    const std::array<std::pair<std::string, std::string>, 5> ordinary{{
        {"-2.25", "0"},
        {"-0.5", "0.125"},
        {"0", "0"},
        {"0.75", "-0.25"},
        {"2.5", "1.25"}
    }};

    for (const auto& [real, imag] : ordinary) {
        check_mpc_unary("sin", real, imag, mpc_sin);
        check_mpc_unary("cos", real, imag, mpc_cos);
        check_mpc_unary("tan", real, imag, mpc_tan);
        check_mpc_unary("sinh", real, imag, mpc_sinh);
        check_mpc_unary("cosh", real, imag, mpc_cosh);
        check_mpc_unary("tanh", real, imag, mpc_tanh);
        check_mpc_unary("asin", real, imag, mpc_asin);
        check_mpc_unary("acos", real, imag, mpc_acos);
        check_mpc_unary("atan", real, imag, mpc_atan);
        check_mpc_unary("asinh", real, imag, mpc_asinh);
        check_mpc_unary("acosh", real, imag, mpc_acosh);
        check_mpc_unary("atanh", real, imag, mpc_atanh);
        check_mpc_unary("sqrt", real, imag, mpc_sqrt);
        check_mpc_unary("exp", real, imag, mpc_exp);
        check_mpc_unary("ln", real, imag, mpc_log);
    }

    // log10(z), exp2(z), exp10(z), square(z), cube(z), abs(z), conj(z),
    // real(z), imag(z), sgn(z), roots and arbitrary powers use independently
    // composed MPFR/MPC references.
    for (const auto& [real, imag] : ordinary) {
        MpcValue input;
        if (!set_complex(input.get(), real, imag)) continue;

        {
            MpcValue log_value;
            MpcValue denominator;
            MpcValue expected;
            mpc_log(log_value.get(), input.get(), MPC_RNDNN);
            MpfrValue ten;
            mpfr_set_ui(ten.get(), 10U, MPFR_RNDN);
            mpfr_log(ten.get(), ten.get(), MPFR_RNDN);
            mpc_set_fr(denominator.get(), ten.get(), MPC_RNDNN);
            mpc_div(
                expected.get(), log_value.get(),
                denominator.get(), MPC_RNDNN);
            const std::string expression =
                "log(" + complex_expression(real, imag) + ")";
            check_result(
                expression, evaluate(expression),
                expected.get(), 100);
        }

        for (const auto& [name, base_ui] :
             std::array<std::pair<const char*, unsigned>, 2>{{
                 {"exp2", 2U}, {"exp10", 10U}}}) {
            MpcValue base;
            MpcValue expected;
            mpc_set_ui(base.get(), base_ui, MPC_RNDNN);
            mpc_pow(
                expected.get(), base.get(), input.get(), MPC_RNDNN);
            const std::string expression =
                std::string(name) + "(" +
                complex_expression(real, imag) + ")";
            check_result(
                expression, evaluate(expression),
                expected.get(), 100);
        }

        {
            MpcValue expected;
            mpc_mul(
                expected.get(), input.get(), input.get(), MPC_RNDNN);
            const std::string expression =
                "square(" + complex_expression(real, imag) + ")";
            check_result(
                expression, evaluate(expression),
                expected.get(), 100);
        }
        {
            MpcValue square;
            MpcValue expected;
            mpc_mul(square.get(), input.get(), input.get(), MPC_RNDNN);
            mpc_mul(
                expected.get(), square.get(), input.get(), MPC_RNDNN);
            const std::string expression =
                "cube(" + complex_expression(real, imag) + ")";
            check_result(
                expression, evaluate(expression),
                expected.get(), 100);
        }
        {
            MpfrValue magnitude;
            MpcValue expected;
            mpc_abs(magnitude.get(), input.get(), MPFR_RNDN);
            mpc_set_fr(expected.get(), magnitude.get(), MPC_RNDNN);
            const std::string expression =
                "abs(" + complex_expression(real, imag) + ")";
            check_result(
                expression, evaluate(expression),
                expected.get(), 100);
        }
        {
            MpcValue expected;
            mpc_conj(expected.get(), input.get(), MPC_RNDNN);
            const std::string expression =
                "conj(" + complex_expression(real, imag) + ")";
            check_result(
                expression, evaluate(expression),
                expected.get(), 100);
        }
        {
            MpcValue expected;
            mpc_set_fr(
                expected.get(), mpc_realref(input.get()), MPC_RNDNN);
            const std::string expression =
                "real(" + complex_expression(real, imag) + ")";
            check_result(
                expression, evaluate(expression),
                expected.get(), 100);
        }
        {
            MpcValue expected;
            mpc_set_fr(
                expected.get(), mpc_imagref(input.get()), MPC_RNDNN);
            const std::string expression =
                "imag(" + complex_expression(real, imag) + ")";
            check_result(
                expression, evaluate(expression),
                expected.get(), 100);
        }
        {
            MpcValue expected;
            if (mpfr_zero_p(mpc_realref(input.get())) &&
                mpfr_zero_p(mpc_imagref(input.get()))) {
                mpc_set_ui(expected.get(), 0U, MPC_RNDNN);
            } else {
                MpfrValue magnitude;
                MpcValue denominator;
                mpc_abs(magnitude.get(), input.get(), MPFR_RNDN);
                mpc_set_fr(
                    denominator.get(), magnitude.get(), MPC_RNDNN);
                mpc_div(
                    expected.get(), input.get(),
                    denominator.get(), MPC_RNDNN);
            }
            const std::string expression =
                "sgn(" + complex_expression(real, imag) + ")";
            check_result(
                expression, evaluate(expression),
                expected.get(), 100);
        }
    }

    // Real-only rounding and remainder contracts.
    for (const std::string value :
         {"-3.75", "-2.5", "-0.1", "0", "0.1", "2.5", "3.75"}) {
        MpfrValue input;
        if (!set_real(input.get(), value)) continue;

        for (const std::string name :
             {"floor", "ceil", "int", "round", "frac"}) {
            MpfrValue expected_real;
            if (name == "floor") {
                mpfr_floor(expected_real.get(), input.get());
            } else if (name == "ceil") {
                mpfr_ceil(expected_real.get(), input.get());
            } else if (name == "int") {
                mpfr_trunc(expected_real.get(), input.get());
            } else if (name == "round") {
                if (mpfr_sgn(input.get()) >= 0) {
                    MpfrValue half;
                    mpfr_set_d(half.get(), 0.5, MPFR_RNDN);
                    mpfr_add(
                        expected_real.get(), input.get(),
                        half.get(), MPFR_RNDN);
                    mpfr_floor(
                        expected_real.get(), expected_real.get());
                } else {
                    MpfrValue half;
                    mpfr_set_d(half.get(), 0.5, MPFR_RNDN);
                    mpfr_sub(
                        expected_real.get(), input.get(),
                        half.get(), MPFR_RNDN);
                    mpfr_ceil(
                        expected_real.get(), expected_real.get());
                }
            } else {
                MpfrValue integral;
                mpfr_trunc(integral.get(), input.get());
                mpfr_sub(
                    expected_real.get(), input.get(),
                    integral.get(), MPFR_RNDN);
            }

            MpcValue expected;
            mpc_set_fr(
                expected.get(), expected_real.get(), MPC_RNDNN);
            const std::string expression =
                name + "(" + value + ")";
            check_result(
                expression, evaluate(expression),
                expected.get(), 100, 10U);
        }
    }

    for (const std::pair<std::string, std::string>& values :
         std::array<std::pair<std::string, std::string>, 5>{{
             {"10.5", "3.25"},
             {"-10.5", "3.25"},
             {"10.5", "-3.25"},
             {"-10.5", "-3.25"},
             {"1e50", "3"}
         }}) {
        MpfrValue left;
        MpfrValue right;
        MpfrValue remainder;
        set_real(left.get(), values.first);
        set_real(right.get(), values.second);
        mpfr_fmod(
            remainder.get(), left.get(), right.get(), MPFR_RNDN);
        MpcValue expected;
        mpc_set_fr(expected.get(), remainder.get(), MPC_RNDNN);
        const std::string expression =
            values.first + " mod " + values.second;
        check_result(
            expression, evaluate(expression),
            expected.get(), 100, 10U);
    }

    // Integer factorial is compared with MPFR's independent factorial.
    for (const unsigned n : {0U, 1U, 10U, 50U, 100U, 1000U}) {
        MpfrValue factorial;
        mpfr_fac_ui(factorial.get(), n, MPFR_RNDN);
        MpcValue expected;
        mpc_set_fr(expected.get(), factorial.get(), MPC_RNDNN);
        const std::string expression = std::to_string(n) + "!";
        check_result(
            expression, evaluate(expression),
            expected.get(), 100, 20U);
    }

    // Numbered logarithm/root semantics.
    {
        MpfrValue x;
        MpfrValue numerator;
        MpfrValue denominator;
        set_real(x.get(), "32");
        mpfr_log(numerator.get(), x.get(), MPFR_RNDN);
        mpfr_set_ui(denominator.get(), 2U, MPFR_RNDN);
        mpfr_log(denominator.get(), denominator.get(), MPFR_RNDN);
        mpfr_div(
            numerator.get(), numerator.get(),
            denominator.get(), MPFR_RNDN);
        MpcValue expected;
        mpc_set_fr(expected.get(), numerator.get(), MPC_RNDNN);
        check_result(
            "log2(32)", evaluate("log2(32)"),
            expected.get(), 100);
    }
    {
        MpcValue input;
        MpcValue exponent;
        MpcValue expected;
        set_complex(input.get(), "2", "3");
        MpfrValue one_third;
        mpfr_set_ui(one_third.get(), 1U, MPFR_RNDN);
        mpfr_div_ui(
            one_third.get(), one_third.get(), 3U, MPFR_RNDN);
        mpc_set_fr(exponent.get(), one_third.get(), MPC_RNDNN);
        mpc_pow(
            expected.get(), input.get(), exponent.get(), MPC_RNDNN);
        check_result(
            "root3(2+3i)", evaluate("root3(2+3*i)"),
            expected.get(), 100);
    }
    {
        MpfrValue input;
        MpfrValue expected_real;
        set_real(input.get(), "-8");
        mpfr_cbrt(expected_real.get(), input.get(), MPFR_RNDN);
        MpcValue expected;
        mpc_set_fr(expected.get(), expected_real.get(), MPC_RNDNN);
        check_result(
            "cbrt(-8)", evaluate("cbrt(-8)"),
            expected.get(), 100);
        check_result(
            "root3(-8)", evaluate("root3(-8)"),
            expected.get(), 100);
    }

    // Representative arbitrary real/complex powers.
    for (const auto& values :
         std::array<std::array<const char*, 4>, 7>{{
             {{"2", "0", "0.5", "0"}},
             {{"2", "0", "-3.25", "0"}},
             {{"-2", "0.25", "0.5", "0"}},
             {{"1", "1", "2.5", "-1"}},
             {{"0.25", "-2", "-0.75", "0.5"}},
             {{"10", "3", "0", "2"}},
             {{"1e20", "0", "0.125", "0"}}
         }}) {
        check_power(
            values[0], values[1], values[2], values[3]);
    }
}

void check_branch_cuts() {
    constexpr const char* eps = "1e-40";
    constexpr const char* neg_eps = "-1e-40";

    for (const char* imag : {eps, neg_eps}) {
        check_mpc_unary("sqrt", "-2", imag, mpc_sqrt);
        check_mpc_unary("ln", "-2", imag, mpc_log);
        check_mpc_unary("asin", "2", imag, mpc_asin);
        check_mpc_unary("acos", "2", imag, mpc_acos);
        check_mpc_unary("acosh", "-2", imag, mpc_acosh);
        check_mpc_unary("atanh", "2", imag, mpc_atanh);
        check_mpc_unary("atan", "0.1", imag, mpc_atan);
        check_mpc_unary("asinh", "0.1", imag, mpc_asinh);
        check_power("-2", imag, "0.5", "0");
        check_power("-1", imag, "0.3", "0.2");
    }
}

void check_angle_units() {
    const std::array<std::string, 7> inputs{
        "-720", "-123.456", "0", "30", "45", "90", "1000000"
    };

    for (const auto& value : inputs) {
        MpfrValue real_input;
        MpfrValue pi;
        MpfrValue radians;
        set_real(real_input.get(), value);
        mpfr_const_pi(pi.get(), MPFR_RNDN);

        for (const auto unit :
             {calculator::AngleUnit::Degrees,
              calculator::AngleUnit::Gradians}) {
            const unsigned divisor =
                unit == calculator::AngleUnit::Degrees ? 180U : 200U;
            mpfr_mul(
                radians.get(), real_input.get(), pi.get(), MPFR_RNDN);
            mpfr_div_ui(
                radians.get(), radians.get(), divisor, MPFR_RNDN);

            for (const auto& fn :
                 std::array<std::pair<const char*, int>, 3>{{
                     {"sin", 0}, {"cos", 1}, {"tan", 2}}}) {
                MpfrValue expected_real;
                if (fn.second == 0) {
                    mpfr_sin(
                        expected_real.get(), radians.get(), MPFR_RNDN);
                } else if (fn.second == 1) {
                    mpfr_cos(
                        expected_real.get(), radians.get(), MPFR_RNDN);
                } else {
                    mpfr_tan(
                        expected_real.get(), radians.get(), MPFR_RNDN);
                }
                MpcValue expected;
                mpc_set_fr(
                    expected.get(), expected_real.get(), MPC_RNDNN);
                const std::string expression =
                    std::string(fn.first) + "(" + value + ")";
                check_result(
                    expression,
                    evaluate(expression, 100, unit),
                    expected.get(), 100);
            }
        }
    }

    for (const std::string value :
         {"-0.9", "-0.5", "0", "0.5", "0.9"}) {
        MpfrValue input;
        MpfrValue pi;
        set_real(input.get(), value);
        mpfr_const_pi(pi.get(), MPFR_RNDN);

        for (const auto unit :
             {calculator::AngleUnit::Degrees,
              calculator::AngleUnit::Gradians}) {
            const unsigned multiplier =
                unit == calculator::AngleUnit::Degrees ? 180U : 200U;

            for (const auto& fn :
                 std::array<std::pair<const char*, int>, 3>{{
                     {"asin", 0}, {"acos", 1}, {"atan", 2}}}) {
                MpfrValue radians;
                if (fn.second == 0) {
                    mpfr_asin(
                        radians.get(), input.get(), MPFR_RNDN);
                } else if (fn.second == 1) {
                    mpfr_acos(
                        radians.get(), input.get(), MPFR_RNDN);
                } else {
                    mpfr_atan(
                        radians.get(), input.get(), MPFR_RNDN);
                }
                MpfrValue expected_real;
                mpfr_mul_ui(
                    expected_real.get(), radians.get(),
                    multiplier, MPFR_RNDN);
                mpfr_div(
                    expected_real.get(), expected_real.get(),
                    pi.get(), MPFR_RNDN);
                MpcValue expected;
                mpc_set_fr(
                    expected.get(), expected_real.get(), MPC_RNDNN);
                const std::string expression =
                    std::string(fn.first) + "(" + value + ")";
                check_result(
                    expression,
                    evaluate(expression, 100, unit),
                    expected.get(), 100);
            }
        }
    }
}

void check_precision_sweep() {
    constexpr std::array<unsigned, 6> precisions{
        16U, 50U, 100U, 250U, 500U, 1000U
    };

    for (const unsigned digits : precisions) {
        const unsigned guard =
            digits >= 100U ? 25U : 8U;

        // pi
        {
            MpfrValue pi;
            mpfr_const_pi(pi.get(), MPFR_RNDN);
            MpcValue expected;
            mpc_set_fr(expected.get(), pi.get(), MPC_RNDNN);
            check_result(
                "precision pi",
                evaluate("pi", digits),
                expected.get(), digits, guard);
        }

        // e
        {
            MpfrValue one;
            MpfrValue e;
            mpfr_set_ui(one.get(), 1U, MPFR_RNDN);
            mpfr_exp(e.get(), one.get(), MPFR_RNDN);
            MpcValue expected;
            mpc_set_fr(expected.get(), e.get(), MPC_RNDNN);
            check_result(
                "precision e",
                evaluate("e", digits),
                expected.get(), digits, guard);
        }

        for (const auto& spec :
             std::array<std::pair<const char*, MpcUnary>, 4>{{
                 {"sqrt", mpc_sqrt},
                 {"sin", mpc_sin},
                 {"exp", mpc_exp},
                 {"ln", mpc_log}
             }}) {
            const std::string input =
                std::string(spec.first) +
                (std::string(spec.first) == "ln" ? "(2)" : "(1.23456789)");
            MpcValue operand;
            MpcValue expected;
            set_complex(
                operand.get(),
                std::string(spec.first) == "ln"
                    ? "2" : "1.23456789",
                "0");
            spec.second(
                expected.get(), operand.get(), MPC_RNDNN);
            check_result(
                "precision " + input,
                evaluate(input, digits),
                expected.get(), digits, guard);
        }

        {
            MpcValue input;
            MpcValue expected;
            set_complex(input.get(), "1.25", "-0.75");
            mpc_exp(expected.get(), input.get(), MPC_RNDNN);
            check_result(
                "precision complex exp",
                evaluate("exp(1.25-0.75*i)", digits),
                expected.get(), digits, guard);
        }

        {
            MpcValue base;
            MpcValue exponent;
            MpcValue expected;
            set_complex(base.get(), "1", "1");
            set_complex(exponent.get(), "2.5", "-1");
            mpc_pow(
                expected.get(), base.get(),
                exponent.get(), MPC_RNDNN);
            check_result(
                "precision complex power",
                evaluate("(1+i)^(2.5-i)", digits),
                expected.get(), digits, guard);
        }

        // Euler identity against an independently generated MPFR pi.
        {
            MpfrValue pi;
            mpfr_const_pi(pi.get(), MPFR_RNDN);
            MpcValue exponent;
            mpc_set_ui(exponent.get(), 0U, MPC_RNDNN);
            mpfr_set(
                mpc_imagref(exponent.get()), pi.get(), MPFR_RNDN);
            MpcValue expected;
            mpc_exp(
                expected.get(), exponent.get(), MPC_RNDNN);
            mpc_add_ui(
                expected.get(), expected.get(), 1U, MPC_RNDNN);
            check_result(
                "precision Euler identity",
                evaluate("exp(i*pi)+1", digits),
                expected.get(), digits, guard);
        }
    }
}

void check_constants() {
    // Mathematical constants.
    {
        MpfrValue pi;
        mpfr_const_pi(pi.get(), MPFR_RNDN);
        MpcValue expected;
        mpc_set_fr(expected.get(), pi.get(), MPC_RNDNN);
        check_result("pi", evaluate("pi"), expected.get(), 100);
    }
    {
        MpfrValue pi;
        mpfr_const_pi(pi.get(), MPFR_RNDN);
        mpfr_mul_ui(pi.get(), pi.get(), 2U, MPFR_RNDN);
        MpcValue expected;
        mpc_set_fr(expected.get(), pi.get(), MPC_RNDNN);
        check_result("tau", evaluate("tau"), expected.get(), 100);
    }
    {
        MpfrValue five;
        MpfrValue expected_real;
        mpfr_set_ui(five.get(), 5U, MPFR_RNDN);
        mpfr_sqrt(expected_real.get(), five.get(), MPFR_RNDN);
        mpfr_add_ui(
            expected_real.get(), expected_real.get(), 1U, MPFR_RNDN);
        mpfr_div_ui(
            expected_real.get(), expected_real.get(), 2U, MPFR_RNDN);
        MpcValue expected;
        mpc_set_fr(expected.get(), expected_real.get(), MPC_RNDNN);
        check_result("phi", evaluate("phi"), expected.get(), 100);
    }

    // SI-defined exact decimal constants are exact inputs, not binary64.
    for (const auto& item :
         std::array<std::pair<const char*, const char*>, 7>{{
             {"c0", "299792458"},
             {"h", "6.62607015e-34"},
             {"kB", "1.380649e-23"},
             {"NA", "6.02214076e23"},
             {"qe", "1.602176634e-19"},
             {"g0", "9.80665"},
             {"Rgas", "8.31446261815324"}
         }}) {
        MpcValue expected;
        if (!set_complex(expected.get(), item.second, "0")) {
            fail(item.first, "constant oracle parse failed");
            continue;
        }
        check_result(
            item.first, evaluate(item.first),
            expected.get(), 100, 5U);
    }
}

} // namespace

int main() {
    check_binary_fuzz();
    check_function_matrix();
    check_branch_cuts();
    check_angle_units();
    check_precision_sweep();
    check_constants();

    if (failures != 0) {
        std::cerr << failures
                  << " Scientific MPFR/MPC forensic oracle failure(s) across "
                  << comparisons << " independent comparisons\n";
        return 1;
    }

    std::cout
        << "Scientific MPFR/MPC forensic oracle passed "
        << comparisons
        << " independent comparisons covering arithmetic fuzz, every "
           "Scientific function family, complex branches, angle units, "
           "precision 16..1000 digits and constants.\n";
    return 0;
}
