/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "../ui/calculator_theme.hpp"
#include "../ui/calculator_ui_controller.hpp"
namespace calculator::app {
class WindowsPersistence {
public:
    explicit WindowsPersistence(ui::Controller& controller) noexcept : controller_(controller) {}
    ui::ThemeMode load_theme_mode() const;
    void save_theme_mode(ui::ThemeMode mode) const;
    void load_controller_state();
    void save_controller_state() const;
private:
    ui::Controller& controller_;
};
} // namespace calculator::app
