/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <infiltratr/design.h>

#include <cstdint>
#include <string_view>

namespace infiltrator::calc::ui {

/*
 * Calc owns the platform adapter, not the palette. Common is the source of
 * truth for System/Day/Night policy and every semantic colour value.
 */
enum class ThemeMode {
    System = INFILTRATR_THEME_SYSTEM,
    Day = INFILTRATR_THEME_DAY,
    Night = INFILTRATR_THEME_NIGHT
};

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

inline ThemePalette from_common(const InfiltratrThemePalette& source) {
    return {
        source.background_rgb,
        source.panel_rgb,
        source.card_rgb,
        source.surface_rgb,
        source.input_rgb,
        source.border_rgb,
        source.text_rgb,
        source.title_rgb,
        source.muted_rgb,
        source.subtle_rgb,
        source.button_background_rgb,
        source.button_foreground_rgb,
        source.selection_background_rgb,
        source.selection_foreground_rgb,
        source.neutral_accent_rgb,
        source.success_rgb,
        source.warning_rgb,
        source.fault_rgb,
        source.info_rgb,
        source.operation_rgb,
        source.card_hover_rgb,
        source.surface_hover_rgb,
        source.operation_hover_rgb,
        source.equals_hover_rgb
    };
}

inline std::string_view theme_mode_name(ThemeMode mode) {
    return infiltratr_theme_mode_name(
        static_cast<InfiltratrThemeMode>(mode));
}

inline ThemeMode next_theme_mode(ThemeMode mode) {
    return static_cast<ThemeMode>(
        infiltratr_theme_mode_next(
            static_cast<InfiltratrThemeMode>(mode)));
}

inline const ThemePalette& resolved_palette(bool dark) {
    static const ThemePalette day = from_common(
        *infiltratr_theme_resolve(INFILTRATR_THEME_DAY, false));
    static const ThemePalette night = from_common(
        *infiltratr_theme_resolve(INFILTRATR_THEME_NIGHT, true));
    return dark ? night : day;
}

} // namespace infiltrator::calc::ui
