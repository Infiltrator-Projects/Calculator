/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../ui/calculator_ui_controller.hpp"
#include "../core/advanced_tools.hpp"
#include "../ui/calculator_theme.hpp"

#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

using calculator::ui::ButtonRole;
using calculator::ui::ButtonSpec;
using calculator::ui::Command;
using calculator::ui::Controller;
using calculator::ui::DisplayPreferences;
using calculator::ui::LayoutClass;
using calculator::ui::Mode;
using calculator::ui::ResultFormat;
using calculator::ui::ThemeMode;
using calculator::ui::ThemePalette;

GtkWidget* expression_entry = nullptr;
GtkWidget* result_label = nullptr;
GtkWidget* status_label = nullptr;
GtkWidget* standard_panel = nullptr;
GtkWidget* scientific_grid = nullptr;
GtkWidget* programmer_grid = nullptr;
GtkWidget* mode_buttons[3] = {nullptr, nullptr, nullptr};
GtkWidget* programmer_base_buttons[4] = {nullptr, nullptr, nullptr, nullptr};
GtkWidget* programmer_width_buttons[4] = {nullptr, nullptr, nullptr, nullptr};
GtkWidget* programmer_signed_button = nullptr;
GtkWidget* angle_button = nullptr;
GtkWidget* second_button = nullptr;
GtkWidget* hyperbolic_button = nullptr;
GtkWidget* notation_button = nullptr;
GtkWidget* history_button = nullptr;
GtkWidget* tools_button = nullptr;
GtkWidget* results_button = nullptr;
GtkWidget* bases_button = nullptr;
GtkWidget* theme_button = nullptr;
GtkWidget* preferences_button = nullptr;
GtkWidget* main_window = nullptr;
GtkWidget* tools_window = nullptr;
GtkWidget* preferences_window = nullptr;
GtkWidget* programmer_bits_window = nullptr;
GtkWidget* programmer_bits_value = nullptr;
std::vector<GtkWidget*> programmer_bit_buttons;
GtkWidget* history_dock = nullptr;
GtkWidget* history_text = nullptr;
GtkWidget* calculator_column = nullptr;
GtkCssProvider* css_provider = nullptr;

std::vector<std::pair<GtkWidget*, const ButtonSpec*>> command_buttons;
LayoutClass last_layout_class = LayoutClass::Regular;
bool responsive_layout_initialized = false;
int history_navigation_index = -1;

Controller controller;
ThemeMode theme_mode = ThemeMode::System;
bool effective_dark_theme = true;

// Calculator has exactly three approved MB Corpo faces: S Regular, S Bold
// and A Condensed Regular. GTK selects the S regular/bold face by weight and
// the A condensed face for display text; no fourth family is selected here.
std::string hex_colour(std::uint32_t value) {
    char buffer[8] = {};
    std::snprintf(buffer, sizeof(buffer), "#%06X", value & 0xFFFFFFU);
    return buffer;
}

bool system_prefers_dark() {
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings) return false;

    gboolean prefer_dark = FALSE;
    gchar* theme_name = nullptr;
    g_object_get(
        settings,
        "gtk-application-prefer-dark-theme", &prefer_dark,
        "gtk-theme-name", &theme_name,
        nullptr);

    bool dark = prefer_dark != FALSE;
    if (theme_name) {
        std::string name(theme_name);
        std::transform(
            name.begin(), name.end(), name.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        dark = dark || name.find("dark") != std::string::npos;
        g_free(theme_name);
    }
    return dark;
}

ThemeMode load_theme_mode() {
    gchar* path = g_build_filename(
        g_get_user_config_dir(), "infiltrator-calc", "theme", nullptr);
    gchar* contents = nullptr;
    gsize length = 0;
    ThemeMode mode = ThemeMode::System;
    if (g_file_get_contents(path, &contents, &length, nullptr) && contents) {
        std::string value(contents, length);
        while (!value.empty() &&
               std::isspace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }
        if (value == "day") mode = ThemeMode::Day;
        else if (value == "night") mode = ThemeMode::Night;
    }
    g_free(contents);
    g_free(path);
    return mode;
}

bool user_functions_loaded = false;
bool user_variables_loaded = false;

void load_user_variables() {
    if (user_variables_loaded) return;
    user_variables_loaded = true;
    gchar* path = g_build_filename(
        g_get_user_data_dir(), "infiltrator-calc", "variables", nullptr);
    gchar* contents = nullptr;
    gsize length = 0;
    if (g_file_get_contents(path, &contents, &length, nullptr) && contents) {
        if (!controller.load_variables_text(
                std::string_view(contents, length))) {
            g_printerr(
                "Calculator ignored malformed variables data at %s\n", path);
        }
    }
    g_free(contents);
    g_free(path);
}

void save_user_variables() {
    gchar* directory = g_build_filename(
        g_get_user_data_dir(), "infiltrator-calc", nullptr);
    if (g_mkdir_with_parents(directory, 0700) == 0) {
        gchar* path = g_build_filename(directory, "variables", nullptr);
        const std::string text = controller.variables_text();
        if (!g_file_set_contents(
                path, text.data(), static_cast<gssize>(text.size()), nullptr)) {
            g_printerr(
                "Calculator could not persist variables to %s\n", path);
        }
        g_free(path);
    }
    g_free(directory);
}

void load_user_functions() {
    if (user_functions_loaded) return;
    user_functions_loaded = true;

    gchar* path = g_build_filename(
        g_get_user_data_dir(), "infiltrator-calc", "custom-functions", nullptr);
    gchar* contents = nullptr;
    gsize length = 0;
    if (g_file_get_contents(path, &contents, &length, nullptr) && contents) {
        if (!controller.load_function_definitions_text(
                std::string_view(contents, length))) {
            g_printerr(
                "Calculator ignored malformed custom-functions data at %s\n",
                path);
        }
    }
    g_free(contents);
    g_free(path);
}

void save_user_functions() {
    gchar* directory = g_build_filename(
        g_get_user_data_dir(), "infiltrator-calc", nullptr);
    if (g_mkdir_with_parents(directory, 0700) == 0) {
        gchar* path = g_build_filename(directory, "custom-functions", nullptr);
        const std::string text = controller.function_definitions_text();
        if (!g_file_set_contents(
                path, text.data(), static_cast<gssize>(text.size()), nullptr)) {
            g_printerr(
                "Calculator could not persist custom functions to %s\n",
                path);
        }
        g_free(path);
    }
    g_free(directory);
}

const char* result_format_name(ResultFormat format) {
    switch (format) {
    case ResultFormat::Fixed: return "fixed";
    case ResultFormat::Scientific: return "scientific";
    case ResultFormat::Engineering: return "engineering";
    case ResultFormat::Automatic: return "automatic";
    }
    return "automatic";
}

ResultFormat result_format_from_name(const char* name) {
    if (!name) return ResultFormat::Automatic;
    if (g_strcmp0(name, "fixed") == 0) return ResultFormat::Fixed;
    if (g_strcmp0(name, "scientific") == 0) return ResultFormat::Scientific;
    if (g_strcmp0(name, "engineering") == 0) return ResultFormat::Engineering;
    return ResultFormat::Automatic;
}

void load_display_preferences() {
    gchar* path = g_build_filename(
        g_get_user_config_dir(), "infiltrator-calc",
        "presentation.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    DisplayPreferences prefs = controller.display_preferences();
    if (g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, nullptr)) {
        if (g_key_file_has_key(key, "Presentation", "format", nullptr)) {
            gchar* value = g_key_file_get_string(
                key, "Presentation", "format", nullptr);
            prefs.format = result_format_from_name(value);
            g_free(value);
        }
        if (g_key_file_has_key(
                key, "Presentation", "decimal-places", nullptr)) {
            const gint value = g_key_file_get_integer(
                key, "Presentation", "decimal-places", nullptr);
            prefs.decimal_places =
                static_cast<unsigned>(std::clamp(value, 0, 15));
        }
        if (g_key_file_has_key(
                key, "Presentation", "group-thousands", nullptr)) {
            prefs.group_thousands = g_key_file_get_boolean(
                key, "Presentation", "group-thousands", nullptr);
        }
        if (g_key_file_has_key(
                key, "Presentation", "trailing-zeroes", nullptr)) {
            prefs.trailing_zeroes = g_key_file_get_boolean(
                key, "Presentation", "trailing-zeroes", nullptr);
        }
    }
    controller.set_display_preferences(prefs);
    g_key_file_unref(key);
    g_free(path);
}

void save_display_preferences() {
    gchar* directory = g_build_filename(
        g_get_user_config_dir(), "infiltrator-calc", nullptr);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_free(directory);
        return;
    }
    gchar* path = g_build_filename(
        directory, "presentation.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    const DisplayPreferences& prefs = controller.display_preferences();
    g_key_file_set_string(
        key, "Presentation", "format", result_format_name(prefs.format));
    g_key_file_set_integer(
        key, "Presentation", "decimal-places",
        static_cast<gint>(prefs.decimal_places));
    g_key_file_set_boolean(
        key, "Presentation", "group-thousands", prefs.group_thousands);
    g_key_file_set_boolean(
        key, "Presentation", "trailing-zeroes", prefs.trailing_zeroes);

    gsize length = 0;
    gchar* data = g_key_file_to_data(key, &length, nullptr);
    if (data) {
        if (!g_file_set_contents(
                path, data, static_cast<gssize>(length), nullptr)) {
            g_printerr(
                "Calculator could not persist presentation preferences to %s\n",
                path);
        }
        g_free(data);
    }
    g_key_file_unref(key);
    g_free(path);
    g_free(directory);
}

struct DesktopState {
    int width = calculator::ui::kDesktopMetrics.default_width;
    int height = calculator::ui::desktop_preferred_height(Mode::Standard);
    Mode selected_mode = Mode::Standard;
    int angle_code = static_cast<int>(calculator::AngleUnit::Degrees);
    int programmer_base_code =
        static_cast<int>(calculator::ProgrammerBase::Decimal);
    int programmer_width_code =
        static_cast<int>(calculator::IntegerWidth::Bits64);
    bool programmer_signed_flag = false;
};

DesktopState load_desktop_state() {
    DesktopState state;
    gchar* path = g_build_filename(
        g_get_user_config_dir(), "infiltrator-calc",
        "desktop.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    if (g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, nullptr)) {
        if (g_key_file_has_key(key, "Window", "width", nullptr)) {
            state.width = g_key_file_get_integer(
                key, "Window", "width", nullptr);
        }
        if (g_key_file_has_key(key, "Window", "height", nullptr)) {
            state.height = g_key_file_get_integer(
                key, "Window", "height", nullptr);
        }
        if (g_key_file_has_key(key, "Window", "mode", nullptr)) {
            const gint mode = g_key_file_get_integer(
                key, "Window", "mode", nullptr);
            if (mode >= static_cast<gint>(Mode::Standard) &&
                mode <= static_cast<gint>(Mode::Programmer)) {
                state.selected_mode = static_cast<Mode>(mode);
            }
        }
        if (g_key_file_has_key(key, "Calculator", "angle-unit", nullptr)) {
            const gint angle = g_key_file_get_integer(
                key, "Calculator", "angle-unit", nullptr);
            if (angle >= static_cast<gint>(calculator::AngleUnit::Degrees) &&
                angle <= static_cast<gint>(calculator::AngleUnit::Gradians)) {
                state.angle_code = angle;
            }
        }
        if (g_key_file_has_key(key, "Calculator", "programmer-base", nullptr)) {
            const gint base = g_key_file_get_integer(
                key, "Calculator", "programmer-base", nullptr);
            if (base >= static_cast<gint>(calculator::ProgrammerBase::Binary) &&
                base <= static_cast<gint>(calculator::ProgrammerBase::Hexadecimal)) {
                state.programmer_base_code = base;
            }
        }
        if (g_key_file_has_key(key, "Calculator", "programmer-width", nullptr)) {
            const gint width = g_key_file_get_integer(
                key, "Calculator", "programmer-width", nullptr);
            if (width == 8 || width == 16 || width == 32 || width == 64) {
                state.programmer_width_code = width;
            }
        }
        if (g_key_file_has_key(key, "Calculator", "programmer-signed", nullptr)) {
            state.programmer_signed_flag = g_key_file_get_boolean(
                key, "Calculator", "programmer-signed", nullptr);
        }
    }
    g_key_file_unref(key);
    g_free(path);

    const auto& metrics = calculator::ui::kDesktopMetrics;
    state.width = std::clamp(
        state.width, metrics.minimum_width, 2000);
    state.height = std::clamp(
        state.height,
        calculator::ui::desktop_minimum_height(state.selected_mode), 1600);
    return state;
}

void save_desktop_state(GtkWidget* window) {
    if (!window) return;
    gchar* directory = g_build_filename(
        g_get_user_config_dir(), "infiltrator-calc", nullptr);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_free(directory);
        return;
    }
    gchar* path = g_build_filename(directory, "desktop.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    g_key_file_set_integer(
        key, "Window", "width", gtk_widget_get_width(window));
    g_key_file_set_integer(
        key, "Window", "height", gtk_widget_get_height(window));
    g_key_file_set_integer(
        key, "Window", "mode",
        static_cast<gint>(controller.state().mode));
    g_key_file_set_integer(
        key, "Calculator", "angle-unit",
        static_cast<gint>(controller.state().angle_unit));
    g_key_file_set_integer(
        key, "Calculator", "programmer-base",
        static_cast<gint>(controller.state().programmer_base));
    g_key_file_set_integer(
        key, "Calculator", "programmer-width",
        static_cast<gint>(controller.state().programmer_width));
    g_key_file_set_boolean(
        key, "Calculator", "programmer-signed",
        controller.state().programmer_signed);

    gsize length = 0;
    gchar* data = g_key_file_to_data(key, &length, nullptr);
    if (data) {
        (void)g_file_set_contents(
            path, data, static_cast<gssize>(length), nullptr);
        g_free(data);
    }
    g_key_file_unref(key);
    g_free(path);
    g_free(directory);
}

gboolean on_main_close_request(GtkWindow* window, gpointer) {
    save_desktop_state(GTK_WIDGET(window));
    save_user_variables();
    save_user_functions();
    save_display_preferences();
    return FALSE;
}

void save_theme_mode() {
    gchar* directory = g_build_filename(
        g_get_user_config_dir(), "infiltrator-calc", nullptr);
    if (g_mkdir_with_parents(directory, 0700) == 0) {
        gchar* path = g_build_filename(directory, "theme", nullptr);
        const char* value = "system\n";
        if (theme_mode == ThemeMode::Day) value = "day\n";
        else if (theme_mode == ThemeMode::Night) value = "night\n";
        (void)g_file_set_contents(path, value, -1, nullptr);
        g_free(path);
    }
    g_free(directory);
}

const ThemePalette& active_palette() {
    const bool system_dark = system_prefers_dark();
    effective_dark_theme =
        theme_mode == ThemeMode::Night ||
        (theme_mode == ThemeMode::System && system_dark);
    return calculator::ui::resolved_palette(theme_mode, system_dark);
}

void refresh_history_dock() {
    if (!history_text) return;
    GtkTextBuffer* buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(history_text));
    const std::string text = controller.history_text(50, "\n");
    gtk_text_buffer_set_text(buffer, text.c_str(), -1);
}

bool font_family_available(const char* wanted) {
    PangoFontMap* map = PANGO_FONT_MAP(pango_cairo_font_map_get_default());
    PangoFontFamily** families = nullptr;
    int count = 0;
    pango_font_map_list_families(map, &families, &count);

    bool found = false;
    for (int i = 0; i < count; ++i) {
        const char* name = pango_font_family_get_name(families[i]);
        if (name && g_ascii_strcasecmp(name, wanted) == 0) {
            found = true;
            break;
        }
    }
    g_free(families);
    return found;
}

const char* ui_font() {
    return calculator::ui::typography().ui_family;
}

const char* brand_font() {
    return calculator::ui::typography().brand_family;
}

void sync_expression_from_widget() {
    const char* text = gtk_editable_get_text(GTK_EDITABLE(expression_entry));
    controller.set_expression(text ? text : "");
}

void apply_selected(GtkWidget* button, bool selected) {
    if (!button) return;
    if (selected) gtk_widget_add_css_class(button, "selected");
    else gtk_widget_remove_css_class(button, "selected");
}

void render_state(std::size_t cursor = Controller::kEnd) {
    const auto& state = controller.state();

    const char* current = gtk_editable_get_text(GTK_EDITABLE(expression_entry));
    if (!current || state.expression != current) {
        gtk_editable_set_text(
            GTK_EDITABLE(expression_entry), state.expression.c_str());
    }

    const std::size_t effective_cursor =
        cursor == Controller::kEnd ? state.expression.size() : cursor;
    gtk_editable_set_position(
        GTK_EDITABLE(expression_entry),
        static_cast<int>(effective_cursor));

    gtk_label_set_text(GTK_LABEL(result_label), state.result.c_str());
    gtk_label_set_text(GTK_LABEL(status_label), state.status.c_str());
    if (state.fault) gtk_widget_add_css_class(status_label, "fault");
    else gtk_widget_remove_css_class(status_label, "fault");

    gtk_widget_set_visible(standard_panel, state.mode == Mode::Standard);
    gtk_widget_set_visible(scientific_grid, state.mode == Mode::Scientific);
    gtk_widget_set_visible(programmer_grid, state.mode == Mode::Programmer);
    if (results_button) {
        gtk_widget_set_visible(
            results_button, state.mode != Mode::Programmer);
    }
    if (bases_button) {
        gtk_widget_set_visible(
            bases_button, state.mode == Mode::Programmer);
    }

    for (int i = 0; i < 3; ++i) {
        apply_selected(
            mode_buttons[i],
            i == static_cast<int>(state.mode));
    }

    apply_selected(
        programmer_base_buttons[0],
        state.programmer_base == calculator::ProgrammerBase::Binary);
    apply_selected(
        programmer_base_buttons[1],
        state.programmer_base == calculator::ProgrammerBase::Octal);
    apply_selected(
        programmer_base_buttons[2],
        state.programmer_base == calculator::ProgrammerBase::Decimal);
    apply_selected(
        programmer_base_buttons[3],
        state.programmer_base == calculator::ProgrammerBase::Hexadecimal);

    apply_selected(
        programmer_width_buttons[0],
        state.programmer_width == calculator::IntegerWidth::Bits8);
    apply_selected(
        programmer_width_buttons[1],
        state.programmer_width == calculator::IntegerWidth::Bits16);
    apply_selected(
        programmer_width_buttons[2],
        state.programmer_width == calculator::IntegerWidth::Bits32);
    apply_selected(
        programmer_width_buttons[3],
        state.programmer_width == calculator::IntegerWidth::Bits64);
    apply_selected(programmer_signed_button, state.programmer_signed);
    apply_selected(second_button, state.scientific_second);
    apply_selected(hyperbolic_button, state.scientific_hyperbolic);
    apply_selected(notation_button, state.scientific_notation);

    for (const auto& [button, spec] : command_buttons) {
        if (spec == nullptr) continue;
        const std::string label =
            controller.button_label(spec->command, spec->label);
        gtk_button_set_label(GTK_BUTTON(button), label.c_str());
        gtk_widget_set_sensitive(
            button, controller.command_enabled(spec->command));
    }

    refresh_history_dock();
}

struct PreferencesWindowState {
    GtkWidget* format = nullptr;
    GtkWidget* decimals = nullptr;
    GtkWidget* grouping = nullptr;
    GtkWidget* zeroes = nullptr;
};

void apply_preferences_window(PreferencesWindowState* state) {
    if (!state) return;
    DisplayPreferences prefs = controller.display_preferences();
    switch (gtk_drop_down_get_selected(GTK_DROP_DOWN(state->format))) {
    case 1U: prefs.format = ResultFormat::Fixed; break;
    case 2U: prefs.format = ResultFormat::Scientific; break;
    case 3U: prefs.format = ResultFormat::Engineering; break;
    default: prefs.format = ResultFormat::Automatic; break;
    }
    prefs.decimal_places = static_cast<unsigned>(
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(state->decimals)));
    prefs.group_thousands =
        gtk_switch_get_active(GTK_SWITCH(state->grouping));
    prefs.trailing_zeroes =
        gtk_switch_get_active(GTK_SWITCH(state->zeroes));
    controller.set_display_preferences(prefs);
    save_display_preferences();
    render_state();
}

void on_preferences_changed(GObject*, GParamSpec*, gpointer data) {
    apply_preferences_window(
        static_cast<PreferencesWindowState*>(data));
}

void show_preferences(GtkWidget*, gpointer) {
    if (preferences_window) {
        gtk_window_present(GTK_WINDOW(preferences_window));
        return;
    }

    GtkApplication* app =
        main_window
            ? gtk_window_get_application(GTK_WINDOW(main_window))
            : nullptr;
    GtkWidget* window =
        app ? gtk_application_window_new(app) : gtk_window_new();
    preferences_window = window;
    g_object_add_weak_pointer(
        G_OBJECT(window),
        reinterpret_cast<gpointer*>(&preferences_window));
    gtk_window_set_title(GTK_WINDOW(window), "Calculator Preferences");
    gtk_window_set_default_size(GTK_WINDOW(window), 430, 330);
    gtk_window_set_hide_on_close(GTK_WINDOW(window), TRUE);
    if (main_window) {
        gtk_window_set_transient_for(
            GTK_WINDOW(window), GTK_WINDOW(main_window));
        gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
    }

    auto* state = new PreferencesWindowState();
    g_object_set_data_full(
        G_OBJECT(window), "calculator-preferences-state", state,
        +[](gpointer data) {
            delete static_cast<PreferencesWindowState*>(data);
        });

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(root, "shell");
    gtk_window_set_child(GTK_WINDOW(window), root);

    GtkWidget* title = gtk_label_new("Result Presentation");
    gtk_widget_add_css_class(title, "brand-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(root), title);

    const char* formats[] = {
        "Automatic", "Fixed", "Scientific", "Engineering", nullptr
    };
    GtkStringList* model = gtk_string_list_new(formats);
    state->format = gtk_drop_down_new(G_LIST_MODEL(model), nullptr);
    g_object_unref(model);
    gtk_widget_set_hexpand(state->format, TRUE);
    gtk_box_append(GTK_BOX(root), state->format);

    GtkWidget* decimal_row =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* decimal_label = gtk_label_new("Decimal places");
    gtk_widget_set_hexpand(decimal_label, TRUE);
    gtk_widget_set_halign(decimal_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(decimal_row), decimal_label);
    state->decimals = gtk_spin_button_new_with_range(0.0, 15.0, 1.0);
    gtk_box_append(GTK_BOX(decimal_row), state->decimals);
    gtk_box_append(GTK_BOX(root), decimal_row);

    GtkWidget* grouping_row =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* grouping_label =
        gtk_label_new("Thousands separators");
    gtk_widget_set_hexpand(grouping_label, TRUE);
    gtk_widget_set_halign(grouping_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(grouping_row), grouping_label);
    state->grouping = gtk_switch_new();
    gtk_box_append(GTK_BOX(grouping_row), state->grouping);
    gtk_box_append(GTK_BOX(root), grouping_row);

    GtkWidget* zero_row =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* zero_label = gtk_label_new("Show trailing zeroes");
    gtk_widget_set_hexpand(zero_label, TRUE);
    gtk_widget_set_halign(zero_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(zero_row), zero_label);
    state->zeroes = gtk_switch_new();
    gtk_box_append(GTK_BOX(zero_row), state->zeroes);
    gtk_box_append(GTK_BOX(root), zero_row);

    const DisplayPreferences& prefs = controller.display_preferences();
    guint selected = 0U;
    if (prefs.format == ResultFormat::Fixed) selected = 1U;
    else if (prefs.format == ResultFormat::Scientific) selected = 2U;
    else if (prefs.format == ResultFormat::Engineering) selected = 3U;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(state->format), selected);
    gtk_spin_button_set_value(
        GTK_SPIN_BUTTON(state->decimals), prefs.decimal_places);
    gtk_switch_set_active(
        GTK_SWITCH(state->grouping), prefs.group_thousands);
    gtk_switch_set_active(
        GTK_SWITCH(state->zeroes), prefs.trailing_zeroes);

    g_signal_connect(
        state->format, "notify::selected",
        G_CALLBACK(on_preferences_changed), state);
    g_signal_connect(
        state->decimals, "notify::value",
        G_CALLBACK(on_preferences_changed), state);
    g_signal_connect(
        state->grouping, "notify::active",
        G_CALLBACK(on_preferences_changed), state);
    g_signal_connect(
        state->zeroes, "notify::active",
        G_CALLBACK(on_preferences_changed), state);

    GtkWidget* note = gtk_label_new(
        "F-E remains a temporary Scientific override and resets on Clear.");
    gtk_widget_add_css_class(note, "status");
    gtk_label_set_wrap(GTK_LABEL(note), TRUE);
    gtk_label_set_xalign(GTK_LABEL(note), 0.0F);
    gtk_box_append(GTK_BOX(root), note);

    gtk_window_present(GTK_WINDOW(window));
}

void show_history(GtkWidget*, gpointer) {
    GtkWidget* window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), "Calculation History");
    gtk_window_set_default_size(GTK_WINDOW(window), 520, 460);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(root, "shell");
    gtk_window_set_child(GTK_WINDOW(window), root);

    GtkWidget* title = gtk_label_new("Calculation History");
    gtk_widget_add_css_class(title, "brand-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(root), title);

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(root), scroll);

    GtkWidget* list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(list, "history-list");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);

    const std::size_t count = controller.history_count();
    if (count == 0) {
        GtkWidget* empty = gtk_label_new("No calculations yet.");
        gtk_widget_add_css_class(empty, "history-row");
        gtk_widget_set_halign(empty, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(list), empty);
    } else {
        for (std::size_t index = 0; index < count; ++index) {
            const auto entry = controller.history_entry(index);
            if (!entry) continue;

            std::string label = entry->input;
            label += "\n= ";
            label += entry->output;

            GtkWidget* recall = gtk_button_new_with_label(label.c_str());
            gtk_widget_add_css_class(recall, "history-row");
            gtk_widget_set_halign(recall, GTK_ALIGN_FILL);
            gtk_widget_set_hexpand(recall, TRUE);
            gtk_widget_set_tooltip_text(
                recall, "Recall this calculation into Calculator");
            g_signal_connect(
                recall, "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer data) {
                    const auto encoded = GPOINTER_TO_UINT(data);
                    if (encoded == 0U) return;
                    const std::size_t index =
                        static_cast<std::size_t>(encoded - 1U);
                    if (!controller.recall_history(index)) return;

                    render_state();
                    if (main_window) {
                        const auto& metrics =
                            calculator::ui::kDesktopMetrics;
                        const int current_width =
                            gtk_widget_get_width(main_window);
                        if (current_width < metrics.wide_threshold) {
                            gtk_window_set_default_size(
                                GTK_WINDOW(main_window),
                                std::max(
                                    current_width,
                                    metrics.default_width),
                                calculator::ui::desktop_preferred_height(
                                    controller.state().mode));
                        }
                    }
                    gtk_widget_grab_focus(expression_entry);
                }),
                GUINT_TO_POINTER(static_cast<guint>(index + 1U)));
            gtk_box_append(GTK_BOX(list), recall);
        }
    }

    GtkWidget* clear = gtk_button_new_with_label("Clear History");
    gtk_widget_add_css_class(clear, "toolbar-button");
    g_signal_connect_swapped(
        clear, "clicked",
        G_CALLBACK(+[](GtkWindow* history_window) {
            controller.clear_history();
            refresh_history_dock();
            gtk_window_destroy(history_window);
        }),
        window);
    gtk_box_append(GTK_BOX(root), clear);

    gtk_window_present(GTK_WINDOW(window));
}

void show_additional_results(GtkWidget*, gpointer) {
    GtkWidget* window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), "Additional Results");
    gtk_window_set_default_size(GTK_WINDOW(window), 560, 280);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(root, "shell");
    gtk_window_set_child(GTK_WINDOW(window), root);

    GtkWidget* title = gtk_label_new("Additional Results");
    gtk_widget_add_css_class(title, "brand-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(root), title);

    const std::string text = controller.additional_results_text();
    GtkWidget* value = gtk_label_new(text.c_str());
    gtk_widget_add_css_class(value, "history-row");
    gtk_label_set_selectable(GTK_LABEL(value), TRUE);
    gtk_label_set_wrap(GTK_LABEL(value), TRUE);
    gtk_label_set_xalign(GTK_LABEL(value), 0.0F);
    gtk_widget_set_halign(value, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(value, TRUE);
    gtk_widget_set_vexpand(value, TRUE);
    gtk_widget_set_tooltip_text(
        value, "Select any representation to copy it");
    gtk_box_append(GTK_BOX(root), value);

    gtk_window_present(GTK_WINDOW(window));
}

void refresh_programmer_bits_window() {
    if (!programmer_bits_window || !programmer_bits_value) return;
    const std::string text = controller.programmer_representations_text();
    gtk_label_set_text(GTK_LABEL(programmer_bits_value), text.c_str());

    const auto bits = controller.programmer_bits();
    for (std::size_t bit = 0; bit < programmer_bit_buttons.size(); ++bit) {
        GtkWidget* button = programmer_bit_buttons[bit];
        const bool active = bit < bits.size() && bits[bit];
        gtk_button_set_label(GTK_BUTTON(button), active ? "1" : "0");
        gtk_widget_set_sensitive(button, bit < bits.size());
        apply_selected(button, active);
    }
}

void on_programmer_bit_clicked(GtkButton* button, gpointer) {
    const guint encoded = GPOINTER_TO_UINT(
        g_object_get_data(G_OBJECT(button), "calculator-bit-index"));
    if (encoded == 0U) return;
    const unsigned bit = static_cast<unsigned>(encoded - 1U);
    if (!controller.toggle_programmer_bit(bit)) return;
    render_state();
    refresh_programmer_bits_window();
}

void show_programmer_bases(GtkWidget*, gpointer) {
    if (programmer_bits_window) {
        refresh_programmer_bits_window();
        gtk_window_present(GTK_WINDOW(programmer_bits_window));
        return;
    }

    GtkApplication* app =
        main_window
            ? gtk_window_get_application(GTK_WINDOW(main_window))
            : nullptr;
    GtkWidget* window =
        app ? gtk_application_window_new(app) : gtk_window_new();
    programmer_bits_window = window;
    g_object_add_weak_pointer(
        G_OBJECT(window),
        reinterpret_cast<gpointer*>(&programmer_bits_window));
    gtk_window_set_title(
        GTK_WINDOW(window), "Programmer Representations & Bits");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 430);
    gtk_window_set_hide_on_close(GTK_WINDOW(window), TRUE);
    if (main_window) {
        gtk_window_set_transient_for(
            GTK_WINDOW(window), GTK_WINDOW(main_window));
        gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
    }

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(root, "shell");
    gtk_window_set_child(GTK_WINDOW(window), root);

    GtkWidget* title =
        gtk_label_new("Programmer Representations & Bits");
    gtk_widget_add_css_class(title, "brand-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(root), title);

    programmer_bits_value = gtk_label_new("");
    gtk_widget_add_css_class(programmer_bits_value, "history-row");
    gtk_label_set_selectable(GTK_LABEL(programmer_bits_value), TRUE);
    gtk_label_set_wrap(GTK_LABEL(programmer_bits_value), TRUE);
    gtk_label_set_xalign(GTK_LABEL(programmer_bits_value), 0.0F);
    gtk_widget_set_halign(programmer_bits_value, GTK_ALIGN_FILL);
    gtk_box_append(GTK_BOX(root), programmer_bits_value);

    GtkWidget* hint = gtk_label_new(
        "Click a bit to toggle it. Bit 63 is upper-left; bit 0 is lower-right.");
    gtk_widget_add_css_class(hint, "status");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0F);
    gtk_box_append(GTK_BOX(root), hint);

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(root), scroll);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
    gtk_widget_set_margin_top(grid, 6);
    gtk_widget_set_margin_bottom(grid, 6);
    gtk_widget_set_margin_start(grid, 6);
    gtk_widget_set_margin_end(grid, 6);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), grid);

    programmer_bit_buttons.assign(64U, nullptr);
    for (unsigned display = 0; display < 64U; ++display) {
        const unsigned bit = 63U - display;
        GtkWidget* button = gtk_button_new_with_label("0");
        gtk_widget_add_css_class(button, "calc-button");
        gtk_widget_add_css_class(button, "utility");
        const std::string tooltip =
            "Toggle bit " + std::to_string(bit);
        gtk_widget_set_tooltip_text(button, tooltip.c_str());
        g_object_set_data(
            G_OBJECT(button), "calculator-bit-index",
            GUINT_TO_POINTER(bit + 1U));
        g_signal_connect(
            button, "clicked",
            G_CALLBACK(on_programmer_bit_clicked), nullptr);
        gtk_grid_attach(
            GTK_GRID(grid), button,
            static_cast<int>(display % 8U),
            static_cast<int>(display / 8U), 1, 1);
        programmer_bit_buttons[bit] = button;
    }

    refresh_programmer_bits_window();
    gtk_window_present(GTK_WINDOW(window));
}

struct ToolWindowState {
    GtkWidget* selector = nullptr;
    GtkWidget* prompt = nullptr;
    GtkWidget* generic_input_row = nullptr;
    GtkWidget* input = nullptr;
    GtkWidget* conversion_box = nullptr;
    GtkWidget* conversion_dimension = nullptr;
    GtkWidget* conversion_value = nullptr;
    GtkWidget* conversion_from = nullptr;
    GtkWidget* conversion_to = nullptr;
    GtkWidget* output = nullptr;
    GtkWidget* graph = nullptr;
    bool updating_conversion = false;
    calculator::tools::ToolResult result;
};

struct ConversionPreferences {
    std::string dimension = "length";
    std::string from = "km";
    std::string to = "mi";
};

std::string drop_down_text(GtkWidget* widget) {
    if (!widget) return {};
    gpointer item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(widget));
    if (!item || !GTK_IS_STRING_OBJECT(item)) return {};
    const char* value = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
    return value ? value : "";
}

void set_drop_down_values(GtkWidget* widget,
                          const std::vector<std::string>& values,
                          std::string_view preferred = {}) {
    std::vector<const char*> raw;
    raw.reserve(values.size() + 1U);
    for (const auto& value : values) raw.push_back(value.c_str());
    raw.push_back(nullptr);
    GtkStringList* list = gtk_string_list_new(raw.data());
    gtk_drop_down_set_model(GTK_DROP_DOWN(widget), G_LIST_MODEL(list));
    g_object_unref(list);
    guint selected = values.empty() ? GTK_INVALID_LIST_POSITION : 0U;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i] == preferred) {
            selected = static_cast<guint>(i);
            break;
        }
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(widget), selected);
}

std::vector<std::string> converter_dimensions() {
    std::vector<std::string> dimensions;
    for (const auto& unit : calculator::tools::conversion_units()) {
        if (std::find(dimensions.begin(), dimensions.end(), unit.dimension) ==
            dimensions.end()) {
            dimensions.emplace_back(unit.dimension);
        }
    }
    return dimensions;
}

ConversionPreferences load_conversion_preferences() {
    ConversionPreferences prefs;
    gchar* path = g_build_filename(
        g_get_user_config_dir(), "infiltrator-calc", "conversion.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    if (g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, nullptr)) {
        auto load = [&](const char* name, std::string& target) {
            if (!g_key_file_has_key(key, "Conversion", name, nullptr)) return;
            gchar* value = g_key_file_get_string(key, "Conversion", name, nullptr);
            if (value && *value) target = value;
            g_free(value);
        };
        load("dimension", prefs.dimension);
        load("from", prefs.from);
        load("to", prefs.to);
    }
    g_key_file_unref(key);
    g_free(path);
    return prefs;
}

void save_conversion_preferences(ToolWindowState* state) {
    if (!state || state->updating_conversion) return;
    const std::string dimension = drop_down_text(state->conversion_dimension);
    const std::string from = drop_down_text(state->conversion_from);
    const std::string to = drop_down_text(state->conversion_to);
    if (dimension.empty() || from.empty() || to.empty()) return;

    gchar* directory = g_build_filename(
        g_get_user_config_dir(), "infiltrator-calc", nullptr);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_free(directory);
        return;
    }
    gchar* path = g_build_filename(directory, "conversion.ini", nullptr);
    GKeyFile* key = g_key_file_new();
    g_key_file_set_string(key, "Conversion", "dimension", dimension.c_str());
    g_key_file_set_string(key, "Conversion", "from", from.c_str());
    g_key_file_set_string(key, "Conversion", "to", to.c_str());
    gsize length = 0;
    gchar* data = g_key_file_to_data(key, &length, nullptr);
    if (data) {
        (void)g_file_set_contents(path, data, static_cast<gssize>(length), nullptr);
        g_free(data);
    }
    g_key_file_unref(key);
    g_free(path);
    g_free(directory);
}

void populate_conversion_units(ToolWindowState* state,
                               std::string_view preferred_from = {},
                               std::string_view preferred_to = {}) {
    if (!state) return;
    state->updating_conversion = true;
    const std::string dimension = drop_down_text(state->conversion_dimension);
    std::vector<std::string> units;
    for (const auto& unit : calculator::tools::conversion_units()) {
        if (unit.dimension == dimension) units.emplace_back(unit.name);
    }
    set_drop_down_values(state->conversion_from, units, preferred_from);
    std::string_view target = preferred_to;
    if (target.empty() && units.size() > 1U) target = units[1];
    set_drop_down_values(state->conversion_to, units, target);
    state->updating_conversion = false;
}

void present_tool_result(ToolWindowState* state) {
    if (!state || !state->output) return;
    const std::string text = state->result.ok
        ? state->result.output
        : "Error: " + state->result.error;
    GtkTextBuffer* buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->output));
    gtk_text_buffer_set_text(buffer, text.c_str(), -1);
    gtk_widget_set_visible(state->graph, !state->result.points.empty());
    gtk_widget_queue_draw(state->graph);
}

void run_conversion(ToolWindowState* state) {
    if (!state || !state->conversion_value || !state->output) return;
    const char* raw = gtk_editable_get_text(GTK_EDITABLE(state->conversion_value));
    const std::string value = raw ? raw : "";
    const std::string from = drop_down_text(state->conversion_from);
    const std::string to = drop_down_text(state->conversion_to);
    if (value.empty() || from.empty() || to.empty()) return;
    state->result = calculator::tools::evaluate(
        calculator::tools::AdvancedTool::UnitConversion,
        value + " " + from + " " + to);
    present_tool_result(state);
}

void update_tool_prompt(ToolWindowState* state) {
    if (!state || !state->selector) return;
    const guint selected =
        gtk_drop_down_get_selected(GTK_DROP_DOWN(state->selector));
    const auto& catalog = calculator::tools::catalog();
    const std::size_t index =
        std::min<std::size_t>(selected, catalog.size() - 1U);
    const auto& descriptor = catalog[index];
    const bool conversion =
        descriptor.tool == calculator::tools::AdvancedTool::UnitConversion;
    gtk_widget_set_visible(state->generic_input_row, !conversion);
    gtk_widget_set_visible(state->conversion_box, conversion);

    if (conversion) {
        gtk_label_set_text(
            GTK_LABEL(state->prompt),
            "Choose a dimension, source unit and target unit. "
            "The selected pair is remembered between Calculator sessions.");
        run_conversion(state);
        return;
    }

    std::string prompt(descriptor.prompt);
    prompt += "\nExample: ";
    prompt += descriptor.example;
    gtk_label_set_text(GTK_LABEL(state->prompt), prompt.c_str());
    gtk_editable_set_text(
        GTK_EDITABLE(state->input),
        std::string(descriptor.example).c_str());
}

void on_tool_selected(GObject*, GParamSpec*, gpointer data) {
    update_tool_prompt(static_cast<ToolWindowState*>(data));
}

void draw_tool_graph(GtkDrawingArea*, cairo_t* cr, int width, int height,
                     gpointer data) {
    auto* state = static_cast<ToolWindowState*>(data);
    if (!state || state->result.points.empty() || width <= 2 || height <= 2) {
        return;
    }

    const auto& palette = active_palette();
    auto set_colour = [cr](std::uint32_t rgb) {
        cairo_set_source_rgb(
            cr,
            static_cast<double>((rgb >> 16U) & 0xffU) / 255.0,
            static_cast<double>((rgb >> 8U) & 0xffU) / 255.0,
            static_cast<double>(rgb & 0xffU) / 255.0);
    };

    set_colour(palette.panel_rgb);
    cairo_paint(cr);

    bool have_point = false;
    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
    for (const auto& point : state->result.points) {
        if (!point.valid) continue;
        if (!have_point) {
            xmin = xmax = point.x;
            ymin = ymax = point.y;
            have_point = true;
        } else {
            xmin = std::min(xmin, point.x);
            xmax = std::max(xmax, point.x);
            ymin = std::min(ymin, point.y);
            ymax = std::max(ymax, point.y);
        }
    }
    if (!have_point) return;
    if (xmin == xmax) { xmin -= 1.0; xmax += 1.0; }
    if (ymin == ymax) { ymin -= 1.0; ymax += 1.0; }

    constexpr double margin = 12.0;
    const double plot_width = std::max(1.0, static_cast<double>(width) - 2.0 * margin);
    const double plot_height = std::max(1.0, static_cast<double>(height) - 2.0 * margin);
    auto px = [&](double x) {
        return margin + (x - xmin) / (xmax - xmin) * plot_width;
    };
    auto py = [&](double y) {
        return margin + (ymax - y) / (ymax - ymin) * plot_height;
    };

    set_colour(palette.status_border_rgb);
    cairo_set_line_width(cr, 1.0);
    if (xmin <= 0.0 && xmax >= 0.0) {
        cairo_move_to(cr, px(0.0), margin);
        cairo_line_to(cr, px(0.0), margin + plot_height);
        cairo_stroke(cr);
    }
    if (ymin <= 0.0 && ymax >= 0.0) {
        cairo_move_to(cr, margin, py(0.0));
        cairo_line_to(cr, margin + plot_width, py(0.0));
        cairo_stroke(cr);
    }

    set_colour(palette.accent_foreground_rgb);
    cairo_set_line_width(cr, 2.0);
    bool drawing = false;
    for (const auto& point : state->result.points) {
        if (!point.valid) {
            if (drawing) {
                cairo_stroke(cr);
                drawing = false;
            }
            continue;
        }
        if (!drawing) {
            cairo_move_to(cr, px(point.x), py(point.y));
            drawing = true;
        } else {
            cairo_line_to(cr, px(point.x), py(point.y));
        }
    }
    if (drawing) cairo_stroke(cr);
}

void on_tool_run(GtkButton*, gpointer data) {
    auto* state = static_cast<ToolWindowState*>(data);
    if (!state) return;
    const guint selected =
        gtk_drop_down_get_selected(GTK_DROP_DOWN(state->selector));
    const auto& catalog = calculator::tools::catalog();
    const std::size_t index =
        std::min<std::size_t>(selected, catalog.size() - 1U);
    const char* input =
        gtk_editable_get_text(GTK_EDITABLE(state->input));
    state->result = calculator::tools::evaluate(
        catalog[index].tool, input ? input : "");

    present_tool_result(state);
}

void show_advanced_tools(GtkWidget*, gpointer) {
    if (tools_window) {
        gtk_window_present(GTK_WINDOW(tools_window));
        return;
    }

    GtkApplication* app =
        main_window
            ? gtk_window_get_application(GTK_WINDOW(main_window))
            : nullptr;
    GtkWidget* window =
        app ? gtk_application_window_new(app) : gtk_window_new();
    tools_window = window;
    g_object_add_weak_pointer(
        G_OBJECT(window),
        reinterpret_cast<gpointer*>(&tools_window));

    gtk_window_set_title(GTK_WINDOW(window), "Calculator Tools");
    gtk_window_set_default_size(GTK_WINDOW(window), 720, 620);
    gtk_window_set_hide_on_close(GTK_WINDOW(window), TRUE);
    if (main_window) {
        gtk_window_set_transient_for(
            GTK_WINDOW(window), GTK_WINDOW(main_window));
        gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
    }

    auto* state = new ToolWindowState();
    g_object_set_data_full(
        G_OBJECT(window), "calculator-tool-state", state,
        +[](gpointer data) {
            delete static_cast<ToolWindowState*>(data);
        });

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(root, "shell");
    gtk_window_set_child(GTK_WINDOW(window), root);

    GtkWidget* title = gtk_label_new("Advanced Calculator Tools");
    gtk_widget_add_css_class(title, "brand-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(root), title);

    const auto& catalog = calculator::tools::catalog();
    std::vector<const char*> names;
    names.reserve(catalog.size() + 1U);
    for (const auto& item : catalog) names.push_back(item.name.data());
    names.push_back(nullptr);

    GtkStringList* list = gtk_string_list_new(names.data());
    state->selector = gtk_drop_down_new(G_LIST_MODEL(list), nullptr);
    g_object_unref(list);
    gtk_widget_set_hexpand(state->selector, TRUE);
    gtk_box_append(GTK_BOX(root), state->selector);

    state->prompt = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(state->prompt), TRUE);
    gtk_label_set_xalign(GTK_LABEL(state->prompt), 0.0F);
    gtk_widget_add_css_class(state->prompt, "history-row");
    gtk_box_append(GTK_BOX(root), state->prompt);

    state->generic_input_row =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(root), state->generic_input_row);
    state->input = gtk_entry_new();
    gtk_widget_set_hexpand(state->input, TRUE);
    gtk_box_append(GTK_BOX(state->generic_input_row), state->input);

    GtkWidget* run = gtk_button_new_with_label("Run");
    gtk_widget_add_css_class(run, "toolbar-button");
    gtk_box_append(GTK_BOX(state->generic_input_row), run);

    state->conversion_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(root), state->conversion_box);
    GtkWidget* conversion_top =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(state->conversion_box), conversion_top);

    state->conversion_dimension = gtk_drop_down_new(nullptr, nullptr);
    gtk_widget_set_hexpand(state->conversion_dimension, TRUE);
    gtk_box_append(GTK_BOX(conversion_top), state->conversion_dimension);

    state->conversion_value = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->conversion_value), "Value");
    gtk_editable_set_text(GTK_EDITABLE(state->conversion_value), "100");
    gtk_widget_set_hexpand(state->conversion_value, TRUE);
    gtk_box_append(GTK_BOX(conversion_top), state->conversion_value);

    GtkWidget* conversion_pair =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(state->conversion_box), conversion_pair);
    state->conversion_from = gtk_drop_down_new(nullptr, nullptr);
    state->conversion_to = gtk_drop_down_new(nullptr, nullptr);
    gtk_widget_set_hexpand(state->conversion_from, TRUE);
    gtk_widget_set_hexpand(state->conversion_to, TRUE);
    gtk_box_append(GTK_BOX(conversion_pair), state->conversion_from);

    GtkWidget* swap = gtk_button_new_with_label("⇄");
    gtk_widget_add_css_class(swap, "toolbar-button");
    gtk_widget_set_tooltip_text(swap, "Swap source and target units");
    gtk_box_append(GTK_BOX(conversion_pair), swap);
    gtk_box_append(GTK_BOX(conversion_pair), state->conversion_to);

    const ConversionPreferences converter_prefs =
        load_conversion_preferences();
    state->updating_conversion = true;
    set_drop_down_values(
        state->conversion_dimension, converter_dimensions(),
        converter_prefs.dimension);
    state->updating_conversion = false;
    populate_conversion_units(
        state, converter_prefs.from, converter_prefs.to);

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(root), scroll);
    state->output = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(state->output), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(state->output), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->output), GTK_WRAP_WORD_CHAR);
    gtk_widget_add_css_class(state->output, "history-row");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), state->output);

    state->graph = gtk_drawing_area_new();
    gtk_widget_set_size_request(state->graph, -1, 220);
    gtk_widget_set_visible(state->graph, FALSE);
    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(state->graph), draw_tool_graph, state, nullptr);
    gtk_box_append(GTK_BOX(root), state->graph);

    g_signal_connect(
        state->selector, "notify::selected",
        G_CALLBACK(on_tool_selected), state);
    g_signal_connect(
        run, "clicked", G_CALLBACK(on_tool_run), state);
    g_signal_connect(
        state->input, "activate",
        G_CALLBACK(+[](GtkEntry*, gpointer data) {
            on_tool_run(nullptr, data);
        }), state);
    g_signal_connect(
        state->conversion_dimension, "notify::selected",
        G_CALLBACK(+[](GObject*, GParamSpec*, gpointer data) {
            auto* s = static_cast<ToolWindowState*>(data);
            if (!s || s->updating_conversion) return;
            populate_conversion_units(s);
            save_conversion_preferences(s);
            run_conversion(s);
        }), state);
    auto pair_changed = +[](GObject*, GParamSpec*, gpointer data) {
        auto* s = static_cast<ToolWindowState*>(data);
        if (!s || s->updating_conversion) return;
        save_conversion_preferences(s);
        run_conversion(s);
    };
    g_signal_connect(
        state->conversion_from, "notify::selected",
        G_CALLBACK(pair_changed), state);
    g_signal_connect(
        state->conversion_to, "notify::selected",
        G_CALLBACK(pair_changed), state);
    g_signal_connect(
        state->conversion_value, "changed",
        G_CALLBACK(+[](GtkEditable*, gpointer data) {
            run_conversion(static_cast<ToolWindowState*>(data));
        }), state);
    g_signal_connect(
        swap, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer data) {
            auto* s = static_cast<ToolWindowState*>(data);
            if (!s) return;
            const guint from = gtk_drop_down_get_selected(
                GTK_DROP_DOWN(s->conversion_from));
            const guint to = gtk_drop_down_get_selected(
                GTK_DROP_DOWN(s->conversion_to));
            s->updating_conversion = true;
            gtk_drop_down_set_selected(GTK_DROP_DOWN(s->conversion_from), to);
            gtk_drop_down_set_selected(GTK_DROP_DOWN(s->conversion_to), from);
            s->updating_conversion = false;
            save_conversion_preferences(s);
            run_conversion(s);
        }), state);

    update_tool_prompt(state);
    gtk_window_present(GTK_WINDOW(window));
}

void on_activate(GtkEntry*) {
    history_navigation_index = -1;
    sync_expression_from_widget();
    const int position =
        gtk_editable_get_position(GTK_EDITABLE(expression_entry));
    const auto result = controller.dispatch(
        Command::Equals,
        position < 0 ? Controller::kEnd : static_cast<std::size_t>(position));
    render_state(result.cursor);
    save_user_variables();
    save_user_functions();
}

void on_button_clicked(GtkButton*, gpointer data) {
    const auto* spec = static_cast<const ButtonSpec*>(data);
    if (!spec) return;

    history_navigation_index = -1;
    sync_expression_from_widget();
    const int position =
        gtk_editable_get_position(GTK_EDITABLE(expression_entry));
    const auto result = controller.dispatch(
        spec->command,
        position < 0 ? Controller::kEnd : static_cast<std::size_t>(position));
    render_state(result.cursor);
    if (spec->command == Command::Equals) save_user_variables();
    save_user_functions();
    gtk_widget_grab_focus(expression_entry);
}

void on_mode_clicked(GtkButton*, gpointer data) {
    sync_expression_from_widget();
    const int encoded = GPOINTER_TO_INT(data);
    const Mode mode = static_cast<Mode>(encoded - 1);
    controller.set_mode(mode);
    render_state();
    if (main_window) save_desktop_state(main_window);

    // Standard mode has four fewer keypad rows than the extended modes.
    // Keep it compact instead of carrying the Scientific/Programmer height.
    if (main_window) {
        const auto& metrics = calculator::ui::kDesktopMetrics;
        const int current_width = gtk_widget_get_width(main_window);
        if (current_width < metrics.wide_threshold) {
            gtk_window_set_default_size(
                GTK_WINDOW(main_window),
                std::max(current_width, metrics.default_width),
                calculator::ui::desktop_preferred_height(mode));
        }
    }

    gtk_widget_grab_focus(expression_entry);
}

const char* button_class(ButtonRole role) {
    switch (role) {
    case ButtonRole::Number: return "number";
    case ButtonRole::Operation: return "operation";
    case ButtonRole::Utility: return "utility";
    case ButtonRole::Clear: return "clear";
    case ButtonRole::Equals: return "equals";
    }
    return "operation";
}

void remember_programmer_button(Command command, GtkWidget* button) {
    switch (command) {
    case Command::BaseBin: programmer_base_buttons[0] = button; break;
    case Command::BaseOct: programmer_base_buttons[1] = button; break;
    case Command::BaseDec: programmer_base_buttons[2] = button; break;
    case Command::BaseHex: programmer_base_buttons[3] = button; break;
    case Command::Width8: programmer_width_buttons[0] = button; break;
    case Command::Width16: programmer_width_buttons[1] = button; break;
    case Command::Width32: programmer_width_buttons[2] = button; break;
    case Command::Width64: programmer_width_buttons[3] = button; break;
    case Command::ToggleSigned: programmer_signed_button = button; break;
    case Command::CycleAngleUnit: angle_button = button; break;
    case Command::ToggleSecond: second_button = button; break;
    case Command::ToggleHyperbolic: hyperbolic_button = button; break;
    case Command::ToggleScientificNotation: notation_button = button; break;
    default: break;
    }
}

GtkWidget* calc_button(const ButtonSpec& spec) {
    const std::string label(spec.label);
    GtkWidget* button = gtk_button_new_with_label(label.c_str());
    gtk_widget_add_css_class(button, "calc-button");
    gtk_widget_add_css_class(button, button_class(spec.role));

    if (spec.command == Command::MemoryClear ||
        spec.command == Command::MemoryRecall ||
        spec.command == Command::MemoryStore ||
        spec.command == Command::MemoryAdd ||
        spec.command == Command::MemorySubtract) {
        gtk_widget_add_css_class(button, "memory-button");
    }

    g_signal_connect(
        button, "clicked", G_CALLBACK(on_button_clicked),
        const_cast<ButtonSpec*>(&spec));
    gtk_widget_set_hexpand(button, TRUE);
    gtk_widget_set_vexpand(button, FALSE);
    remember_programmer_button(spec.command, button);
    command_buttons.emplace_back(button, &spec);
    return button;
}

GtkWidget* toolbar_button(const char* text) {
    GtkWidget* button = gtk_button_new_with_label(text);
    gtk_widget_add_css_class(button, "toolbar-button");
    return button;
}

GtkWidget* mode_button(Mode target) {
    const std::string label(calculator::ui::mode_name(target));
    GtkWidget* button = gtk_button_new_with_label(label.c_str());
    gtk_widget_add_css_class(button, "mode-tab");
    gtk_widget_set_hexpand(button, TRUE);
    g_signal_connect(
        button, "clicked", G_CALLBACK(on_mode_clicked),
        GINT_TO_POINTER(static_cast<int>(target) + 1));
    mode_buttons[static_cast<int>(target)] = button;
    return button;
}

GtkWidget* new_grid() {
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(
        GTK_GRID(grid), calculator::ui::kDesktopMetrics.grid_gap_y);
    gtk_grid_set_column_spacing(
        GTK_GRID(grid), calculator::ui::kDesktopMetrics.grid_gap_x);
    gtk_widget_set_vexpand(grid, FALSE);
    return grid;
}

void fill_grid(GtkWidget* grid, const ButtonSpec* specs, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        const int row = static_cast<int>(index / 4U);
        const int column = static_cast<int>(index % 4U);
        gtk_grid_attach(
            GTK_GRID(grid), calc_button(specs[index]),
            column, row, 1, 1);
    }
}

void update_responsive_layout(GtkWidget* window) {
    if (!calculator_column || !history_dock || !history_button) return;

    const int width = gtk_widget_get_width(window);
    const int height = gtk_widget_get_height(window);
    if (width <= 0 || height <= 0) return;

    const auto layout =
        calculator::ui::responsive_layout(width, height);
    last_layout_class = layout.layout_class;
    responsive_layout_initialized = true;

    gtk_widget_set_visible(history_dock, layout.dock_history);
    gtk_widget_set_visible(history_button, !layout.dock_history);

    if (layout.compact_controls) {
        gtk_widget_add_css_class(calculator_column, "compact");
    } else {
        gtk_widget_remove_css_class(calculator_column, "compact");
    }
}

gboolean responsive_tick(
    GtkWidget* widget, GdkFrameClock*, gpointer) {
    const int width = gtk_widget_get_width(widget);
    const int height = gtk_widget_get_height(widget);
    if (width <= 0 || height <= 0) return G_SOURCE_CONTINUE;

    const auto layout =
        calculator::ui::responsive_layout(width, height);
    if (!responsive_layout_initialized ||
        layout.layout_class != last_layout_class) {
        update_responsive_layout(widget);
    }
    return G_SOURCE_CONTINUE;
}

void apply_css(GtkWidget* window) {
    const std::string ui = ui_font();
    const std::string brand = brand_font();
    const auto& metrics = calculator::ui::kDesktopMetrics;
    const auto& design = calculator::ui::design_metrics();
    const ThemePalette& p = active_palette();

    const std::string background = hex_colour(p.background_rgb);
    const std::string panel = hex_colour(p.panel_rgb);
    const std::string card = hex_colour(p.card_rgb);
    const std::string surface = hex_colour(p.surface_rgb);
    const std::string input = hex_colour(p.input_rgb);
    const std::string border = hex_colour(p.border_rgb);
    const std::string text = hex_colour(p.text_rgb);
    const std::string title = hex_colour(p.title_rgb);
    const std::string muted = hex_colour(p.muted_rgb);
    const std::string subtle = hex_colour(p.subtle_rgb);
    const std::string primary = hex_colour(p.button_background_rgb);
    const std::string primary_text = hex_colour(p.button_foreground_rgb);
    const std::string selected = hex_colour(p.selection_background_rgb);
    const std::string neutral = hex_colour(p.neutral_accent_rgb);
    const std::string warning = hex_colour(p.warning_rgb);
    const std::string fault = hex_colour(p.fault_rgb);
    const std::string operation = hex_colour(p.operation_rgb);
    const std::string card_hover = hex_colour(p.card_hover_rgb);
    const std::string surface_hover = hex_colour(p.surface_hover_rgb);
    const std::string operation_hover = hex_colour(p.operation_hover_rgb);
    const std::string equals_hover = hex_colour(p.equals_hover_rgb);
    const std::string heading = hex_colour(p.heading_rgb);
    const std::string summary = hex_colour(p.summary_rgb);
    const std::string kicker = hex_colour(p.kicker_rgb);
    const std::string detail_label = hex_colour(p.detail_label_rgb);
    const std::string note = hex_colour(p.note_rgb);
    const std::string status_border = hex_colour(p.status_border_rgb);
    const std::string accent_hover = hex_colour(p.accent_hover_rgb);
    const std::string selected_summary = hex_colour(p.selected_summary_rgb);
    const std::string warning_muted = hex_colour(p.warning_muted_rgb);
    const std::string warning_border = hex_colour(p.warning_border_rgb);

    const std::string css =
        "*{font-family:\"" + ui + "\";font-weight:400}"
        "window,.shell{background:" + background + ";color:" + text + "}"
        ".shell{padding:" + std::to_string(metrics.shell_padding) + "px}"
        ".calculator-column{background:" + background + "}"
        ".header{margin-bottom:0}"
        ".brand-title{font-family:\"" + brand + "\";font-size:18px;font-weight:400;color:" + heading + "}"
        ".toolbar-button{background:" + surface + ";color:" + muted + ";border:1px solid " + border + ";"
            "border-radius:" + std::to_string(design.control_radius) + "px;min-height:28px;padding:0 " +
            std::to_string(design.control_spacing) + "px;font-size:12px;font-weight:700}"
        ".toolbar-button:hover{background:" + surface_hover + ";color:" + heading + ";border-color:" + accent_hover + "}"
        ".mode-strip{background:" + surface + ";border:1px solid " + border + ";border-radius:" + std::to_string(design.control_radius) + "px;padding:" +
            std::to_string(design.compact_spacing / 2U) + "px}"
        ".mode-tab{background:transparent;color:" + kicker + ";border:0;border-radius:" + std::to_string(design.small_radius) + "px;"
            "min-height:28px;font-size:11px;font-weight:700;padding:0 8px}"
        ".mode-tab:hover{background:" + surface_hover + ";color:" + text + "}"
        ".mode-tab.selected{background:" + primary + ";color:" + primary_text + "}"
        ".display{background:" + panel + ";border:1px solid " + status_border + ";border-radius:" +
            std::to_string(design.card_radius) + "px;padding:" +
            std::to_string(design.control_spacing) + "px}"
        ".expression{background:" + input + ";color:" + summary + ";border:0;border-radius:" + std::to_string(design.small_radius) + "px;"
            "padding:4px 8px;min-height:20px;font-size:13px;outline:none;box-shadow:none}"
        ".expression:focus{border:0;outline:none;box-shadow:none}"
        ".result{font-family:\"" + brand + "\";font-size:30px;font-weight:400;color:" + heading + ";padding-top:2px}"
        ".status{font-size:10px;font-weight:700;letter-spacing:.04em;color:" + note + "}"
        ".status.fault{color:" + fault + "}"
        ".calc-button{border:1px solid " + border + ";border-radius:" +
            std::to_string(design.control_radius) + "px;min-height:" +
            std::to_string(metrics.key_min_height) + "px;font-size:13px;font-weight:700;padding:0}"
        ".calc-button:disabled{opacity:.38}"
        ".calc-button.number{background:" + card + ";color:" + title + "}"
        ".calc-button.number:hover{background:" + card_hover + ";border-color:" + accent_hover + "}"
        ".calc-button.operation{background:" + operation + ";color:" + text + "}"
        ".calc-button.operation:hover{background:" + operation_hover + ";border-color:" + accent_hover + "}"
        ".calc-button.utility{background:" + surface + ";color:" + muted + ";font-size:12px}"
        ".calc-button.utility:hover{background:" + surface_hover + ";color:" + heading + ";border-color:" + accent_hover + "}"
        ".calc-button.utility.selected{background:" + selected + ";color:" + selected_summary + ";border-color:" + accent_hover + "}"
        ".calc-button.memory-button{background:transparent;color:" + muted + ";border-color:transparent;"
            "border-radius:" + std::to_string(design.small_radius) + "px;min-height:" + std::to_string(metrics.memory_height) + "px;font-size:11px}"
        ".calc-button.memory-button:hover{background:" + surface_hover + ";color:" + title + ";border-color:transparent}"
        ".calc-button.clear{background:" + card + ";color:" + warning_muted + ";border-color:" + warning_border + "}"
        ".calc-button.clear:hover{background:" + card_hover + ";color:" + warning + ";border-color:" + warning + "}"
        ".calc-button.equals{background:" + primary + ";color:" + primary_text + ";border-color:" + primary + ";font-size:15px;font-weight:700}"
        ".calc-button.equals:hover{background:" + equals_hover + ";border-color:" + equals_hover + "}"
        ".history-dock{background:" + panel + ";border:1px solid " + border + ";border-radius:" + std::to_string(design.card_radius) + "px;padding:" +
            std::to_string(design.control_spacing) + "px}"
        ".history-text{background:" + panel + ";color:" + detail_label + ";font-size:12px}"
        ".history-list{background:" + panel + ";border:1px solid " + border + ";border-radius:" + std::to_string(design.card_radius) + "px}"
        ".history-row{background:" + card + ";padding:" +
            std::to_string(design.control_spacing) +
            "px;border:1px solid " + border + ";border-radius:" +
            std::to_string(design.control_radius) +
            "px;color:" + text + ";font-size:12px;text-align:left}"
        ".history-row:hover{background:" + card_hover +
            ";border-color:" + accent_hover + "}"
        ".calc-button:focus,.toolbar-button:focus,.mode-tab:focus{outline:2px solid " +
            accent_hover + ";outline-offset:1px}"
        ".compact .brand-title{font-size:17px}"
        ".compact .display{padding:7px}"
        ".compact .calc-button{min-height:30px;font-size:13px}"
        ".compact .mode-tab{min-height:25px}";

    if (!css_provider) {
        css_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(
            gtk_widget_get_display(window),
            GTK_STYLE_PROVIDER(css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    gtk_css_provider_load_from_data(css_provider, css.c_str(), -1);

    if (theme_button) {
        const std::string label(
            calculator::ui::theme_mode_name(theme_mode));
        gtk_button_set_label(GTK_BUTTON(theme_button), label.c_str());
        gtk_widget_set_tooltip_text(
            theme_button,
            theme_mode == ThemeMode::System
                ? (effective_dark_theme
                    ? "Theme: follow system (Night)"
                    : "Theme: follow system (Day)")
                : (theme_mode == ThemeMode::Day
                    ? "Theme: Day"
                    : "Theme: Night"));
    }
}

void on_theme_clicked(GtkButton*, gpointer) {
    theme_mode = calculator::ui::next_theme_mode(theme_mode);
    save_theme_mode();
    if (main_window) apply_css(main_window);
}

void on_system_theme_changed(GObject*, GParamSpec*, gpointer) {
    if (theme_mode == ThemeMode::System && main_window) {
        apply_css(main_window);
    }
}

void insert_expression_shortcut(std::string_view text) {
    if (!expression_entry) return;
    GtkEditable* editable = GTK_EDITABLE(expression_entry);
    int position = gtk_editable_get_position(editable);
    if (position < 0) position = 0;
    gtk_editable_insert_text(
        editable, text.data(), static_cast<int>(text.size()), &position);
    gtk_editable_set_position(editable, position);
    sync_expression_from_widget();
    render_state(static_cast<std::size_t>(position));
}

gboolean on_window_key_pressed(GtkEventControllerKey*, guint keyval,
                               guint, GdkModifierType state, gpointer) {
    const bool control = (state & GDK_CONTROL_MASK) != 0;
    const bool alt = (state & GDK_ALT_MASK) != 0;

    if (keyval == GDK_KEY_Escape ||
        (control && keyval == GDK_KEY_Delete)) {
        history_navigation_index = -1;
        sync_expression_from_widget();
        const auto result = controller.dispatch(Command::Clear);
        render_state(result.cursor);
        gtk_widget_grab_focus(expression_entry);
        return TRUE;
    }

    if (alt && (keyval == GDK_KEY_Left || keyval == GDK_KEY_Right)) {
        const std::size_t count = controller.history_count();
        if (count == 0U) return TRUE;

        if (keyval == GDK_KEY_Left) {
            const int maximum = static_cast<int>(count - 1U);
            history_navigation_index =
                std::min(history_navigation_index + 1, maximum);
        } else if (history_navigation_index > 0) {
            --history_navigation_index;
        } else {
            return TRUE;
        }

        if (controller.recall_history(
                static_cast<std::size_t>(history_navigation_index))) {
            render_state();
            update_responsive_layout(main_window);
            gtk_widget_grab_focus(expression_entry);
        }
        return TRUE;
    }

    if (!control) return FALSE;

    if (controller.state().mode == Mode::Programmer) {
        Command base_command = Command::BaseDec;
        bool handled = true;
        switch (keyval) {
        case GDK_KEY_b:
        case GDK_KEY_B: base_command = Command::BaseBin; break;
        case GDK_KEY_o:
        case GDK_KEY_O: base_command = Command::BaseOct; break;
        case GDK_KEY_d:
        case GDK_KEY_D: base_command = Command::BaseDec; break;
        case GDK_KEY_h:
        case GDK_KEY_H: base_command = Command::BaseHex; break;
        default: handled = false; break;
        }
        if (handled) {
            history_navigation_index = -1;
            const auto result = controller.dispatch(base_command);
            render_state(result.cursor);
            gtk_widget_grab_focus(expression_entry);
            return TRUE;
        }
    }

    if (controller.state().mode == Mode::Scientific) {
        switch (keyval) {
        case GDK_KEY_p:
        case GDK_KEY_P:
            history_navigation_index = -1;
            insert_expression_shortcut("pi");
            return TRUE;
        case GDK_KEY_r:
        case GDK_KEY_R:
            history_navigation_index = -1;
            insert_expression_shortcut("sqrt(");
            return TRUE;
        case GDK_KEY_e:
        case GDK_KEY_E:
            history_navigation_index = -1;
            insert_expression_shortcut("e");
            return TRUE;
        default:
            break;
        }
    }

    return FALSE;
}

void activate(GtkApplication* app, gpointer) {
    const auto& metrics = calculator::ui::kDesktopMetrics;

    GtkWidget* window = gtk_application_window_new(app);
    main_window = window;
    theme_mode = load_theme_mode();
    load_user_variables();
    load_user_functions();
    load_display_preferences();
    const DesktopState desktop_state = load_desktop_state();
    controller.set_angle_unit(
        static_cast<calculator::AngleUnit>(desktop_state.angle_code));
    controller.set_programmer_context(
        static_cast<calculator::ProgrammerBase>(
            desktop_state.programmer_base_code),
        static_cast<calculator::IntegerWidth>(
            desktop_state.programmer_width_code),
        desktop_state.programmer_signed_flag);
    controller.set_mode(desktop_state.selected_mode);

    if (!font_family_available(ui_font()) ||
        !font_family_available(brand_font())) {
        g_printerr(
            "Calculator requires its packaged MB Corpo S and A font families; "
            "refusing silent font substitution.\n");
        gtk_window_destroy(GTK_WINDOW(window));
        main_window = nullptr;
        return;
    }

    gtk_window_set_title(GTK_WINDOW(window), "Calculator");
    gtk_window_set_icon_name(GTK_WINDOW(window), "infiltrator-calc");
    gtk_window_set_default_size(
        GTK_WINDOW(window),
        desktop_state.width,
        desktop_state.height);
    g_signal_connect(
        window, "close-request",
        G_CALLBACK(on_main_close_request), nullptr);

    GtkWidget* shell = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, metrics.section_gap);
    gtk_widget_add_css_class(shell, "shell");
    gtk_window_set_child(GTK_WINDOW(window), shell);
    apply_css(window);

    calculator_column =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, metrics.section_gap);
    gtk_widget_add_css_class(calculator_column, "calculator-column");
    gtk_widget_set_hexpand(calculator_column, TRUE);
    gtk_widget_set_vexpand(calculator_column, TRUE);
    gtk_box_append(GTK_BOX(shell), calculator_column);

    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(header, "header");
    gtk_box_append(GTK_BOX(calculator_column), header);

    GtkWidget* title = gtk_label_new("Calculator");
    gtk_widget_add_css_class(title, "brand-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_box_append(GTK_BOX(header), title);

    const std::string initial_theme_label(
        calculator::ui::theme_mode_name(theme_mode));
    theme_button = toolbar_button(initial_theme_label.c_str());
    g_signal_connect(
        theme_button, "clicked", G_CALLBACK(on_theme_clicked), nullptr);
    gtk_box_append(GTK_BOX(header), theme_button);

    preferences_button = toolbar_button("Prefs");
    gtk_widget_set_tooltip_text(
        preferences_button, "Result format, precision and grouping");
    g_signal_connect(
        preferences_button, "clicked",
        G_CALLBACK(show_preferences), nullptr);
    gtk_box_append(GTK_BOX(header), preferences_button);

    tools_button = toolbar_button("Tools");
    gtk_widget_set_tooltip_text(
        tools_button, "Open engineering, conversion, network, storage, date, statistics, graph, equation, exact, arbitrary-precision and complex tools");
    g_signal_connect(
        tools_button, "clicked", G_CALLBACK(show_advanced_tools), nullptr);
    gtk_box_append(GTK_BOX(header), tools_button);

    results_button = toolbar_button("Results");
    gtk_widget_set_tooltip_text(
        results_button, "Show decimal, scientific and engineering representations");
    g_signal_connect(
        results_button, "clicked",
        G_CALLBACK(show_additional_results), nullptr);
    gtk_box_append(GTK_BOX(header), results_button);

    bases_button = toolbar_button("Bases");
    gtk_widget_set_tooltip_text(
        bases_button, "Show BIN/OCT/DEC/HEX representations");
    g_signal_connect(
        bases_button, "clicked",
        G_CALLBACK(show_programmer_bases), nullptr);
    gtk_box_append(GTK_BOX(header), bases_button);

    history_button = toolbar_button("History");
    g_signal_connect(
        history_button, "clicked", G_CALLBACK(show_history), nullptr);
    gtk_box_append(GTK_BOX(header), history_button);

    GtkWidget* mode_strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_add_css_class(mode_strip, "mode-strip");
    gtk_box_append(GTK_BOX(calculator_column), mode_strip);
    gtk_box_append(GTK_BOX(mode_strip), mode_button(Mode::Standard));
    gtk_box_append(GTK_BOX(mode_strip), mode_button(Mode::Scientific));
    gtk_box_append(GTK_BOX(mode_strip), mode_button(Mode::Programmer));

    GtkWidget* display = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(display, "display");
    gtk_box_append(GTK_BOX(calculator_column), display);

    expression_entry = gtk_entry_new();
    gtk_editable_set_enable_undo(GTK_EDITABLE(expression_entry), TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(expression_entry), "Expression");
    gtk_widget_add_css_class(expression_entry, "expression");
    gtk_entry_set_alignment(GTK_ENTRY(expression_entry), 1);
    g_signal_connect(
        expression_entry, "activate", G_CALLBACK(on_activate), nullptr);
    gtk_box_append(GTK_BOX(display), expression_entry);

    result_label = gtk_label_new("0");
    gtk_widget_add_css_class(result_label, "result");
    gtk_widget_set_halign(result_label, GTK_ALIGN_END);
    gtk_label_set_selectable(GTK_LABEL(result_label), TRUE);
    gtk_widget_set_tooltip_text(
        result_label, "Calculation result; select to copy");
    gtk_label_set_ellipsize(GTK_LABEL(result_label), PANGO_ELLIPSIZE_START);
    gtk_box_append(GTK_BOX(display), result_label);

    status_label = gtk_label_new("READY");
    gtk_widget_add_css_class(status_label, "status");
    gtk_widget_set_halign(status_label, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(display), status_label);

    standard_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(calculator_column), standard_panel);

    GtkWidget* memory_strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(standard_panel), memory_strip);
    for (const ButtonSpec& spec : calculator::ui::kStandardMemory) {
        gtk_box_append(GTK_BOX(memory_strip), calc_button(spec));
    }

    GtkWidget* standard_grid = new_grid();
    gtk_box_append(GTK_BOX(standard_panel), standard_grid);
    fill_grid(
        standard_grid,
        calculator::ui::kStandardKeypad.data(),
        calculator::ui::kStandardKeypad.size());

    scientific_grid = new_grid();
    gtk_box_append(GTK_BOX(calculator_column), scientific_grid);
    fill_grid(
        scientific_grid,
        calculator::ui::kScientificKeypad.data(),
        calculator::ui::kScientificKeypad.size());

    programmer_grid = new_grid();
    gtk_box_append(GTK_BOX(calculator_column), programmer_grid);
    fill_grid(
        programmer_grid,
        calculator::ui::kProgrammerKeypad.data(),
        calculator::ui::kProgrammerKeypad.size());

    history_dock = gtk_scrolled_window_new();
    gtk_widget_add_css_class(history_dock, "history-dock");
    gtk_widget_set_visible(history_dock, FALSE);
    gtk_widget_set_size_request(history_dock, metrics.history_min_width, -1);
    gtk_widget_set_vexpand(history_dock, TRUE);
    gtk_box_append(GTK_BOX(shell), history_dock);

    history_text = gtk_text_view_new();
    gtk_widget_add_css_class(history_text, "history-text");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(history_text), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(history_text), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(history_text), GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(history_dock), history_text);

    render_state();

    GtkEventController* key_controller = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(
        key_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(
        key_controller, "key-pressed",
        G_CALLBACK(on_window_key_pressed), nullptr);
    gtk_widget_add_controller(window, key_controller);

    GtkSettings* settings = gtk_settings_get_default();
    if (settings) {
        g_signal_connect(
            settings, "notify::gtk-theme-name",
            G_CALLBACK(on_system_theme_changed), nullptr);
        g_signal_connect(
            settings, "notify::gtk-application-prefer-dark-theme",
            G_CALLBACK(on_system_theme_changed), nullptr);
    }

    gtk_window_present(GTK_WINDOW(window));
    update_responsive_layout(window);
    gtk_widget_add_tick_callback(window, responsive_tick, nullptr, nullptr);
    gtk_widget_grab_focus(expression_entry);
}

} // namespace

int main(int argc, char** argv) {
    GtkApplication* app =
        gtk_application_new(
            "net.ssmith.infiltrator.calc",
            G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);

    const int status_code =
        g_application_run(G_APPLICATION(app), argc, argv);

    save_user_variables();
    save_user_functions();
    if (css_provider) g_object_unref(css_provider);
    g_object_unref(app);
    return status_code;
}
