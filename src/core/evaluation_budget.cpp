/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "evaluation_budget.hpp"

#include <limits>
#include <utility>

namespace calculator {

EvaluationBudget::EvaluationBudget(EvaluationLimits limits)
    : limits_(std::move(limits)) {
    if (limits_.timeout.count() > 0) {
        has_deadline_ = true;
        deadline_ = Clock::now() + limits_.timeout;
    }
}

bool EvaluationBudget::fail(const char* message) noexcept {
    if (error_.empty()) {
        try {
            error_ = message ? message : "evaluation resource limit reached";
        } catch (...) {
            // Resource checking must itself remain non-throwing. An empty
            // message still leaves the operation failed through the return.
        }
    }
    return false;
}

bool EvaluationBudget::check() noexcept {
    if (!error_.empty()) return false;
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
    if (limits_.max_input_bytes != 0U &&
        bytes > limits_.max_input_bytes) {
        return fail("input exceeds evaluation input limit");
    }
    return true;
}

bool EvaluationBudget::charge(std::uint64_t count) noexcept {
    if (!check()) return false;
    if (count == 0U) return true;
    if (limits_.max_steps != 0U) {
        if (steps_ > limits_.max_steps ||
            count > limits_.max_steps - steps_) {
            return fail("evaluation requires too many steps");
        }
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
    if (limits_.max_parse_depth != 0U &&
        prospective_depth > limits_.max_parse_depth) {
        return fail("expression nesting too deep");
    }
    return true;
}

bool EvaluationBudget::check_function_depth(
    std::size_t prospective_depth) noexcept {
    if (!check()) return false;
    if (limits_.max_function_depth != 0U &&
        prospective_depth > limits_.max_function_depth) {
        return fail("function recursion too deep");
    }
    return true;
}

bool EvaluationBudget::check_factorial(std::uint64_t value) noexcept {
    if (!check()) return false;
    if (limits_.max_factorial != 0U && value > limits_.max_factorial) {
        return fail("factorial input too large");
    }
    return true;
}

bool EvaluationBudget::check_power_exponent(
    std::uint64_t magnitude) noexcept {
    if (!check()) return false;
    if (limits_.max_power_exponent != 0U &&
        magnitude > limits_.max_power_exponent) {
        return fail("power exponent too large");
    }
    return true;
}

bool EvaluationBudget::check_output_digits(std::size_t digits) noexcept {
    if (!check()) return false;
    if (limits_.max_output_digits != 0U &&
        digits > limits_.max_output_digits) {
        return fail("result exceeds evaluation output limit");
    }
    return true;
}

void EvaluationBudget::cancel() noexcept {
    cancelled_.store(true, std::memory_order_relaxed);
}

} // namespace calculator
