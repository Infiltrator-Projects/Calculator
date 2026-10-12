#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_exact(path: str, old: str, new: str, count: int = 1) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    actual = text.count(old)
    if actual != count:
        raise SystemExit(
            f"{path}: expected {count} occurrence(s), found {actual}: {old[:100]!r}"
        )
    target.write_text(text.replace(old, new), encoding="utf-8")


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


# Compile the budget implementation into calculator-core.
replace_exact(
    "CMakeLists.txt",
    "    src/core/calculator.cpp\n    src/core/scientific.cpp\n",
    "    src/core/calculator.cpp\n    src/core/evaluation_budget.cpp\n    src/core/scientific.cpp\n",
)

# Test the policy itself and, critically, prove that Scientific and iterative
# Advanced Tools consume one shared budget rather than merely exposing API.
write(
    "tests/test_evaluation_budget.cpp",
    r'''/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/advanced_tools.hpp"
#include "../src/core/evaluation_budget.hpp"
#include "../src/core/scientific.hpp"

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

    EvaluationLimits depth_limits;
    depth_limits.max_parse_depth = 2U;
    EvaluationBudget depth_budget(depth_limits);
    scientific = calculator::evaluate_scientific(
        "---1", {}, {}, calculator::AngleUnit::Radians,
        calculator::kScientificDefaultDigits, &depth_budget);
    check(!scientific.ok, "Scientific parser consumes the shared parse-depth budget");
    check(depth_budget.error() == "expression nesting too deep",
          "Scientific parse-depth failure is reported by the budget");

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

    if (failures != 0) {
        std::cerr << failures << " evaluation budget test(s) failed\n";
        return 1;
    }
    std::cout << "evaluation budget tests passed\n";
    return 0;
}
''',
)
replace_exact(
    "CMakeLists.txt",
    "add_executable(calculator-advanced-tools-test\n    tests/test_advanced_tools.cpp\n)\ntarget_link_libraries(calculator-advanced-tools-test PRIVATE calculator-core)\nadd_test(NAME calculator-advanced-tools COMMAND calculator-advanced-tools-test)\n",
    "add_executable(calculator-advanced-tools-test\n    tests/test_advanced_tools.cpp\n)\ntarget_link_libraries(calculator-advanced-tools-test PRIVATE calculator-core)\nadd_test(NAME calculator-advanced-tools COMMAND calculator-advanced-tools-test)\n\n"
    "add_executable(calculator-evaluation-budget-test\n    tests/test_evaluation_budget.cpp\n)\n"
    "target_link_libraries(calculator-evaluation-budget-test PRIVATE calculator-core)\n"
    "add_test(NAME calculator-evaluation-budget COMMAND calculator-evaluation-budget-test)\n",
)

# Advanced Tools no longer owns a second literal copy of the input limit.
replace_exact(
    "src/core/advanced_tools.hpp",
    "#pragma once\n\n#include <array>",
    "#pragma once\n\n#include \"evaluation_budget.hpp\"\n\n#include <array>",
)
replace_exact(
    "src/core/advanced_tools.hpp",
    "namespace calculator {\nclass EvaluationBudget;\n}\n\nnamespace calculator::tools {",
    "namespace calculator::tools {",
)
replace_exact(
    "src/core/advanced_tools.hpp",
    "inline constexpr std::size_t kMaxToolInputBytes = 8192U;",
    "inline constexpr std::size_t kMaxToolInputBytes =\n    EvaluationLimits{}.max_input_bytes;",
)

# Scientific parser now consumes the same budget that graph/root callers share.
replace_exact(
    "src/core/scientific.cpp",
    "#include \"scientific.hpp\"\n",
    "#include \"scientific.hpp\"\n#include \"evaluation_budget.hpp\"\n",
)
replace_exact(
    "src/core/scientific.cpp",
    "// Both expression nesting and user-function recursion are bounded separately:\n// one protects native parser stack depth, the other prevents cyclic/custom\n// function expansion from becoming an unbounded recursive evaluation.\n// These limits are deliberately conservative across the smallest supported\n// native stacks (notably Windows). They are product safety bounds, not\n// mathematical limits: ordinary calculator expressions should never need this\n// much recursive grammar or user-function expansion.\nconstexpr std::size_t kMaxParseDepth = 16;\nconstexpr std::size_t kMaxFunctionDepth = 16;\n\n",
    "",
)
replace_exact(
    "src/core/scientific.cpp",
    "        std::size_t function_depth,\n        AngleUnit angle_unit,\n        unsigned digits)\n        : input_(input),\n          variables_(variables),\n          functions_(functions),\n          function_depth_(function_depth),\n          angle_unit_(angle_unit),\n          digits_(digits) {}",
    "        std::size_t function_depth,\n        AngleUnit angle_unit,\n        unsigned digits,\n        EvaluationBudget& budget)\n        : input_(input),\n          variables_(variables),\n          functions_(functions),\n          function_depth_(function_depth),\n          angle_unit_(angle_unit),\n          digits_(digits),\n          budget_(budget) {}",
)
replace_exact(
    "src/core/scientific.cpp",
    "    unsigned digits_ = kScientificDefaultDigits;\n    std::size_t position_ = 0;",
    "    unsigned digits_ = kScientificDefaultDigits;\n    EvaluationBudget& budget_;\n    std::size_t position_ = 0;",
)
replace_exact(
    "src/core/scientific.cpp",
    "    // Internal nested evaluation stays in the full backend precision.",
    "    bool charge(std::uint64_t amount = 1U) {\n"
    "        if (budget_.charge(amount)) return true;\n"
    "        if (error_.empty()) error_ = budget_.error();\n"
    "        return false;\n"
    "    }\n\n"
    "    // Internal nested evaluation stays in the full backend precision.",
)
for signature in (
    "    Complex parse_expression() {\n",
    "    Complex parse_term() {\n",
    "    Complex parse_unary() {\n",
    "    Complex parse_power() {\n",
    "    Complex parse_postfix() {\n",
    "    Complex parse_primary() {\n",
):
    replace_exact(
        "src/core/scientific.cpp",
        signature,
        signature + "        if (!charge()) return {};\n",
    )
replace_exact(
    "src/core/scientific.cpp",
    "    Complex parse_unary() {\n        if (!charge()) return {};\n        if (recursion_depth_ >= kMaxParseDepth) {\n            error_ = \"expression nesting too deep\";\n            return {};\n        }\n\n        ++recursion_depth_;",
    "    Complex parse_unary() {\n        if (!charge()) return {};\n        if (!budget_.check_parse_depth(recursion_depth_ + 1U)) {\n            error_ = budget_.error();\n            return {};\n        }\n\n        ++recursion_depth_;",
)
replace_exact(
    "src/core/scientific.cpp",
    "        if (function_depth_ >= kMaxFunctionDepth) {\n            error_ = \"function recursion too deep\";\n            return {};\n        }",
    "        if (!budget_.check_function_depth(function_depth_ + 1U)) {\n            error_ = budget_.error();\n            return {};\n        }",
)
replace_exact(
    "src/core/scientific.cpp",
    "        if (input > 100000) {\n            error_ = \"factorial input too large\";\n            return {};\n        }\n\n        const unsigned long count =\n            input.convert_to<unsigned long>();\n        Real result = 1;",
    "        const unsigned long count =\n            input.convert_to<unsigned long>();\n        if (!budget_.check_factorial(static_cast<std::uint64_t>(count))) {\n            error_ = budget_.error();\n            return {};\n        }\n        if (!charge(static_cast<std::uint64_t>(count))) return {};\n        Real result = 1;",
)
replace_exact(
    "src/core/scientific.cpp",
    "            const Complex right = parse_unary();\n            bool valid = true;\n            left = principal_power(left, right, valid);",
    "            const Complex right = parse_unary();\n"
    "            long long integral_exponent = 0;\n"
    "            if (real_integer_exponent(right, integral_exponent)) {\n"
    "                const std::uint64_t magnitude = integral_exponent < 0\n"
    "                    ? static_cast<std::uint64_t>(-(integral_exponent + 1LL)) + 1ULL\n"
    "                    : static_cast<std::uint64_t>(integral_exponent);\n"
    "                if (!budget_.check_power_exponent(magnitude)) {\n"
    "                    error_ = budget_.error();\n"
    "                    return {};\n"
    "                }\n"
    "            }\n"
    "            bool valid = true;\n"
    "            left = principal_power(left, right, valid);",
)
replace_exact(
    "src/core/scientific.cpp",
    "        Parser nested(\n            definition.expression, scoped, functions_,\n            function_depth_ + 1U, angle_unit_, digits_);",
    "        Parser nested(\n            definition.expression, scoped, functions_,\n            function_depth_ + 1U, angle_unit_, digits_, budget_);",
)
# Superscript exponents use the same policy rather than a private literal cap.
replace_exact(
    "src/core/scientific.cpp",
    "            if (value > 100000000LL) {\n                position_ = saved;\n                return false;\n            }\n            value = value * 10LL + digit;",
    "            if (value > 100000000LL) {\n                position_ = saved;\n                return false;\n            }\n            value = value * 10LL + digit;",
)
replace_exact(
    "src/core/scientific.cpp",
    "                if (!parse_superscript_exponent(exponent)) break;\n                bool valid = true;\n                value = integer_power(value, exponent, valid);",
    "                if (!parse_superscript_exponent(exponent)) break;\n"
    "                const std::uint64_t magnitude = exponent < 0\n"
    "                    ? static_cast<std::uint64_t>(-(exponent + 1LL)) + 1ULL\n"
    "                    : static_cast<std::uint64_t>(exponent);\n"
    "                if (!budget_.check_power_exponent(magnitude)) {\n"
    "                    error_ = budget_.error();\n"
    "                    return {};\n"
    "                }\n"
    "                bool valid = true;\n"
    "                value = integer_power(value, exponent, valid);",
)

# The internal parser entry becomes explicit and budget-aware; the public
# wrapper in evaluation_budget.cpp owns input/deadline/output checks.
replace_exact(
    "src/core/scientific.cpp",
    "ScientificResult evaluate_scientific(\n    const std::string& expression,\n    const ScientificVariables& variables,\n    const Functions& functions,\n    AngleUnit angle_unit,\n    unsigned decimal_digits) {\n    const std::string normalized =\n        normalize_expression_spelling(expression);\n    return Parser(\n        normalized, variables, &functions, 0U,\n        angle_unit, clamp_digits(decimal_digits)).run();\n}",
    "ScientificResult evaluate_scientific_unchecked(\n"
    "    const std::string& expression,\n"
    "    const ScientificVariables& variables,\n"
    "    const Functions& functions,\n"
    "    AngleUnit angle_unit,\n"
    "    unsigned decimal_digits,\n"
    "    EvaluationBudget& budget) {\n"
    "    const std::string normalized =\n"
    "        normalize_expression_spelling(expression);\n"
    "    return Parser(\n"
    "        normalized, variables, &functions, 0U,\n"
    "        angle_unit, clamp_digits(decimal_digits), budget).run();\n"
    "}",
)

replace_exact(
    "src/core/evaluation_budget.cpp",
    "// scientific.cpp retains the parser implementation behind this internal\n// five-argument overload. Public callers see only the budget-aware overload in\n// scientific.hpp, so all external Scientific work crosses one resource gate.\nScientificResult evaluate_scientific(\n    const std::string& expression,\n    const ScientificVariables& variables,\n    const Functions& functions,\n    AngleUnit angle_unit,\n    unsigned decimal_digits);",
    "// scientific.cpp retains the parser implementation behind this explicit\n"
    "// budget-aware internal entry. Public callers cross the wrapper below so\n"
    "// input, deadline, cancellation, output and parser work share one policy.\n"
    "ScientificResult evaluate_scientific_unchecked(\n"
    "    const std::string& expression,\n"
    "    const ScientificVariables& variables,\n"
    "    const Functions& functions,\n"
    "    AngleUnit angle_unit,\n"
    "    unsigned decimal_digits,\n"
    "    EvaluationBudget& budget);",
)
replace_exact(
    "src/core/evaluation_budget.cpp",
    "    using ParserEntry = ScientificResult (*)(\n        const std::string&,\n        const ScientificVariables&,\n        const Functions&,\n        AngleUnit,\n        unsigned);\n    const auto parser_entry =\n        static_cast<ParserEntry>(&evaluate_scientific);\n    ScientificResult result = parser_entry(\n        expression, variables, functions, angle_unit, decimal_digits);",
    "    ScientificResult result = evaluate_scientific_unchecked(\n"
    "        expression, variables, functions, angle_unit,\n"
    "        decimal_digits, active_budget);",
)

print("evaluation budget integration patch applied")
