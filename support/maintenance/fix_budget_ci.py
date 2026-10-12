#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_exact(path: str, old: str, new: str, count: int = 1) -> None:
    p = ROOT / path
    text = p.read_text(encoding="utf-8")
    actual = text.count(old)
    if actual != count:
        raise SystemExit(f"{path}: expected {count} occurrence(s), found {actual}")
    p.write_text(text.replace(old, new), encoding="utf-8")


replace_exact(
    "ios/project.yml",
    "      - path: ../src/core/calculator.hpp\n      - path: ../src/core/scientific.cpp\n",
    "      - path: ../src/core/calculator.hpp\n"
    "      - path: ../src/core/evaluation_budget.cpp\n"
    "      - path: ../src/core/evaluation_budget.hpp\n"
    "      - path: ../src/core/scientific.cpp\n",
)

replace_exact(
    "tests/test_source_structure.py",
    "for relative in ADVANCED_MODULES:\n"
    "    assert relative in cmake, f\"CMake omits Advanced Tools module: {relative}\"\n"
    "    assert f\"../{relative}\" in ios, f\"iPhone omits Advanced Tools module: {relative}\"\n\n"
    "assert \"src/core/advanced_tools_internal.hpp\" in ios, (\n"
    "    \"iPhone source manifest omits the private Advanced Tools boundary\"\n"
    ")\n",
    "for relative in ADVANCED_MODULES:\n"
    "    assert relative in cmake, f\"CMake omits Advanced Tools module: {relative}\"\n"
    "    assert f\"../{relative}\" in ios, f\"iPhone omits Advanced Tools module: {relative}\"\n\n"
    "for relative in (\n"
    "    \"src/core/evaluation_budget.cpp\",\n"
    "    \"src/core/evaluation_budget.hpp\",\n"
    "):\n"
    "    assert relative in cmake or relative.endswith(\".hpp\"), (\n"
    "        f\"CMake omits evaluation-budget implementation: {relative}\"\n"
    "    )\n"
    "    assert f\"../{relative}\" in ios, (\n"
    "        f\"iPhone omits evaluation-budget source: {relative}\"\n"
    "    )\n\n"
    "assert \"src/core/advanced_tools_internal.hpp\" in ios, (\n"
    "    \"iPhone source manifest omits the private Advanced Tools boundary\"\n"
    ")\n",
)

replace_exact(
    "tests/test_source_structure.py",
    "for handler in (\n"
    "    \"engineering_tool\",\n"
    "    \"unit_tool\",\n"
    "    \"network_tool\",\n"
    "    \"storage_tool\",\n"
    "    \"datetime_tool\",\n"
    "    \"constants_tool\",\n"
    "    \"statistics_tool\",\n"
    "    \"graph_tool\",\n"
    "    \"equation_tool\",\n"
    "    \"exact_decimal_tool\",\n"
    "    \"arbitrary_precision_tool\",\n"
    "    \"complex_tool\",\n"
    "    \"financial_tool\",\n"
    "    \"number_utilities_tool\",\n"
    "):\n"
    "    assert f\"ToolResult {handler}(\" not in dispatcher, (\n"
    "        f\"Advanced Tools domain implementation leaked back into dispatcher: {handler}\"\n"
    "    )\n"
    "    assert f\"detail::{handler}(input)\" in dispatcher, (\n"
    "        f\"Advanced Tools dispatcher no longer routes through private module: {handler}\"\n"
    "    )\n",
    "BUDGETED_HANDLERS = {\"graph_tool\", \"equation_tool\"}\n"
    "for handler in (\n"
    "    \"engineering_tool\",\n"
    "    \"unit_tool\",\n"
    "    \"network_tool\",\n"
    "    \"storage_tool\",\n"
    "    \"datetime_tool\",\n"
    "    \"constants_tool\",\n"
    "    \"statistics_tool\",\n"
    "    \"graph_tool\",\n"
    "    \"equation_tool\",\n"
    "    \"exact_decimal_tool\",\n"
    "    \"arbitrary_precision_tool\",\n"
    "    \"complex_tool\",\n"
    "    \"financial_tool\",\n"
    "    \"number_utilities_tool\",\n"
    "):\n"
    "    assert f\"ToolResult {handler}(\" not in dispatcher, (\n"
    "        f\"Advanced Tools domain implementation leaked back into dispatcher: {handler}\"\n"
    "    )\n"
    "    arguments = \"input, active_budget\" if handler in BUDGETED_HANDLERS else \"input\"\n"
    "    assert f\"detail::{handler}({arguments})\" in dispatcher, (\n"
    "        f\"Advanced Tools dispatcher no longer routes through private module: {handler}\"\n"
    "    )\n",
)

replace_exact(
    "src/core/scientific.cpp",
    "        const unsigned long count =\n"
    "            input.convert_to<unsigned long>();\n"
    "        if (!budget_.check_factorial(static_cast<std::uint64_t>(count))) {",
    "        if (input > Real(std::numeric_limits<unsigned long>::max())) {\n"
    "            error_ = \"factorial input too large\";\n"
    "            return {};\n"
    "        }\n"
    "        const unsigned long count =\n"
    "            input.convert_to<unsigned long>();\n"
    "        if (!budget_.check_factorial(static_cast<std::uint64_t>(count))) {",
)

print("evaluation-budget CI fixes applied")
