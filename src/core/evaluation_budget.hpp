/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace calculator {

// Product safety ceilings for multiprecision/iterative evaluation. A custom
// EvaluationLimits object is a restriction mechanism: callers may choose a
// smaller positive bound but cannot disable or raise these ceilings. Keeping
// the hard maxima explicit prevents an embedding surface from accidentally
// bypassing parser-stack, input-size or computational-work invariants that the
// ordinary Calculator UI relies on.
inline constexpr std::size_t kEvaluationMaxInputBytes = 8192U;
inline constexpr std::uint64_t kEvaluationMaxSteps = 5000000U;
inline constexpr std::size_t kEvaluationMaxParseDepth = 16U;
inline constexpr std::size_t kEvaluationMaxFunctionDepth = 16U;
inline constexpr std::uint64_t kEvaluationMaxFactorial = 100000U;
inline constexpr std::uint64_t kEvaluationMaxPowerExponent = 100000000U;
inline constexpr std::size_t kEvaluationMaxOutputDigits = 20000U;

// Resource policy shared by the multiprecision Scientific evaluator and the
// Advanced Tools operations that perform iterative or arbitrary-precision
// work. Binary64 Standard evaluation retains its separate, deliberately wider
// parser-depth contract; the two domains have different representations and
// workload characteristics and are not falsely collapsed into one parser.
struct EvaluationLimits {
    std::size_t max_input_bytes = kEvaluationMaxInputBytes;
    std::uint64_t max_steps = kEvaluationMaxSteps;
    std::size_t max_parse_depth = kEvaluationMaxParseDepth;
    std::size_t max_function_depth = kEvaluationMaxFunctionDepth;
    std::uint64_t max_factorial = kEvaluationMaxFactorial;
    std::uint64_t max_power_exponent = kEvaluationMaxPowerExponent;
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
    bool failed() const noexcept { return failed_; }

private:
    using Clock = std::chrono::steady_clock;

    EvaluationLimits limits_;
    Clock::time_point deadline_{};
    bool has_deadline_ = false;
    std::uint64_t steps_ = 0U;
    std::atomic_bool cancelled_{false};
    bool failed_ = false;
    std::string error_;

    bool fail(const char* message) noexcept;
};

} // namespace calculator
