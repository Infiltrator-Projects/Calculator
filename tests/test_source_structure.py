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

assert "src/core/evaluation_budget.cpp" in cmake, (
    "CMake omits evaluation-budget implementation"
)

# XcodeGen owns Calculator's portable source trees as cohesive directories.
# Do not recreate CMake's translation-unit list in project.yml: that historical
# duplication allowed a new core module to compile on desktop while silently
# disappearing from iOS until another manifest was edited by hand.
for source_tree in ("../src/core", "../src/ui"):
    assert f"- path: {source_tree}" in ios, (
        f"iPhone no longer owns Calculator source tree: {source_tree}"
    )
for relative in ADVANCED_MODULES:
    assert f"../{relative}" not in ios, (
        f"iPhone has regressed to per-file Calculator manifest duplication: {relative}"
    )

dispatcher = (ROOT / "src" / "core" / "advanced_tools.cpp").read_text(
    encoding="utf-8"
)
BUDGETED_HANDLERS = {
    "graph_tool",
    "equation_tool",
    "exact_decimal_tool",
    "arbitrary_precision_tool",
}
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
    arguments = "input, active_budget" if handler in BUDGETED_HANDLERS else "input"
    assert f"detail::{handler}({arguments})" in dispatcher, (
        f"Advanced Tools dispatcher no longer routes through private module: {handler}"
    )

assert len(dispatcher.splitlines()) < 200, (
    "Advanced Tools dispatcher has grown back into a god file"
)


# Desktop entry points are orchestration boundaries. Persistence and reusable
# native primitives have named owners so platform mains cannot silently absorb
# storage policy or utility implementations again.
UI_MODULES = (
    "src/ui/calculator_ui_controller.cpp",
    "src/ui/calculator_ui_state.cpp",
    "src/ui/calculator_ui_presentation.cpp",
)
PLATFORM_MODULES = (
    "src/app/linux_persistence.cpp",
    "src/app/windows_platform.cpp",
    "src/app/windows_persistence.cpp",
)
for relative in UI_MODULES + PLATFORM_MODULES:
    assert (ROOT / relative).is_file(), f"missing architecture module: {relative}"
    assert relative in cmake, f"CMake omits architecture module: {relative}"

# Linux exposes exactly one Calculator executable. Terminal operation is an
# explicit /cli mode of that executable, never a second installed program.
for relative in (
    "src/app/linux_entry.cpp",
    "src/app/cli_mode.cpp",
    "src/app/cli_mode.hpp",
):
    assert (ROOT / relative).is_file(), f"missing Linux mode module: {relative}"
assert not (ROOT / "src/app/cli_main.cpp").exists(), (
    "obsolete separate CLI entry point returned"
)
for forbidden in (
    "add_executable(calculator-cli",
    "install(TARGETS calculator-cli",
):
    assert forbidden not in cmake, (
        "CMake must not build or install a separate Calculator CLI executable"
    )
assert "src/app/linux_entry.cpp" in cmake, (
    "CMake omits the single Calculator process dispatcher"
)
assert "src/app/cli_mode.cpp" in cmake, (
    "CMake omits Calculator /cli implementation"
)
linux_entry = (ROOT / "src/app/linux_entry.cpp").read_text(encoding="utf-8")
assert '"/cli"' in linux_entry, "Calculator entry point lost /cli dispatch"
assert "calculator_gui_main" in linux_entry, (
    "Calculator entry point no longer routes normal launch to the GTK shell"
)

controller_dispatch = (ROOT / "src/ui/calculator_ui_controller.cpp").read_text(encoding="utf-8")
controller_state = (ROOT / "src/ui/calculator_ui_state.cpp").read_text(encoding="utf-8")
controller_presentation = (ROOT / "src/ui/calculator_ui_presentation.cpp").read_text(encoding="utf-8")
for method in ("persistent_state_text", "load_persistent_state_text", "recall_history", "recall_memory"):
    assert f"Controller::{method}(" not in controller_dispatch, f"Controller state leaked back into dispatch TU: {method}"
    assert f"Controller::{method}(" in controller_state, f"Controller state TU lost ownership of: {method}"
for method in ("additional_results", "programmer_bits", "format_display", "button_label", "command_enabled"):
    assert f"Controller::{method}(" not in controller_dispatch, f"Controller presentation leaked back into dispatch TU: {method}"
    assert f"Controller::{method}(" in controller_presentation, f"Controller presentation TU lost ownership of: {method}"

linux_main = (ROOT / "src/app/main.cpp").read_text(encoding="utf-8")
linux_persistence = (ROOT / "src/app/linux_persistence.cpp").read_text(encoding="utf-8")
for mechanism in ("g_get_user_data_dir", "g_get_user_config_dir", "infiltratr_atomic_file_write_bytes", "infiltratr_read_text_file_alloc"):
    assert mechanism not in linux_main, f"Linux storage mechanism leaked into main.cpp: {mechanism}"
    assert mechanism in linux_persistence, f"Linux persistence owner lost mechanism: {mechanism}"

windows_main = (ROOT / "src/app/windows_main.cpp").read_text(encoding="utf-8")
windows_persistence = (ROOT / "src/app/windows_persistence.cpp").read_text(encoding="utf-8")
windows_platform = (ROOT / "src/app/windows_platform.cpp").read_text(encoding="utf-8")
assert 'L"Software\\\\Infiltrator\\\\Calc"' not in windows_main, "Windows app Registry identity leaked back into windows_main.cpp"
assert 'L"Software\\\\Infiltrator\\\\Calc"' in windows_persistence, "Windows persistence owner lost Calculator Registry identity"
for mechanism in ("MultiByteToWideChar", "GetDpiForWindow"):
    assert mechanism not in windows_main, f"Reusable Win32 primitive leaked back into windows_main.cpp: {mechanism}"
    assert mechanism in windows_platform, f"Windows platform owner lost primitive: {mechanism}"

print("Calculator source modularity contract passed.")
