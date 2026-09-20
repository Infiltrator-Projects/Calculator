#!/usr/bin/env python3
from pathlib import Path

linux = Path("src/app/main.cpp").read_text(encoding="utf-8")
windows = Path("src/app/windows_main.cpp").read_text(encoding="utf-8")
contract = Path("src/ui/calculator_ui_contract.hpp").read_text(encoding="utf-8")
theme = Path("src/ui/calculator_theme.hpp").read_text(encoding="utf-8")
ios = Path("ios/Sources/ContentView.swift").read_text(encoding="utf-8")
ios_app = Path("ios/Sources/CalculatorApp.swift").read_text(encoding="utf-8")
ios_typography = Path("ios/Sources/CalculatorTypography.swift").read_text(encoding="utf-8")
ios_bridge = Path("ios/Bridge/CalculatorBridge.mm").read_text(encoding="utf-8")
ios_project = Path("ios/project.yml").read_text(encoding="utf-8")
ios_font_script = Path("ios/bundle-fonts.sh").read_text(encoding="utf-8")
cmake = Path("CMakeLists.txt").read_text(encoding="utf-8")

for needle in (
    "calculator::ui::typography().ui_family",
    "calculator::ui::typography().brand_family",
    "calculator::ui::design_metrics()",
    "calculator::ui::desktop_preferred_height(mode)",
    "calculator::ui::desktop_preferred_height(Mode::Standard)",
):
    assert needle in linux, f"Linux Common/design contract missing: {needle}"

for forbidden in (
    "kStandardWindowHeight",
    "kExtendedWindowHeight",
    "gtk_widget_set_vexpand(standard_panel, TRUE);",
    "gtk_widget_set_vexpand(standard_grid, TRUE);",
):
    assert forbidden not in linux, (
        f"Linux reintroduced local/stretched Standard geometry: {forbidden}"
    )

for needle in (
    "desktop_preferred_height(mode)",
    "desktop_minimum_height(",
    "resize_main_for_mode(window, Mode::Standard)",
    "resize_main_for_mode(window, Mode::Scientific)",
    "resize_main_for_mode(window, Mode::Programmer)",
):
    assert needle in windows, f"Windows mode-aware geometry missing: {needle}"

for forbidden in (
    'kUiFont = "Sans"',
    'kBrandFont = "Sans"',
    'L"Segoe UI"',
):
    assert forbidden not in linux + windows, (
        f"Calculator introduced a non-canonical desktop font: {forbidden}"
    )

for needle in (
    "calculator::ui::typography().ui_family",
    "calculator::ui::typography().brand_family",
    "calculator::ui::design_metrics()",
    "FW_BOLD",
):
    assert needle in windows, f"Windows Common typography/design contract missing: {needle}"

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
    "480, 480",
    "desktop_preferred_height(Mode mode)",
    "desktop_minimum_height(Mode mode)",
    "inline constexpr std::array<ButtonSpec, 5> kStandardMemory",
    "inline constexpr std::array<ButtonSpec, 24> kStandardKeypad",
    "inline constexpr std::array<ButtonSpec, 40> kScientificKeypad",
    "inline constexpr std::array<ButtonSpec, 44> kProgrammerKeypad",
    '{"MC", ButtonRole::Utility, Command::MemoryClear}',
    '{"MS", ButtonRole::Utility, Command::MemoryStore}',
    '{"=", ButtonRole::Equals, Command::Equals}',
    "is_programmer_selector(Command command)",
    "insertion_text(Command command)",
    "responsive_layout(int width, int height)",
    "wide_threshold",
    "history_min_width",
):
    assert needle in contract, f"shared UI contract is incomplete: {needle}"

for forbidden_state in (
    "calculator::Session session",
    "calculator::Session g_session",
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
    "bool responsive_layout_initialized = false;",
    "responsive_layout_initialized = true;",
    "if (!responsive_layout_initialized ||",
    "gtk_widget_set_visible(history_dock, FALSE);",
):
    assert needle in linux, (
        f"Linux GTK history dock initialization regression: {needle}"
    )

for needle in (
    "#include <infiltratr/design.h>",
    "INFILTRATR_THEME_SYSTEM",
    "INFILTRATR_THEME_DAY",
    "INFILTRATR_THEME_NIGHT",
    "infiltratr_theme_resolve",
    "infiltratr_theme_mode_next",
    "infiltratr_design_metrics",
    "infiltratr_typography",
):
    assert needle in theme, f"Calculator Common theme adapter missing: {needle}"

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
    "CalculatorBridge.themePalette(dark: dark)",
    "CalculatorBridge.designMetrics()",
    "CalculatorPalette(dark: false)",
    "CalculatorPalette(dark: true)",
    "sharedDesign.controlSpacing",
    "sharedDesign.contentPadding",
    "sharedDesign.screenPadding",
):
    assert needle in ios, f"iPhone does not consume Common design data: {needle}"

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
    "infiltratr_design_metrics",
    "background_rgb",
    "button_background_rgb",
    "equals_hover_rgb",
    "control_radius",
    "content_padding",
):
    assert needle in ios_bridge, f"iPhone Common theme bridge missing: {needle}"

for needle in (
    "../src/ui/calculator_ui_controller.cpp",
    "../src/ui/calculator_ui_controller.hpp",
    "../src/ui/calculator_ui_contract.hpp",
    "../src/infiltratr-common/src/core.c",
    "../src/infiltratr-common/src/design.c",
    "../src/infiltratr-common/include",
    "ASSETCATALOG_COMPILER_APPICON_NAME: AppIcon",
):
    assert needle in ios_project, f"iPhone Common build integration missing: {needle}"

print("Cross-platform desktop UI parity contract passed.")

# Calculator-owned text has exactly three approved MB Corpo faces. Release
# builds carry those exact verified TTFs, and platform shells do not deliberately
# select a generic fourth family.
for needle in (
    "mb_corpo_s_regular.ttf",
    "mb_corpo_s_bold.ttf",
    "mb_corpo_a_cond_regular.ttf",
):
    assert needle in ios_project, f"iPhone font bundle declaration missing: {needle}"

for needle in (
    "typography.assets.source_repository",
    "typography.assets.source_commit",
    "typography.assets.archive_sha256",
    "typography.font_files.brand_regular",
    "typography.font_files.ui_bold",
    "typography.font_files.ui_regular",
):
    assert needle in ios_font_script, f"iPhone Common typography metadata missing: {needle}"

for needle in (
    "InfiltratrTypographyAssets.cmake",
    "INFILTRATR_MB_CORPO_ARCHIVE_URL",
    "INFILTRATR_MB_CORPO_ARCHIVE_SHA256",
    "INFILTRATR_MB_CORPO_BRAND_REGULAR_FILE",
    "INFILTRATR_MB_CORPO_UI_BOLD_FILE",
    "INFILTRATR_MB_CORPO_UI_REGULAR_FILE",
):
    assert needle in cmake, f"desktop Common typography provenance missing: {needle}"

for needle in (
    "MBCorpoSTitleWEB-Regular",
    "MBCorpoSTitleWEB-Bold",
    "MBCorpoATitleCondWEB-Regular",
    "preconditionFailure",
):
    assert needle in ios_typography, f"iPhone strict MB typography missing: {needle}"

assert "CalculatorTypography.verifyBundledFonts()" in ios_app
assert ".font(.system(" not in ios, "Calculator-owned iPhone text reintroduced a system font"
assert "return .system(" not in ios_typography, "iPhone typography reintroduced a system fallback"
assert ".monospaced" not in ios, "iPhone history reintroduced a fourth text face"
assert "CALCULATOR_FONT_RESOURCE_REGULAR=1101" in cmake
assert "CALCULATOR_FONT_RESOURCE_BOLD=1102" in cmake
assert "CALCULATOR_FONT_RESOURCE_CONDENSED=1103" in cmake
assert "AddFontMemResourceEx" in windows
assert "refusing silent font substitution" in linux

for needle in (
    "DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2",
    "GetDpiForWindow",
    "WM_DPICHANGED",
    "CALCULATOR_ICON_RESOURCE",
):
    assert needle in windows, f"Windows DPI/icon contract missing: {needle}"

assert "CALCULATOR_ICON_RESOURCE=1001" in cmake
assert "assets/windows/calculator.ico" in cmake
assert Path("assets/windows/calculator.ico").is_file()
assert Path("ios/Sources/Assets.xcassets/AppIcon.appiconset/Contents.json").is_file()
assert Path("ios/Sources/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png").is_file()

for forbidden in (
    "applyUnary",
    "applyScientific",
    "evaluateProgrammer",
    "memoryAdd",
    "memorySubtract",
):
    assert forbidden not in ios_bridge, (
        f"iPhone bridge reintroduced calculator semantics: {forbidden}"
    )
assert "calculator_ui_controller.hpp" in ios_bridge
assert "Controller" in ios_bridge
assert "history_text()" in ios_bridge
assert "session().history()" not in ios_bridge
assert "session().history()" not in linux
assert "session().history()" not in windows

for needle in (
    'bases_button = toolbar_button("Bases")',
    "show_programmer_bases",
    "programmer_representations_text()",
    "gtk_label_set_selectable(GTK_LABEL(result_label), TRUE)",
):
    assert needle in linux, f"Linux Programmer/copy completeness missing: {needle}"

for needle in (
    "kIdBases",
    'L"Bases"',
    "programmer_representations_text()",
    'L"Programmer Representations"',
):
    assert needle in windows, f"Windows Programmer representations missing: {needle}"

for needle in (
    "programmerRepresentationsText",
    "programmer_representations_text()",
):
    assert needle in ios_bridge, f"iPhone Programmer representations bridge missing: {needle}"

for needle in (
    "ProgrammerRepresentationsView",
    "model.showingBases",
    "model.programmerRepresentationsText()",
    ".textSelection(.enabled)",
):
    assert needle in ios, f"iPhone Programmer/copy completeness missing: {needle}"

for needle in (
    "controller.history_count()",
    "controller.history_entry(index)",
    "controller.recall_history(index)",
):
    assert needle in linux, f"Linux structured history replay missing: {needle}"

for needle in (
    "LB_RESETCONTENT",
    "LBS_NOTIFY",
    "LBN_DBLCLK",
    "g_controller.history_entry(index)",
    "g_controller.recall_history(",
):
    assert needle in windows, f"Windows structured history replay missing: {needle}"

for needle in (
    "historyEntries",
    "recallHistoryAtIndex",
):
    assert needle in ios_bridge, f"iPhone structured history bridge missing: {needle}"

for needle in (
    "model.historyEntries()",
    "model.recallHistory(entry.id)",
):
    assert needle in ios, f"iPhone structured history UI missing: {needle}"
