/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <infiltratr/design.h>

#include <string_view>

namespace calculator::ui {

enum class ThemeMode {
    System = INFILTRATR_THEME_SYSTEM,
    Day = INFILTRATR_THEME_DAY,
    Night = INFILTRATR_THEME_NIGHT
};

using ThemePalette = InfiltratrThemePalette;
using DesignMetrics = InfiltratrDesignMetrics;
using Typography = InfiltratrTypography;

inline std::string_view theme_mode_name(ThemeMode mode) {
    return infiltratr_theme_mode_name(
        static_cast<InfiltratrThemeMode>(mode));
}

inline ThemeMode next_theme_mode(ThemeMode mode) {
    return static_cast<ThemeMode>(
        infiltratr_theme_mode_next(
            static_cast<InfiltratrThemeMode>(mode)));
}

inline const char* theme_mode_key(ThemeMode mode) {
    const char* key = infiltratr_theme_mode_key(
        static_cast<InfiltratrThemeMode>(mode));
    return key ? key : "system";
}

inline bool theme_mode_parse(const char* text, ThemeMode& mode) {
    InfiltratrThemeMode parsed = INFILTRATR_THEME_SYSTEM;
    if (!infiltratr_theme_mode_parse(text, &parsed)) return false;
    mode = static_cast<ThemeMode>(parsed);
    return true;
}

inline const ThemePalette& resolved_palette(ThemeMode mode,
                                            bool system_is_dark) {
    return *infiltratr_theme_resolve(
        static_cast<InfiltratrThemeMode>(mode), system_is_dark);
}

inline const DesignMetrics& design_metrics() {
    return *infiltratr_design_metrics();
}

inline const Typography& typography() {
    return *infiltratr_typography();
}

} // namespace calculator::ui
