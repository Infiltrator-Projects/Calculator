/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../ui/calculator_ui_controller.hpp"
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
using calculator::ui::LayoutClass;
using calculator::ui::Mode;
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
GtkWidget* theme_button = nullptr;
GtkWidget* main_window = nullptr;
GtkWidget* history_dock = nullptr;
GtkWidget* history_text = nullptr;
GtkWidget* calculator_column = nullptr;
GtkCssProvider* css_provider = nullptr;

std::vector<std::pair<GtkWidget*, const ButtonSpec*>> command_buttons;
LayoutClass last_layout_class = LayoutClass::Regular;
bool responsive_layout_initialized = false;

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

    GtkWidget* history_view = gtk_text_view_new();
    gtk_widget_add_css_class(history_view, "history-text");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(history_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(history_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(history_view), GTK_WRAP_WORD_CHAR);
    GtkTextBuffer* history_buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(history_view));
    const std::string history = controller.history_text();
    gtk_text_buffer_set_text(history_buffer, history.c_str(), -1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), history_view);

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

void on_activate(GtkEntry*) {
    sync_expression_from_widget();
    const int position =
        gtk_editable_get_position(GTK_EDITABLE(expression_entry));
    const auto result = controller.dispatch(
        Command::Equals,
        position < 0 ? Controller::kEnd : static_cast<std::size_t>(position));
    render_state(result.cursor);
}

void on_button_clicked(GtkButton*, gpointer data) {
    const auto* spec = static_cast<const ButtonSpec*>(data);
    if (!spec) return;

    sync_expression_from_widget();
    const int position =
        gtk_editable_get_position(GTK_EDITABLE(expression_entry));
    const auto result = controller.dispatch(
        spec->command,
        position < 0 ? Controller::kEnd : static_cast<std::size_t>(position));
    render_state(result.cursor);
    gtk_widget_grab_focus(expression_entry);
}

void on_mode_clicked(GtkButton*, gpointer data) {
    sync_expression_from_widget();
    const int encoded = GPOINTER_TO_INT(data);
    const Mode mode = static_cast<Mode>(encoded - 1);
    controller.set_mode(mode);
    render_state();

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
    const std::string selection_text = hex_colour(p.selection_foreground_rgb);
    const std::string neutral = hex_colour(p.neutral_accent_rgb);
    const std::string warning = hex_colour(p.warning_rgb);
    const std::string fault = hex_colour(p.fault_rgb);
    const std::string operation = hex_colour(p.operation_rgb);
    const std::string card_hover = hex_colour(p.card_hover_rgb);
    const std::string surface_hover = hex_colour(p.surface_hover_rgb);
    const std::string operation_hover = hex_colour(p.operation_hover_rgb);
    const std::string equals_hover = hex_colour(p.equals_hover_rgb);

    const std::string css =
        "*{font-family:\"" + ui + "\";font-weight:400}"
        "window,.shell{background:" + background + ";color:" + text + "}"
        ".shell{padding:" + std::to_string(metrics.shell_padding) + "px}"
        ".calculator-column{background:" + background + "}"
        ".header{margin-bottom:0}"
        ".brand-title{font-family:\"" + brand + "\";font-size:18px;font-weight:400;color:" + title + "}"
        ".toolbar-button{background:" + surface + ";color:" + muted + ";border:1px solid " + border + ";"
            "border-radius:" + std::to_string(design.control_radius) + "px;min-height:28px;padding:0 " +
            std::to_string(design.control_spacing) + "px;font-size:12px;font-weight:700}"
        ".toolbar-button:hover{background:" + surface_hover + ";color:" + title + ";border-color:" + neutral + "}"
        ".mode-strip{background:" + surface + ";border:1px solid " + border + ";border-radius:" + std::to_string(design.control_radius) + "px;padding:" +
            std::to_string(design.compact_spacing / 2U) + "px}"
        ".mode-tab{background:transparent;color:" + subtle + ";border:0;border-radius:" + std::to_string(design.small_radius) + "px;"
            "min-height:28px;font-size:11px;font-weight:700;padding:0 8px}"
        ".mode-tab:hover{background:" + surface_hover + ";color:" + text + "}"
        ".mode-tab.selected{background:" + primary + ";color:" + primary_text + "}"
        ".display{background:" + panel + ";border:1px solid " + border + ";border-radius:" +
            std::to_string(design.card_radius) + "px;padding:" +
            std::to_string(design.control_spacing) + "px}"
        ".expression{background:" + input + ";color:" + muted + ";border:0;border-radius:" + std::to_string(design.small_radius) + "px;"
            "padding:4px 8px;min-height:20px;font-size:13px;outline:none;box-shadow:none}"
        ".expression:focus{border:0;outline:none;box-shadow:none}"
        ".result{font-family:\"" + brand + "\";font-size:30px;font-weight:400;color:" + title + ";padding-top:2px}"
        ".status{font-size:10px;font-weight:700;letter-spacing:.04em;color:" + subtle + "}"
        ".status.fault{color:" + fault + "}"
        ".calc-button{border:1px solid " + border + ";border-radius:" +
            std::to_string(design.control_radius) + "px;min-height:" +
            std::to_string(metrics.key_min_height) + "px;font-size:13px;font-weight:700;padding:0}"
        ".calc-button:disabled{opacity:.38}"
        ".calc-button.number{background:" + card + ";color:" + title + "}"
        ".calc-button.number:hover{background:" + card_hover + ";border-color:" + neutral + "}"
        ".calc-button.operation{background:" + operation + ";color:" + text + "}"
        ".calc-button.operation:hover{background:" + operation_hover + ";border-color:" + neutral + "}"
        ".calc-button.utility{background:" + surface + ";color:" + muted + ";font-size:12px}"
        ".calc-button.utility:hover{background:" + surface_hover + ";color:" + title + ";border-color:" + neutral + "}"
        ".calc-button.utility.selected{background:" + selected + ";color:" + selection_text + ";border-color:" + neutral + "}"
        ".calc-button.memory-button{background:transparent;color:" + muted + ";border-color:transparent;"
            "border-radius:" + std::to_string(design.small_radius) + "px;min-height:" + std::to_string(metrics.memory_height) + "px;font-size:11px}"
        ".calc-button.memory-button:hover{background:" + surface_hover + ";color:" + title + ";border-color:transparent}"
        ".calc-button.clear{background:" + card + ";color:" + warning + "}"
        ".calc-button.clear:hover{background:" + card_hover + ";border-color:" + warning + "}"
        ".calc-button.equals{background:" + primary + ";color:" + primary_text + ";border-color:" + primary + ";font-size:15px;font-weight:700}"
        ".calc-button.equals:hover{background:" + equals_hover + ";border-color:" + equals_hover + "}"
        ".history-dock{background:" + panel + ";border:1px solid " + border + ";border-radius:" + std::to_string(design.card_radius) + "px;padding:" +
            std::to_string(design.control_spacing) + "px}"
        ".history-text{background:" + panel + ";color:" + text + ";font-size:12px}"
        ".history-list{background:" + panel + ";border:1px solid " + border + ";border-radius:" + std::to_string(design.card_radius) + "px}"
        ".history-row{padding:" + std::to_string(design.control_spacing) +
            "px;border-bottom:1px solid " + border + ";color:" + text + ";font-size:12px}"
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

void activate(GtkApplication* app, gpointer) {
    const auto& metrics = calculator::ui::kDesktopMetrics;

    GtkWidget* window = gtk_application_window_new(app);
    main_window = window;
    theme_mode = load_theme_mode();

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
    gtk_window_set_default_size(
        GTK_WINDOW(window),
        metrics.default_width,
        calculator::ui::desktop_preferred_height(Mode::Standard));

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
    gtk_entry_set_placeholder_text(GTK_ENTRY(expression_entry), "Expression");
    gtk_widget_add_css_class(expression_entry, "expression");
    gtk_entry_set_alignment(GTK_ENTRY(expression_entry), 1);
    g_signal_connect(
        expression_entry, "activate", G_CALLBACK(on_activate), nullptr);
    gtk_box_append(GTK_BOX(display), expression_entry);

    result_label = gtk_label_new("0");
    gtk_widget_add_css_class(result_label, "result");
    gtk_widget_set_halign(result_label, GTK_ALIGN_END);
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

    if (css_provider) g_object_unref(css_provider);
    g_object_unref(app);
    return status_code;
}
