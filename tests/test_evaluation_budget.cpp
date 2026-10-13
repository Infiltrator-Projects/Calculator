/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/advanced_tools.hpp"
#include "../src/core/evaluation_budget.hpp"
#include "../src/core/scientific.hpp"

#include <chrono>
#include <iostream>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

int main() {
    using calculator::EvaluationBudget;
    using calculator::EvaluationLimits;

    EvaluationLimits input_limits;
    input_limits.max_input_bytes = 4U;
    EvaluationBudget input_budget(input_limits);
    auto scientific = calculator::evaluate_scientific(
        "12345", {}, {}, calculator::AngleUnit::Radians,
        calculator::kScientificDefaultDigits, &input_budget);
    check(!scientific.ok, "Scientific rejects input beyond the shared byte limit");
    check(input_budget.failed(), "oversized Scientific input exhausts its budget");
    check(input_budget.error() == "input exceeds evaluation input limit",
          "input ceiling keeps its explicit failure diagnostic");

    EvaluationLimits depth_limits;
    depth_limits.max_parse_depth = 2U;
    EvaluationBudget depth_budget(depth_limits);
    scientific = calculator::evaluate_scientific(
        "---1", {}, {}, calculator::AngleUnit::Radians,
        calculator::kScientificDefaultDigits, &depth_budget);
    check(!scientific.ok, "Scientific parser consumes the shared parse-depth budget");
    check(depth_budget.error() == "expression nesting too deep",
          "Scientific parse-depth failure is reported by the budget");

    EvaluationLimits exact_depth_limits;
    exact_depth_limits.max_parse_depth = 3U;
    EvaluationBudget exact_depth_budget(exact_depth_limits);
    const auto exact_depth = calculator::tools::evaluate(
        calculator::tools::AdvancedTool::ExactDecimal,
        "----1", &exact_depth_budget);
    check(!exact_depth.ok && exact_depth_budget.failed(),
          "exact-decimal unary recursion consumes parse-depth budget");

    EvaluationBudget integer_depth_budget(exact_depth_limits);
    const auto integer_depth = calculator::tools::evaluate(
        calculator::tools::AdvancedTool::ArbitraryPrecision,
        "2^2^2^2", &integer_depth_budget);
    check(!integer_depth.ok && integer_depth_budget.failed(),
          "integer power recursion consumes parse-depth budget");

    EvaluationLimits factorial_limits;
    factorial_limits.max_factorial = 5U;
    EvaluationBudget factorial_budget(factorial_limits);
    scientific = calculator::evaluate_scientific(
        "6!", {}, {}, calculator::AngleUnit::Radians,
        calculator::kScientificDefaultDigits, &factorial_budget);
    check(!scientific.ok, "factorial consumes the shared operation limit");
    check(factorial_budget.error() == "factorial input too large",
          "factorial limit reports the budget error");

    EvaluationLimits power_limits;
    power_limits.max_power_exponent = 3U;
    EvaluationBudget power_budget(power_limits);
    scientific = calculator::evaluate_scientific(
        "2^4", {}, {}, calculator::AngleUnit::Radians,
        calculator::kScientificDefaultDigits, &power_budget);
    check(!scientific.ok, "integer power consumes the shared exponent limit");
    check(power_budget.error() == "power exponent too large",
          "power limit reports the budget error");

    EvaluationLimits relaxed_limits;
    relaxed_limits.max_input_bytes = calculator::kEvaluationMaxInputBytes + 1U;
    relaxed_limits.max_steps = calculator::kEvaluationMaxSteps + 1U;
    relaxed_limits.max_parse_depth = calculator::kEvaluationMaxParseDepth + 1U;
    relaxed_limits.max_function_depth = calculator::kEvaluationMaxFunctionDepth + 1U;
    relaxed_limits.max_factorial = calculator::kEvaluationMaxFactorial + 1U;
    relaxed_limits.max_power_exponent = calculator::kEvaluationMaxPowerExponent + 1U;
    relaxed_limits.max_output_digits = calculator::kEvaluationMaxOutputDigits + 1U;
    EvaluationBudget relaxed_budget(relaxed_limits);
    check(relaxed_budget.limits().max_input_bytes == calculator::kEvaluationMaxInputBytes,
          "caller cannot relax hard input ceiling");
    check(relaxed_budget.limits().max_steps == calculator::kEvaluationMaxSteps,
          "caller cannot relax hard work ceiling");
    check(relaxed_budget.limits().max_parse_depth == calculator::kEvaluationMaxParseDepth,
          "caller cannot relax hard parse-depth ceiling");
    check(relaxed_budget.limits().max_function_depth == calculator::kEvaluationMaxFunctionDepth,
          "caller cannot relax hard function-depth ceiling");
    check(relaxed_budget.limits().max_factorial == calculator::kEvaluationMaxFactorial,
          "caller cannot relax hard factorial ceiling");
    check(relaxed_budget.limits().max_power_exponent == calculator::kEvaluationMaxPowerExponent,
          "caller cannot relax hard exponent ceiling");
    check(relaxed_budget.limits().max_output_digits == calculator::kEvaluationMaxOutputDigits,
          "caller cannot relax hard arbitrary-precision output ceiling");

    EvaluationLimits zero_limits;
    zero_limits.max_input_bytes = 0U;
    zero_limits.max_steps = 0U;
    zero_limits.max_parse_depth = 0U;
    zero_limits.max_function_depth = 0U;
    zero_limits.max_factorial = 0U;
    zero_limits.max_power_exponent = 0U;
    zero_limits.max_output_digits = 0U;
    EvaluationBudget zero_budget(zero_limits);
    check(zero_budget.limits().max_input_bytes == calculator::kEvaluationMaxInputBytes &&
              zero_budget.limits().max_steps == calculator::kEvaluationMaxSteps &&
              zero_budget.limits().max_parse_depth == calculator::kEvaluationMaxParseDepth &&
              zero_budget.limits().max_function_depth == calculator::kEvaluationMaxFunctionDepth &&
              zero_budget.limits().max_factorial == calculator::kEvaluationMaxFactorial &&
              zero_budget.limits().max_power_exponent == calculator::kEvaluationMaxPowerExponent &&
              zero_budget.limits().max_output_digits == calculator::kEvaluationMaxOutputDigits,
          "zero cannot disable product evaluation safety ceilings");

    EvaluationLimits maximum_timeout_limits;
    maximum_timeout_limits.timeout = std::chrono::milliseconds::max();
    EvaluationBudget maximum_timeout_budget(maximum_timeout_limits);
    check(maximum_timeout_budget.check(),
          "maximum timeout saturates without overflowing the monotonic clock");

    EvaluationLimits graph_limits;
    graph_limits.max_steps = 50U;
    EvaluationBudget graph_budget(graph_limits);
    const auto graph = calculator::tools::evaluate(
        calculator::tools::AdvancedTool::Graph,
        "x ; 0 ; 1 ; 20", &graph_budget);
    check(!graph.ok, "graph samples share one cumulative evaluation budget");
    check(graph_budget.error() == "evaluation requires too many steps",
          "graph cumulative limit reports the budget error");

    EvaluationBudget cancelled;
    cancelled.cancel();
    scientific = calculator::evaluate_scientific(
        "1+1", {}, {}, calculator::AngleUnit::Radians,
        calculator::kScientificDefaultDigits, &cancelled);
    check(!scientific.ok, "cancelled evaluation stops before parsing");
    check(cancelled.error() == "evaluation cancelled",
          "cancellation has a stable diagnostic");

    EvaluationBudget cancelled_integer;
    cancelled_integer.cancel();
    const auto integer_cancelled = calculator::tools::evaluate(
        calculator::tools::AdvancedTool::ArbitraryPrecision,
        "100!", &cancelled_integer);
    check(!integer_cancelled.ok && cancelled_integer.failed(),
          "cancelled arbitrary-precision work stops at the shared boundary");

    if (failures != 0) {
        std::cerr << failures << " evaluation budget test(s) failed\n";
        return 1;
    }
    std::cout << "evaluation budget tests passed\n";
    return 0;
}
