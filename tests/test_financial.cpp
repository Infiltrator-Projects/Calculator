/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/advanced_tools.hpp"
#include "../src/core/evaluation_budget.hpp"

#include <charconv>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {
int failures = 0;
void check(bool condition, std::string_view label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

bool close(double actual, double expected) {
    if (expected == 0.0) return actual == 0.0;
    // A unit-sized absolute tolerance would conceal every tiny false zero.
    return actual != 0.0 && std::fabs(actual - expected) <=
        64.0 * std::numeric_limits<double>::epsilon() * std::fabs(expected) +
        std::numeric_limits<double>::denorm_min();
}

struct Reference {
    const char* input;
    double expected;
};

// Independent Python Decimal references at 800 digits. Annuities use direct
// sums of individual payments; small DDB periods use the book-value recurrence.
// Transcendental/extreme fixtures use Decimal ln/exp, never Calculator's backend.
// Boundary fixtures widen their parsed inputs with Decimal.from_float so the
// oracle preserves binary64 values rather than reinterpreting decimal spelling.
// Expected values are rounded to binary64; output has 15 significant digits.
constexpr Reference references[] = {
    {"fv 25 0 12", 300.0},
    {"pv 25 0 12", 300.0},
    {"pmt 1000 0 12", 83.33333333333333},
    {"fv 25 .005 12", 308.38905932249787},
    {"pv 25 .005 12", 290.47330167041025},
    {"pmt 1000 .005 12", 86.06642970708066},
    {"fv 25 -.01 12", 284.0378207096768},
    {"pv 25 -.01 12", 320.4452487549244},
    {"pmt 1000 -.01 12", 78.01644773057605},
    {"fv 25 1e-20 12", 300.0},
    {"pv 25 1e-20 12", 300.0},
    {"pmt 1000 1e-20 12", 83.33333333333333},
    {"fv 25 -1e-20 12", 300.0},
    {"pv 25 -1e-20 12", 300.0},
    {"pmt 1000 -1e-20 12", 83.33333333333333},
    {"fv 25 1e-300 12", 300.0},
    {"pv 25 1e-300 12", 300.0},
    {"pmt 1000 1e-300 12", 83.33333333333333},
    {"pv 1 1e-20 360", 360.0},
    {"pmt 1000 1e-20 360", 2.7777777777777777},
    {"fv 1e300 1e-300 1e-300", 1.0},
    {"pv 1e300 1e-300 1e-300", 1.0},
    {"pmt 1e-300 1e-300 1e-300", 1.0},
    {"syd 100 0 1e200 1", 2e-198},
    {"syd 1e308 -1e308 1e308 1", 4.0},
    {"syd 10000 1000 5 3", 1800.0},
    {"sln 1e308 -1e308 1e308", 2.0},
    {"gpm 80 0.2", 100.0},
    {"gpm 1e300 -1e300", 1.0},
    {"ctrm .01 200 100", 69.66071689357489},
    {"ctrm 1e-300 2 1", 6.931471805599454e+299},
    {"ctrm -.5 1e-300 1e300", 1993.1568569324174},
    {"rate 200 100 12", 0.05946309435929526},
    {"rate 1e300 1e-300 2", 1e+300},
    {"term 100 1e-20 .05", 1.0247967157143935e-22},
    {"term 1e300 1e-20 1e-300", 1e-320},
    {"term 100 1000 0", 10.0},
    {"ddb 10000 1 1", 20000.0},
    {"ddb 10000 2 1", 10000.0},
    {"ddb 10000 2 2", 0.0},
    {"ddb 10000 5 1", 4000.0},
    {"ddb 10000 5 2", 2400.0},
    {"ddb 10000 5 3", 1440.0},
    {"ddb 10000 5 4", 864.0},
    {"ddb 10000 10 1", 2000.0},
    {"ddb 10000 10 2", 1600.0},
    {"ddb 10000 10 3", 1280.0},
    {"ddb 10000 10 4", 1024.0},
    {"ddb 1 1e12 1e12", 2.7067056647322537e-13},
    {"ddb 1 1e20 1e20", 2.7067056647322537e-21},
    {"ddb 1 1e308 1e308", 2.706705664732253e-309},
    {"ddb 1e308 1e308 1e308", 0.2706705664732254},
    {"fv 0 1e308 1e308", 0.0},
    {"fv 1 1e308 0", 0.0},
    {"pv 0 -.99 1e308", 0.0},
    {"pv 1 .1 0", 0.0},
    {"pmt 0 -.99 1e308", 0.0},
    {"term 1 0 .05", 0.0},
    {"ddb 0 1e20 1e20", 0.0},
    {"ddb 1 2 2", 0.0},
    {"fv 1 5e-324 2", 2.0},
    {"pv 1 5e-324 360", 360.0},
    {"sln 1.0000000000000002 1 1", 2.220446049250313e-16},
    {"syd 1.0000000000000002 1 2 1", 1.4802973661668753e-16},
    {"rate 1.0000000000000002 1 1", 2.220446049250313e-16},
    {"ctrm 1e-16 1.0000000000000002 1", 2.220446049250313},
    {"pmt 1e-300 0 5e-324", 2.0240225330731062e+23},
    {"fv 1e300 0 5e-324", 4.940656458412466e-24},
    {"pv 1e300 0 5e-324", 4.940656458412466e-24},
    {"ddb 10000 2.0000000000000004 2", 2.2204460492503123e-12},
};
} // namespace

int main() {
    using calculator::tools::AdvancedTool;
    using calculator::tools::evaluate;
    for (const auto& reference : references) {
        const auto result = evaluate(AdvancedTool::Financial, reference.input);
        constexpr std::string_view prefix = "Result  ";
        if (!result.ok || result.output.compare(0, prefix.size(), prefix) != 0) {
            check(false, reference.input);
            std::cerr << "  " << result.error << '\n';
            continue;
        }
        double actual = 0.0;
        const char* begin = result.output.data() + prefix.size();
        const char* end = result.output.data() + result.output.size();
        const auto parsed = std::from_chars(begin, end, actual);
        const bool matches = parsed.ec == std::errc{} && parsed.ptr == end &&
                             std::isfinite(actual) && close(actual, reference.expected);
        check(matches, reference.input);
        if (!matches) std::cerr << "  observed: " << result.output
                                << "; expected: " << reference.expected << '\n';
    }

    for (const char* input : {
             "ddb 1 10 1.5", "ddb 1 10 0", "ddb 1 10 11", "ddb -1 10 1",
             "fv 1 -1 2", "pv 1 -1 2", "pmt 1 0 0", "ctrm 0 2 1",
             "rate 0 1 1", "gpm 1 1", "term 1 2 -.5", "term 1 3 -.5",
             "sln 1 0 0", "syd 1 0 5 0", "syd 1 0 5 6", "fv nan 0 1",
             "pmt 1e-308 0 1e308", "gpm 1e-300 -1e300",
             "fv 1e-300 0 1e-300", "pv 1e-300 0 1e-300",
             "fv 1e308 1e308 2", "pmt 1e308 1e308 2",
             "rate 1.0000000000000002 1 1e308"}) {
        check(!evaluate(AdvancedTool::Financial, input).ok, input);
    }

    calculator::EvaluationLimits limits;
    limits.max_steps = 10U;
    calculator::EvaluationBudget bounded(limits);
    const auto exhausted = evaluate(AdvancedTool::Financial, "ddb 100 10 5", &bounded);
    check(!exhausted.ok && bounded.failed() &&
          exhausted.error == "evaluation requires too many steps",
          "Financial evaluation consumes the caller's work budget");

    calculator::EvaluationBudget cancelled;
    cancelled.cancel();
    const auto stopped = evaluate(AdvancedTool::Financial, "ddb 1 1e20 1e20", &cancelled);
    check(!stopped.ok && stopped.error == "evaluation cancelled",
          "Financial evaluation respects cancellation");

    if (failures != 0) return 1;
    std::cout << "Financial references, range and budget checks passed.\n";
    return 0;
}
