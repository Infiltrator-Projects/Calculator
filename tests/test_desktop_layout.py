#!/usr/bin/env python3
from pathlib import Path

linux = Path("src/app/main.cpp").read_text(encoding="utf-8")
windows = Path("src/app/windows_main.cpp").read_text(encoding="utf-8")
contract = Path("src/ui/calculator_ui_contract.hpp").read_text(encoding="utf-8")
theme = Path("src/ui/calculator_theme.hpp").read_text(encoding="utf-8")
ios = Path("ios/Sources/ContentView.swift").read_text(encoding="utf-8")
ios_bridge = Path("ios/Bridge/InfiltratorCalcBridge.mm").read_text(encoding="utf-8")
ios_project = Path("ios/project.yml").read_text(encoding="utf-8")

for name, source in (("Linux GTK", linux), ("Windows Win32", windows)):
    assert '../ui/calculator_ui_controller.hpp' in source, (
        f"{name} does not consume the shared calculator UI controller"
    )
    for needle in (
        "kDesktopMetrics",
        "kStandardMemory",
        "kStandardKeypad",
        "kScientificKeypad",
        "kProgrammerKeypad",
        "mode_name(",
        "Command::",
        ".dispatch(",
        "command_enabled(",
        "responsive_layout(",
    ):
        assert needle in source, f"{name} bypasses shared UI contract: {needle}"

    for forbidden in (
        "standard_keys[][4]",
        "scientific_keys[][4]",
        "programmer_keys[][4]",
        'L"MC", L"MR", L"M+", L"M−"',
        '{"MC", "MR", "M+", "M−"}',
    ):
        assert forbidden not in source, (
            f"{name} reintroduced a local calculator layout definition: {forbidden}"
        )

for needle in (
    "inline constexpr DesktopMetrics kDesktopMetrics",
    "360, 610",
    "320, 520",
    "inline constexpr std::array<ButtonSpec, 4> kStandardMemory",
    "inline constexpr std::array<ButtonSpec, 24> kStandardKeypad",
    "inline constexpr std::array<ButtonSpec, 40> kScientificKeypad",
    "inline constexpr std::array<ButtonSpec, 40> kProgrammerKeypad",
    '{"MC", ButtonRole::Utility, Command::MemoryClear}',
    '{"=", ButtonRole::Equals, Command::Equals}',
    "is_programmer_selector(Command command)",
    "insertion_text(Command command)",
    "responsive_layout(int width, int height)",
    "wide_threshold",
    "history_min_width",
):
    assert needle in contract, f"shared UI contract is incomplete: {needle}"

for forbidden_state in (
    "infiltrator::calc::Session session",
    "infiltrator::calc::Session g_session",
    "Mode mode = Mode::Standard",
    "Mode g_mode = Mode::Standard",
    "bool degrees = true",
    "bool g_degrees = true",
    "ProgrammerBase programmer_base",
    "ProgrammerBase g_programmer_base",
    "IntegerWidth programmer_width",
    "IntegerWidth g_programmer_width",
    "bool programmer_signed =",
    "bool g_programmer_signed =",
):
    assert forbidden_state not in linux, (
        f"Linux GTK reintroduced platform-owned calculator state: {forbidden_state}"
    )
    assert forbidden_state not in windows, (
        f"Windows Win32 reintroduced platform-owned calculator state: {forbidden_state}"
    )

for needle in (
    "#include <infiltratr/design.h>",
    "INFILTRATR_THEME_SYSTEM",
    "INFILTRATR_THEME_DAY",
    "INFILTRATR_THEME_NIGHT",
    "infiltratr_theme_resolve",
    "infiltratr_theme_mode_next",
):
    assert needle in theme, f"Calc Common theme adapter missing: {needle}"

for forbidden in (
    "0x050608",
    "0xF4F5F7",
    "kNightPalette",
    "kDayPalette",
):
    assert forbidden not in theme, (
        f"Calc reintroduced private canonical theme values: {forbidden}"
    )

for name, source in (("Linux GTK", linux), ("Windows Win32", windows)):
    for needle in (
        'calculator_theme.hpp',
        'ThemeMode::System',
        'ThemeMode::Day',
        'ThemeMode::Night',
        'next_theme_mode',
    ):
        assert needle in source, f"{name} theme support missing: {needle}"

for needle in (
    "ThemePreference",
    "case system",
    "case day",
    "case night",
    '@AppStorage("themePreference")',
    ".preferredColorScheme(themePreference.preferredScheme)",
):
    assert needle in ios, f"iPhone theme support missing: {needle}"

# iPhone owns only SwiftUI adaptation and platform theme preference. The
# semantic Day/Night values must come from Common through the Objective-C++
# bridge rather than being mirrored in Swift.
for needle in (
    "ICCalculatorBridge.themePalette(dark: dark)",
    "InfiltratorPalette(dark: false)",
    "InfiltratorPalette(dark: true)",
):
    assert needle in ios, f"iPhone does not consume Common theme data: {needle}"

for forbidden in (
    "static let night",
    "static let day",
    "0x050608",
    "0xF4F5F7",
):
    assert forbidden not in ios, (
        f"iPhone reintroduced a private Common palette mirror: {forbidden}"
    )

for needle in (
    "#include <infiltratr/design.h>",
    "infiltratr_theme_resolve",
    "background_rgb",
    "button_background_rgb",
    "equals_hover_rgb",
):
    assert needle in ios_bridge, f"iPhone Common theme bridge missing: {needle}"

for needle in (
    "../src/infiltratr-common/src/core.c",
    "../src/infiltratr-common/src/design.c",
    "../src/infiltratr-common/include",
):
    assert needle in ios_project, f"iPhone Common build integration missing: {needle}"

print("Cross-platform desktop UI parity contract passed.")
