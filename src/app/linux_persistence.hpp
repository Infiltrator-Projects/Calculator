/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "../ui/calculator_ui_controller.hpp"
#include "../ui/calculator_ui_contract.hpp"
#include "../ui/calculator_theme.hpp"

#include <string>

namespace calculator::app {

struct LinuxConversionPreferences {
    std::string dimension = "length";
    std::string from = "km";
    std::string to = "mi";
};

struct LinuxDesktopState {
    int width = ui::kDesktopMetrics.default_width;
    int height = ui::desktop_preferred_height(ui::Mode::Standard);
    ui::Mode selected_mode = ui::Mode::Standard;
    int angle_code = static_cast<int>(AngleUnit::Degrees);
    int programmer_base_code = static_cast<int>(ProgrammerBase::Decimal);
    int programmer_width_code = static_cast<int>(IntegerWidth::Bits64);
    bool programmer_signed_flag = false;
};

class LinuxPersistence {
public:
    explicit LinuxPersistence(ui::Controller& controller) noexcept : controller_(controller) {}
    ui::ThemeMode load_theme_mode() const;
    void save_theme_mode(ui::ThemeMode mode) const;
    bool load_controller_state();
    bool save_controller_state() const;
    LinuxDesktopState load_desktop_state() const;
    void save_desktop_state(int width, int height) const;
    LinuxConversionPreferences load_conversion_preferences() const;
    void save_conversion_preferences(const LinuxConversionPreferences& preferences) const;
private:
    void load_legacy_variables();
    void load_legacy_functions();
    void load_legacy_display_preferences();
    ui::Controller& controller_;
    bool controller_state_loaded_ = false;
    bool shared_semantic_state_ = false;
    mutable std::string baseline_state_;
};

} // namespace calculator::app
