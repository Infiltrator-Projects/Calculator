/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace infiltrator::calc::ui {

enum class ThemeMode { System = 0, Day = 1, Night = 2 };

struct ThemePalette {
    std::uint32_t background;
    std::uint32_t panel;
    std::uint32_t card;
    std::uint32_t surface;
    std::uint32_t input;
    std::uint32_t border;
    std::uint32_t text;
    std::uint32_t title;
    std::uint32_t muted;
    std::uint32_t subtle;
    std::uint32_t button_background;
    std::uint32_t button_foreground;
    std::uint32_t selection_background;
    std::uint32_t selection_foreground;
    std::uint32_t neutral_accent;
    std::uint32_t success;
    std::uint32_t warning;
    std::uint32_t fault;
    std::uint32_t info;
    std::uint32_t operation;
    std::uint32_t card_hover;
    std::uint32_t surface_hover;
    std::uint32_t operation_hover;
    std::uint32_t equals_hover;
};

inline constexpr ThemePalette kNightPalette{
    0x050608, 0x101318, 0x171B20, 0x0D1014, 0x0E1115,
    0x353A40, 0xE8ECEF, 0xEEF1F3, 0xAEB6BD, 0x899198,
    0xD7DDE2, 0x111418, 0x2B3137, 0xEEF1F3, 0xBEC7CF,
    0x63AB7C, 0xD19E47, 0xC96B6B, 0x7FA7C9,
    0x20252B, 0x22272D, 0x171B20, 0x2B3137, 0xEEF1F3
};

inline constexpr ThemePalette kDayPalette{
    0xF4F5F7, 0xFFFFFF, 0xF8F9FA, 0xECEFF2, 0xFFFFFF,
    0xC7CDD3, 0x20252B, 0x111418, 0x59636C, 0x737D86,
    0x20252B, 0xFFFFFF, 0xDDE2E7, 0x111418, 0x6F7881,
    0x3A8A58, 0x9A6500, 0xB54848, 0x467AA3,
    0xE8ECEF, 0xEEF1F3, 0xF1F3F5, 0xDDE2E7, 0x343B42
};

inline constexpr std::string_view theme_mode_name(ThemeMode mode) {
    switch (mode) {
    case ThemeMode::System: return "System";
    case ThemeMode::Day: return "Day";
    case ThemeMode::Night: return "Night";
    }
    return "System";
}

inline constexpr ThemeMode next_theme_mode(ThemeMode mode) {
    switch (mode) {
    case ThemeMode::System: return ThemeMode::Day;
    case ThemeMode::Day: return ThemeMode::Night;
    case ThemeMode::Night: return ThemeMode::System;
    }
    return ThemeMode::System;
}

inline constexpr const ThemePalette& resolved_palette(bool dark) {
    return dark ? kNightPalette : kDayPalette;
}

} // namespace infiltrator::calc::ui
