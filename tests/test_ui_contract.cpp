/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/ui/calculator_ui_contract.hpp"
#include "../src/ui/calculator_theme.hpp"

#include <iostream>
#include <string_view>

using namespace calculator::ui;

namespace {
int failures = 0;

void check(bool condition, const char* expression) {
    if (!condition) {
        std::cerr << "FAIL: " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression)
} // namespace

int main() {
    static_assert(kStandardMemory.size() == 4);
    static_assert(kStandardKeypad.size() == 24);
    static_assert(kScientificKeypad.size() == 40);
    static_assert(kProgrammerKeypad.size() == 40);

    static_assert(kDesktopMetrics.default_width == 360);
    static_assert(kDesktopMetrics.default_height == 610);
    static_assert(kDesktopMetrics.minimum_width == 320);
    static_assert(kDesktopMetrics.minimum_height == 520);
    static_assert(kDesktopMetrics.standard_height == 480);
    static_assert(kDesktopMetrics.standard_minimum_height == 480);
    static_assert(desktop_preferred_height(Mode::Standard) == 480);
    static_assert(desktop_preferred_height(Mode::Scientific) == 610);
    static_assert(desktop_preferred_height(Mode::Programmer) == 610);
    static_assert(desktop_minimum_height(Mode::Standard) == 480);
    static_assert(desktop_minimum_height(Mode::Scientific) == 520);
    static_assert(desktop_minimum_height(Mode::Programmer) == 520);
    static_assert(kDesktopMetrics.grid_gap_x == 6);
    static_assert(kDesktopMetrics.grid_gap_y == 5);
    static_assert(kDesktopMetrics.key_min_height == 32);
    static_assert(kDesktopMetrics.memory_height == 24);
    static_assert(kDesktopMetrics.wide_threshold == 720);
    static_assert(kDesktopMetrics.compact_width_threshold == 340);
    static_assert(kDesktopMetrics.compact_height_threshold == 560);
    static_assert(kDesktopMetrics.history_min_width == 240);

    constexpr auto compact = responsive_layout(320, 520);
    constexpr auto regular = responsive_layout(360, 610);
    constexpr auto wide = responsive_layout(900, 610);
    static_assert(compact.layout_class == LayoutClass::Compact);
    static_assert(compact.compact_controls);
    static_assert(!compact.dock_history);
    static_assert(regular.layout_class == LayoutClass::Regular);
    static_assert(!regular.dock_history);
    static_assert(wide.layout_class == LayoutClass::Wide);
    static_assert(wide.dock_history);

    const auto& night = resolved_palette(ThemeMode::Night, false);
    const auto& day = resolved_palette(ThemeMode::Day, true);
    const auto& system_night = resolved_palette(ThemeMode::System, true);
    CHECK(night.background_rgb == 0x050608);
    CHECK(night.panel_rgb == 0x101318);
    CHECK(day.background_rgb == 0xFFFFFF);
    CHECK(day.panel_rgb == 0xFFFFFF);
    CHECK(system_night.background_rgb == night.background_rgb);
    CHECK(theme_mode_name(ThemeMode::System) == "System");
    CHECK(theme_mode_name(ThemeMode::Day) == "Day");
    CHECK(theme_mode_name(ThemeMode::Night) == "Night");
    CHECK(next_theme_mode(ThemeMode::System) == ThemeMode::Day);
    CHECK(next_theme_mode(ThemeMode::Day) == ThemeMode::Night);
    CHECK(next_theme_mode(ThemeMode::Night) == ThemeMode::System);

    CHECK(mode_name(Mode::Standard) == "Standard");
    CHECK(mode_name(Mode::Scientific) == "Scientific");
    CHECK(mode_name(Mode::Programmer) == "Programmer");

    CHECK(kStandardMemory[0].command == Command::MemoryClear);
    CHECK(kStandardMemory[1].command == Command::MemoryRecall);
    CHECK(kStandardMemory[2].command == Command::MemoryAdd);
    CHECK(kStandardMemory[3].command == Command::MemorySubtract);

    CHECK(kStandardKeypad.front().label == "%");
    CHECK(kStandardKeypad.back().command == Command::Equals);
    CHECK(kScientificKeypad.front().command == Command::ToggleDegrees);
    CHECK(kScientificKeypad.back().command == Command::Equals);
    CHECK(kProgrammerKeypad.front().command == Command::BaseBin);
    CHECK(kProgrammerKeypad.back().command == Command::HexF);

    CHECK(insertion_text(Command::Divide) == "/");
    CHECK(insertion_text(Command::Multiply) == "*");
    CHECK(insertion_text(Command::Subtract) == "-");
    CHECK(insertion_text(Command::Add) == "+");
    CHECK(insertion_text(Command::ShiftLeft) == "<<");
    CHECK(insertion_text(Command::ShiftRight) == ">>");

    CHECK(is_programmer_selector(Command::BaseHex));
    CHECK(is_programmer_selector(Command::Width64));
    CHECK(is_programmer_selector(Command::ToggleSigned));
    CHECK(!is_programmer_selector(Command::Equals));

    if (failures != 0) {
        std::cerr << failures << " UI contract test(s) failed\n";
        return 1;
    }
    std::cout << "UI contract tests passed\n";
    return 0;
}
