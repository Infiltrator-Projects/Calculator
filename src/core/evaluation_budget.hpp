/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace calculator {

// Hard product safety ceiling for materialised arbitrary-precision decimal
// output. Callers may tighten this through EvaluationLimits but may not relax
// it: a caller-supplied budget is a restriction mechanism, not permission to
// bypass Calculator's product safety contract.
inline constexpr std::size_t kEvaluationMaxOutputDigits = 20000U;

// Resource policy shared by the multiprecision Scientific evaluator and the
// Advanced Tools operations that perform iterative or arbitrary-precision
// work. Binary64 Standard evaluation retains its separate, deliberately wider
// parser-depth contract; the two domains have different representations and
// workload characteristics and are not falsely collapsed into one parser.
struct EvaluationLimits {
    std::size_t max_input_bytes = 8192U;
    std::uint64_t max_steps = 5000000U;
    std::size_t max_parse_depth = 16U;
    std::size_t max_function_depth = 16U;
    std::uint64_t max_factorial = 100000U;
    std::uint64_t max_power_exponent = 100000000U;
    std::size_t max_output_digits = kEvaluationMaxOutputDigits;
    std::chrono::milliseconds timeout{0};
};

class EvaluationBudget {
public:
    explicit EvaluationBudget(EvaluationLimits limits = {});

    EvaluationBudget(const EvaluationBudget&) = delete;
    EvaluationBudget& operator=(const EvaluationBudget&) = delete;

    bool check() noexcept;
    bool check_input(std::size_t bytes) noexcept;
    bool charge(std::uint64_t steps = 1U) noexcept;
    bool check_parse_depth(std::size_t prospective_depth) noexcept;
    bool check_function_depth(std::size_t prospective_depth) noexcept;
    bool check_factorial(std::uint64_t value) noexcept;
    bool check_power_exponent(std::uint64_t magnitude) noexcept;
    bool check_output_digits(std::size_t digits) noexcept;

    void cancel() noexcept;

    const EvaluationLimits& limits() const noexcept { return limits_; }
    std::uint64_t steps() const noexcept { return steps_; }
    const std::string& error() const noexcept { return error_; }
    bool failed() const noexcept { return !error_.empty(); }

private:
    using Clock = std::chrono::steady_clock;

    EvaluationLimits limits_;
    Clock::time_point deadline_{};
    bool has_deadline_ = false;
    std::uint64_t steps_ = 0U;
    std::atomic_bool cancelled_{false};
    std::string error_;

    bool fail(const char* message) noexcept;
};

} // namespace calculator
