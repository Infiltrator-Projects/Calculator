#!/usr/bin/env python3
from pathlib import Path

linux = Path("src/app/main.cpp").read_text(encoding="utf-8")
windows = Path("src/app/windows_main.cpp").read_text(encoding="utf-8")
contract = Path("src/ui/calculator_ui_contract.hpp").read_text(encoding="utf-8")
theme = Path("src/ui/calculator_theme.hpp").read_text(encoding="utf-8")
ios = Path("ios/Sources/ContentView.swift").read_text(encoding="utf-8")

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
    "enum class ThemeMode { System = 0, Day = 1, Night = 2 }",
    "kNightPalette",
    "kDayPalette",
    "next_theme_mode",
):
    assert needle in theme, f"shared theme contract missing: {needle}"

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

print("Cross-platform desktop UI parity contract passed.")
