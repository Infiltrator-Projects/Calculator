/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "evaluation_budget.hpp"
#include "scientific.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace calculator {
namespace {

template <typename T>
void clamp_product_ceiling(T& value, T maximum) noexcept {
    // Zero traditionally means "unbounded" in many limit APIs. That meaning is
    // unsafe here: EvaluationLimits may narrow Calculator's product policy but
    // must never turn off the parser/work/input safety contract.
    if (value == T{} || value > maximum) value = maximum;
}

} // namespace

// scientific.cpp retains the parser implementation behind this explicit
// budget-aware internal entry. Public callers cross the wrapper below so
// input, deadline, cancellation, output and parser work share one policy.
ScientificResult evaluate_scientific_unchecked(
    const std::string& expression,
    const ScientificVariables& variables,
    const Functions& functions,
    AngleUnit angle_unit,
    unsigned decimal_digits,
    EvaluationBudget& budget);

EvaluationBudget::EvaluationBudget(EvaluationLimits limits)
    : limits_(std::move(limits)) {
    clamp_product_ceiling(
        limits_.max_input_bytes, kEvaluationMaxInputBytes);
    clamp_product_ceiling(
        limits_.max_steps, kEvaluationMaxSteps);
    clamp_product_ceiling(
        limits_.max_parse_depth, kEvaluationMaxParseDepth);
    clamp_product_ceiling(
        limits_.max_function_depth, kEvaluationMaxFunctionDepth);
    clamp_product_ceiling(
        limits_.max_factorial, kEvaluationMaxFactorial);
    clamp_product_ceiling(
        limits_.max_power_exponent, kEvaluationMaxPowerExponent);
    clamp_product_ceiling(
        limits_.max_output_digits, kEvaluationMaxOutputDigits);

    if (limits_.timeout.count() > 0) {
        has_deadline_ = true;
        const Clock::time_point now = Clock::now();
        const auto remaining = Clock::time_point::max() - now;
        const auto maximum_timeout =
            std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
        if (limits_.timeout >= maximum_timeout) {
            // Saturation preserves the requested "effectively unbounded in
            // practice" deadline without overflowing the clock's signed
            // duration representation.
            deadline_ = Clock::time_point::max();
        } else {
            deadline_ = now +
                std::chrono::duration_cast<Clock::duration>(limits_.timeout);
        }
    }
}

bool EvaluationBudget::fail(const char* message) noexcept {
    failed_ = true;
    if (error_.empty()) {
        try {
            error_ = message ? message : "evaluation resource limit reached";
        } catch (...) {
            // Failure state is independent of diagnostic allocation. If the
            // process cannot allocate the message, callers still observe a
            // failed budget and abort the operation rather than continuing.
        }
    }
    return false;
}

bool EvaluationBudget::check() noexcept {
    if (failed_) return false;
    if (cancelled_.load(std::memory_order_relaxed)) {
        return fail("evaluation cancelled");
    }
    if (has_deadline_ && Clock::now() >= deadline_) {
        return fail("evaluation timed out");
    }
    return true;
}

bool EvaluationBudget::check_input(std::size_t bytes) noexcept {
    if (!check()) return false;
    if (bytes > limits_.max_input_bytes) {
        return fail("input exceeds evaluation input limit");
    }
    return true;
}

bool EvaluationBudget::charge(std::uint64_t count) noexcept {
    if (!check()) return false;
    if (count == 0U) return true;
    if (steps_ > limits_.max_steps ||
        count > limits_.max_steps - steps_) {
        return fail("evaluation requires too many steps");
    }
    if (count > std::numeric_limits<std::uint64_t>::max() - steps_) {
        return fail("evaluation requires too many steps");
    }
    steps_ += count;
    return true;
}

bool EvaluationBudget::check_parse_depth(
    std::size_t prospective_depth) noexcept {
    if (!check()) return false;
    if (prospective_depth > limits_.max_parse_depth) {
        return fail("expression nesting too deep");
    }
    return true;
}

bool EvaluationBudget::check_function_depth(
    std::size_t prospective_depth) noexcept {
    if (!check()) return false;
    if (prospective_depth > limits_.max_function_depth) {
        return fail("function recursion too deep");
    }
    return true;
}

bool EvaluationBudget::check_factorial(std::uint64_t value) noexcept {
    if (!check()) return false;
    if (value > limits_.max_factorial) {
        return fail("factorial input too large");
    }
    return true;
}

bool EvaluationBudget::check_power_exponent(
    std::uint64_t magnitude) noexcept {
    if (!check()) return false;
    if (magnitude > limits_.max_power_exponent) {
        return fail("power exponent too large");
    }
    return true;
}

bool EvaluationBudget::check_output_digits(std::size_t digits) noexcept {
    if (!check()) return false;
    if (digits > limits_.max_output_digits) {
        return fail("result exceeds evaluation output limit");
    }
    return true;
}

void EvaluationBudget::cancel() noexcept {
    cancelled_.store(true, std::memory_order_relaxed);
}

ScientificResult evaluate_scientific(
    const std::string& expression,
    const ScientificVariables& variables,
    const Functions& functions,
    AngleUnit angle_unit,
    unsigned decimal_digits,
    EvaluationBudget* budget) {
    EvaluationBudget local_budget;
    EvaluationBudget& active_budget = budget ? *budget : local_budget;

    if (!active_budget.check_input(expression.size())) {
        return {false, {}, active_budget.error(), {}};
    }

    // Charge deterministic textual work here; repeated callers such as graph
    // and root search share this budget, so thousands of individually small
    // evaluations are bounded as one user operation rather than resetting on
    // every probe.
    const std::uint64_t textual_steps =
        static_cast<std::uint64_t>(expression.size()) + 1U;
    if (!active_budget.charge(textual_steps)) {
        return {false, {}, active_budget.error(), {}};
    }

    const unsigned effective_digits = std::max(
        kScientificMinDigits,
        std::min(decimal_digits, kScientificMaxDigits));
    if (!active_budget.check_output_digits(effective_digits)) {
        return {false, {}, active_budget.error(), {}};
    }

    ScientificResult result = evaluate_scientific_unchecked(
        expression, variables, functions, angle_unit,
        decimal_digits, active_budget);

    if (!active_budget.check()) {
        return {false, {}, active_budget.error(), {}};
    }
    return result;
}

} // namespace calculator
