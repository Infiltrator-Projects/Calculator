/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace calculator {

// Shared deterministic work limits for calculator evaluation domains. A zero
// limit means unlimited. The budget intentionally measures work rather than
// wall-clock time so core behaviour is reproducible in tests and across native
// platforms; callers may cancel an in-flight budget explicitly.
struct EvaluationLimits {
    std::size_t max_input_bytes = 8192U;
    std::uint64_t max_steps = 1000000U;
    std::uint64_t max_factorial = 100000U;
    std::uint64_t max_power_exponent = 100000U;
    std::size_t max_output_digits = 20000U;
};

class EvaluationBudget {
public:
    explicit EvaluationBudget(EvaluationLimits limits = {}) noexcept
        : limits_(limits) {}

    const EvaluationLimits& limits() const noexcept { return limits_; }
    std::uint64_t steps() const noexcept { return steps_; }
    bool ok() const noexcept { return error_.empty() && !cancelled_; }
    const std::string& error() const noexcept { return error_; }

    void cancel() noexcept { cancelled_ = true; }

    bool check() {
        if (!error_.empty()) return false;
        if (cancelled_) return fail("evaluation cancelled");
        return true;
    }

    bool check_input(std::size_t bytes) {
        if (!check()) return false;
        if (limits_.max_input_bytes != 0U && bytes > limits_.max_input_bytes) {
            return fail("expression exceeds evaluation input budget");
        }
        return true;
    }

    bool charge(std::uint64_t count = 1U) {
        if (!check()) return false;
        if (limits_.max_steps == 0U) return true;
        if (count > limits_.max_steps -
                        std::min(limits_.max_steps, steps_)) {
            steps_ = limits_.max_steps;
            return fail("evaluation requires too many steps");
        }
        steps_ += count;
        return true;
    }

    bool check_factorial(std::uint64_t value) {
        if (!check()) return false;
        if (limits_.max_factorial != 0U && value > limits_.max_factorial) {
            return fail("factorial input exceeds evaluation budget");
        }
        return true;
    }

    bool check_power_exponent(std::uint64_t value) {
        if (!check()) return false;
        if (limits_.max_power_exponent != 0U &&
            value > limits_.max_power_exponent) {
            return fail("power exponent exceeds evaluation budget");
        }
        return true;
    }

    bool check_output_digits(std::size_t digits) {
        if (!check()) return false;
        if (limits_.max_output_digits != 0U &&
            digits > limits_.max_output_digits) {
            return fail("result exceeds evaluation output budget");
        }
        return true;
    }

private:
    bool fail(std::string message) {
        if (error_.empty()) error_ = std::move(message);
        return false;
    }

    EvaluationLimits limits_{};
    std::uint64_t steps_ = 0U;
    bool cancelled_ = false;
    std::string error_;
};

} // namespace calculator
