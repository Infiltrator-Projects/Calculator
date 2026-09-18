/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <infiltratr/design.h>

#include <string_view>

namespace infiltrator::calc::ui {

enum class ThemeMode {
    System = INFILTRATR_THEME_SYSTEM,
    Day = INFILTRATR_THEME_DAY,
    Night = INFILTRATR_THEME_NIGHT
};

using ThemePalette = InfiltratrThemePalette;

inline std::string_view theme_mode_name(ThemeMode mode) {
    return infiltratr_theme_mode_name(
        static_cast<InfiltratrThemeMode>(mode));
}

inline ThemeMode next_theme_mode(ThemeMode mode) {
    return static_cast<ThemeMode>(
        infiltratr_theme_mode_next(
            static_cast<InfiltratrThemeMode>(mode)));
}

inline const ThemePalette& resolved_palette(ThemeMode mode,
                                            bool system_is_dark) {
    return *infiltratr_theme_resolve(
        static_cast<InfiltratrThemeMode>(mode), system_is_dark);
}

} // namespace infiltrator::calc::ui
