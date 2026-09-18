/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../ui/calculator_ui_controller.hpp"

#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#include <iomanip>
#include <sstream>
#include <string>

namespace {

using infiltrator::calc::ui::ButtonRole;
using infiltrator::calc::ui::ButtonSpec;
using infiltrator::calc::ui::Command;
using infiltrator::calc::ui::Controller;
using infiltrator::calc::ui::Mode;

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
GtkWidget* degrees_button = nullptr;
GtkCssProvider* css_provider = nullptr;

Controller controller;

constexpr const char* kUiFont = "MB Corpo S Title WEB";
constexpr const char* kBrandFont = "MB Corpo A Title Cond WEB";

std::string format_value(double value) {
    std::ostringstream out;
    out << std::setprecision(15) << value;
    return out.str();
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
    return font_family_available(kUiFont) ? kUiFont : "Sans";
}

const char* brand_font() {
    return font_family_available(kBrandFont) ? kBrandFont : ui_font();
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
        state.programmer_base == infiltrator::calc::ProgrammerBase::Binary);
    apply_selected(
        programmer_base_buttons[1],
        state.programmer_base == infiltrator::calc::ProgrammerBase::Octal);
    apply_selected(
        programmer_base_buttons[2],
        state.programmer_base == infiltrator::calc::ProgrammerBase::Decimal);
    apply_selected(
        programmer_base_buttons[3],
        state.programmer_base == infiltrator::calc::ProgrammerBase::Hexadecimal);

    apply_selected(
        programmer_width_buttons[0],
        state.programmer_width == infiltrator::calc::IntegerWidth::Bits8);
    apply_selected(
        programmer_width_buttons[1],
        state.programmer_width == infiltrator::calc::IntegerWidth::Bits16);
    apply_selected(
        programmer_width_buttons[2],
        state.programmer_width == infiltrator::calc::IntegerWidth::Bits32);
    apply_selected(
        programmer_width_buttons[3],
        state.programmer_width == infiltrator::calc::IntegerWidth::Bits64);
    apply_selected(programmer_signed_button, state.programmer_signed);

    if (degrees_button) {
        gtk_button_set_label(
            GTK_BUTTON(degrees_button),
            state.degrees ? "DEG" : "RAD");
    }
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

    GtkWidget* list = gtk_list_box_new();
    gtk_widget_add_css_class(list, "history-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);

    for (auto it = controller.session().history().rbegin();
         it != controller.session().history().rend(); ++it) {
        const std::string text =
            it->input + "\n" +
            (it->result.ok
                 ? format_value(it->result.value)
                 : ("Error: " + it->result.error));

        GtkWidget* row = gtk_label_new(text.c_str());
        gtk_label_set_xalign(GTK_LABEL(row), 0.0F);
        gtk_widget_add_css_class(row, "history-row");
        gtk_list_box_append(GTK_LIST_BOX(list), row);
    }

    GtkWidget* clear = gtk_button_new_with_label("Clear History");
    gtk_widget_add_css_class(clear, "toolbar-button");
    g_signal_connect_swapped(
        clear, "clicked",
        G_CALLBACK(+[](GtkWindow* history_window) {
            controller.clear_history();
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
    controller.set_mode(static_cast<Mode>(encoded - 1));
    render_state();
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
    case Command::ToggleDegrees: degrees_button = button; break;
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
    return button;
}

GtkWidget* toolbar_button(const char* text) {
    GtkWidget* button = gtk_button_new_with_label(text);
    gtk_widget_add_css_class(button, "toolbar-button");
    g_signal_connect(button, "clicked", G_CALLBACK(show_history), nullptr);
    return button;
}

GtkWidget* mode_button(Mode target) {
    const std::string label(infiltrator::calc::ui::mode_name(target));
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
        GTK_GRID(grid), infiltrator::calc::ui::kDesktopMetrics.grid_gap_y);
    gtk_grid_set_column_spacing(
        GTK_GRID(grid), infiltrator::calc::ui::kDesktopMetrics.grid_gap_x);
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

void apply_css(GtkWidget* window) {
    const std::string ui = ui_font();
    const std::string brand = brand_font();
    const auto& metrics = infiltrator::calc::ui::kDesktopMetrics;

    const std::string css =
        "*{font-family:\"" + ui + "\";font-weight:400}"
        "window,.shell{background:#050608;color:#E8ECEF}"
        ".shell{padding:" + std::to_string(metrics.shell_padding) + "px}"
        ".header{margin-bottom:0}"
        ".brand-title{font-family:\"" + brand + "\";font-size:20px;color:#EEF1F3}"
        ".brand-subtitle{font-size:9px;font-weight:700;letter-spacing:.10em;color:#899198}"
        ".toolbar-button{background:#0D1014;color:#AEB6BD;border:1px solid #353A40;"
            "border-radius:8px;min-height:28px;padding:0 10px;font-weight:700}"
        ".toolbar-button:hover{background:#171B20;color:#EEF1F3;border-color:#6A737C}"
        ".mode-strip{background:#0D1014;border:1px solid #353A40;border-radius:9px;padding:3px}"
        ".mode-tab{background:transparent;color:#899198;border:0;border-radius:7px;"
            "min-height:28px;font-size:10px;font-weight:700;padding:0 8px}"
        ".mode-tab:hover{background:#171B20;color:#D7DDE2}"
        ".mode-tab.selected{background:#D7DDE2;color:#111418}"
        ".display{background:#101318;border:1px solid #353A40;border-radius:9px;padding:10px}"
        ".expression{background:#0E1115;color:#AEB6BD;border:0;border-radius:7px;"
            "padding:4px 8px;min-height:20px;font-size:12px;outline:none;box-shadow:none}"
        ".expression:focus{border:0;outline:none;box-shadow:none}"
        ".result{font-family:\"" + brand + "\";font-size:34px;color:#EEF1F3;padding-top:2px}"
        ".status{font-size:9px;font-weight:700;letter-spacing:.08em;color:#899198}"
        ".status.fault{color:#C96B6B}"
        ".calc-button{border:1px solid #353A40;border-radius:8px;min-height:" +
            std::to_string(metrics.key_min_height) + "px;font-size:13px;font-weight:700;padding:0}"
        ".calc-button.number{background:#171B20;color:#EEF1F3}"
        ".calc-button.number:hover{background:#22272D;border-color:#6A737C}"
        ".calc-button.operation{background:#20252B;color:#D7DDE2}"
        ".calc-button.operation:hover{background:#2B3137;border-color:#6A737C}"
        ".calc-button.utility{background:#0D1014;color:#AEB6BD;font-size:11px}"
        ".calc-button.utility:hover{background:#171B20;color:#EEF1F3;border-color:#6A737C}"
        ".calc-button.utility.selected{background:#2B3137;color:#EEF1F3;border-color:#BEC7CF}"
        ".calc-button.memory-button{background:transparent;color:#AEB6BD;border-color:transparent;"
            "border-radius:5px;min-height:" + std::to_string(metrics.memory_height) + "px;font-size:10px}"
        ".calc-button.memory-button:hover{background:#171B20;color:#EEF1F3;border-color:transparent}"
        ".calc-button.clear{background:#171B20;color:#D19E47}"
        ".calc-button.clear:hover{background:#22272D;border-color:#D19E47}"
        ".calc-button.equals{background:#D7DDE2;color:#111418;border-color:#D7DDE2;font-size:16px}"
        ".calc-button.equals:hover{background:#EEF1F3;border-color:#EEF1F3}"
        ".history-list{background:#101318;border:1px solid #353A40;border-radius:10px}"
        ".history-row{padding:10px;border-bottom:1px solid #353A40;color:#D7DDE2;font-size:12px}";

    css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider, css.c_str(), -1);
    gtk_style_context_add_provider_for_display(
        gtk_widget_get_display(window),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void activate(GtkApplication* app, gpointer) {
    const auto& metrics = infiltrator::calc::ui::kDesktopMetrics;

    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Infiltrator Calc");
    gtk_window_set_default_size(
        GTK_WINDOW(window), metrics.default_width, metrics.default_height);

    GtkWidget* shell =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, metrics.section_gap);
    gtk_widget_add_css_class(shell, "shell");
    gtk_window_set_child(GTK_WINDOW(window), shell);
    apply_css(window);

    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(header, "header");
    gtk_box_append(GTK_BOX(shell), header);

    GtkWidget* title = gtk_label_new("Infiltrator Calc");
    gtk_widget_add_css_class(title, "brand-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_box_append(GTK_BOX(header), title);

    GtkWidget* history = toolbar_button("History");
    gtk_box_append(GTK_BOX(header), history);

    GtkWidget* mode_strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_add_css_class(mode_strip, "mode-strip");
    gtk_box_append(GTK_BOX(shell), mode_strip);
    gtk_box_append(GTK_BOX(mode_strip), mode_button(Mode::Standard));
    gtk_box_append(GTK_BOX(mode_strip), mode_button(Mode::Scientific));
    gtk_box_append(GTK_BOX(mode_strip), mode_button(Mode::Programmer));

    GtkWidget* display = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(display, "display");
    gtk_box_append(GTK_BOX(shell), display);

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
    gtk_box_append(GTK_BOX(shell), standard_panel);

    GtkWidget* memory_strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(standard_panel), memory_strip);
    for (const ButtonSpec& spec : infiltrator::calc::ui::kStandardMemory) {
        gtk_box_append(GTK_BOX(memory_strip), calc_button(spec));
    }

    GtkWidget* standard_grid = new_grid();
    gtk_box_append(GTK_BOX(standard_panel), standard_grid);
    fill_grid(
        standard_grid,
        infiltrator::calc::ui::kStandardKeypad.data(),
        infiltrator::calc::ui::kStandardKeypad.size());

    scientific_grid = new_grid();
    gtk_box_append(GTK_BOX(shell), scientific_grid);
    fill_grid(
        scientific_grid,
        infiltrator::calc::ui::kScientificKeypad.data(),
        infiltrator::calc::ui::kScientificKeypad.size());

    programmer_grid = new_grid();
    gtk_box_append(GTK_BOX(shell), programmer_grid);
    fill_grid(
        programmer_grid,
        infiltrator::calc::ui::kProgrammerKeypad.data(),
        infiltrator::calc::ui::kProgrammerKeypad.size());

    render_state();

    gtk_window_present(GTK_WINDOW(window));
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
