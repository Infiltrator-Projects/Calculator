#!/usr/bin/env python3
"""Protect Calculator's reviewed source-ownership and build-module boundaries."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

ADVANCED_MODULES = (
    "src/core/advanced_tools.cpp",
    "src/core/advanced_tools_conversion.cpp",
    "src/core/advanced_tools_analysis.cpp",
    "src/core/advanced_tools_exact.cpp",
    "src/core/advanced_tools_number.cpp",
)

for relative in ADVANCED_MODULES:
    assert (ROOT / relative).is_file(), f"missing Advanced Tools module: {relative}"

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
ios = (ROOT / "ios" / "project.yml").read_text(encoding="utf-8")
for relative in ADVANCED_MODULES:
    assert relative in cmake, f"CMake omits Advanced Tools module: {relative}"
    assert f"../{relative}" in ios, f"iPhone omits Advanced Tools module: {relative}"

assert "src/core/advanced_tools_internal.hpp" in ios, (
    "iPhone source manifest omits the private Advanced Tools boundary"
)

dispatcher = (ROOT / "src" / "core" / "advanced_tools.cpp").read_text(
    encoding="utf-8"
)
for handler in (
    "engineering_tool",
    "unit_tool",
    "network_tool",
    "storage_tool",
    "datetime_tool",
    "constants_tool",
    "statistics_tool",
    "graph_tool",
    "equation_tool",
    "exact_decimal_tool",
    "arbitrary_precision_tool",
    "complex_tool",
    "financial_tool",
    "number_utilities_tool",
):
    assert f"ToolResult {handler}(" not in dispatcher, (
        f"Advanced Tools domain implementation leaked back into dispatcher: {handler}"
    )
    assert f"detail::{handler}(input)" in dispatcher, (
        f"Advanced Tools dispatcher no longer routes through private module: {handler}"
    )

assert len(dispatcher.splitlines()) < 200, (
    "Advanced Tools dispatcher has grown back into a god file"
)

print("Calculator source modularity contract passed.")
