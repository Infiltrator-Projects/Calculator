/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../core/programmer.hpp"
#include "../core/session.hpp"
#include "../ui/calculator_ui_contract.hpp"

#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

GtkWidget* expression_entry = nullptr;
GtkWidget* result_label = nullptr;
GtkWidget* status_label = nullptr;
GtkWidget* standard_panel = nullptr;
GtkWidget* standard_grid = nullptr;
GtkWidget* scientific_grid = nullptr;
GtkWidget* programmer_grid = nullptr;
GtkWidget* mode_buttons[3] = {nullptr, nullptr, nullptr};
GtkWidget* programmer_base_buttons[4] = {nullptr, nullptr, nullptr, nullptr};
GtkWidget* programmer_width_buttons[4] = {nullptr, nullptr, nullptr, nullptr};
GtkWidget* programmer_signed_button = nullptr;
GtkCssProvider* css_provider = nullptr;

infiltrator::calc::Session session;
bool degrees = true;

constexpr const char* kUiFont = "MB Corpo S Title WEB";
constexpr const char* kBrandFont = "MB Corpo A Title Cond WEB";
constexpr double kPi = 3.14159265358979323846;

using infiltrator::calc::ui::ButtonRole;
using infiltrator::calc::ui::ButtonSpec;
using infiltrator::calc::ui::Command;
using infiltrator::calc::ui::Mode;

Mode mode = Mode::Standard;

infiltrator::calc::ProgrammerBase programmer_base =
    infiltrator::calc::ProgrammerBase::Decimal;
infiltrator::calc::IntegerWidth programmer_width =
    infiltrator::calc::IntegerWidth::Bits64;
bool programmer_signed = false;

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

void status(const char* text, bool fault = false) {
    gtk_label_set_text(GTK_LABEL(status_label), text);
    if (fault) {
        gtk_widget_add_css_class(status_label, "fault");
    } else {
        gtk_widget_remove_css_class(status_label, "fault");
    }
}

void apply_css(GtkWidget* window);

void set_expression(const std::string& text) {
    gtk_editable_set_text(GTK_EDITABLE(expression_entry), text.c_str());
    gtk_editable_set_position(GTK_EDITABLE(expression_entry), -1);
    gtk_widget_grab_focus(expression_entry);
}

void insert_text(const char* text) {
    int position = gtk_editable_get_position(GTK_EDITABLE(expression_entry));
    gtk_editable_insert_text(GTK_EDITABLE(expression_entry), text, -1, &position);
    gtk_editable_set_position(GTK_EDITABLE(expression_entry), position);
    gtk_widget_grab_focus(expression_entry);
}

void backspace() {
    const int position = gtk_editable_get_position(GTK_EDITABLE(expression_entry));
    if (position > 0) {
        gtk_editable_delete_text(GTK_EDITABLE(expression_entry), position - 1, position);
    }
    gtk_widget_grab_focus(expression_entry);
}

bool current_value(double& value) {
    const char* text = gtk_editable_get_text(GTK_EDITABLE(expression_entry));
    const auto result = session.evaluate(text ? text : "");
    if (!result.ok) {
        gtk_label_set_text(
            GTK_LABEL(result_label),
            ("Error: " + result.error).c_str());
        status("CALCULATION ERROR", true);
        return false;
    }
    value = result.value;
    return true;
}

bool current_programmer_value(std::uint64_t& value) {
    const char* text = gtk_editable_get_text(GTK_EDITABLE(expression_entry));
    const auto result = infiltrator::calc::evaluate_programmer(
        text ? text : "", programmer_base, programmer_width);
    if (!result.ok) {
        gtk_label_set_text(
            GTK_LABEL(result_label),
            ("Error: " + result.error).c_str());
        status("PROGRAMMER ERROR", true);
        return false;
    }
    value = result.value;
    return true;
}

const char* programmer_base_name() {
    switch (programmer_base) {
    case infiltrator::calc::ProgrammerBase::Binary: return "BIN";
    case infiltrator::calc::ProgrammerBase::Octal: return "OCT";
    case infiltrator::calc::ProgrammerBase::Decimal: return "DEC";
    case infiltrator::calc::ProgrammerBase::Hexadecimal: return "HEX";
    }
    return "DEC";
}

unsigned programmer_width_bits() {
    return static_cast<unsigned>(programmer_width);
}

std::string programmer_status_text() {
    std::ostringstream out;
    out << "PROGRAMMER · " << programmer_base_name()
        << " · " << programmer_width_bits() << " BIT · "
        << (programmer_signed ? "SIGNED" : "UNSIGNED");
    return out.str();
}

void update_programmer_selector_state() {
    const bool bases[4] = {
        programmer_base == infiltrator::calc::ProgrammerBase::Binary,
        programmer_base == infiltrator::calc::ProgrammerBase::Octal,
        programmer_base == infiltrator::calc::ProgrammerBase::Decimal,
        programmer_base == infiltrator::calc::ProgrammerBase::Hexadecimal
    };
    const bool widths[4] = {
        programmer_width == infiltrator::calc::IntegerWidth::Bits8,
        programmer_width == infiltrator::calc::IntegerWidth::Bits16,
        programmer_width == infiltrator::calc::IntegerWidth::Bits32,
        programmer_width == infiltrator::calc::IntegerWidth::Bits64
    };

    for (int i = 0; i < 4; ++i) {
        if (programmer_base_buttons[i]) {
            if (bases[i]) gtk_widget_add_css_class(programmer_base_buttons[i], "selected");
            else gtk_widget_remove_css_class(programmer_base_buttons[i], "selected");
        }
        if (programmer_width_buttons[i]) {
            if (widths[i]) gtk_widget_add_css_class(programmer_width_buttons[i], "selected");
            else gtk_widget_remove_css_class(programmer_width_buttons[i], "selected");
        }
    }

    if (programmer_signed_button) {
        if (programmer_signed) gtk_widget_add_css_class(programmer_signed_button, "selected");
        else gtk_widget_remove_css_class(programmer_signed_button, "selected");
    }
}

void calculate_programmer() {
    std::uint64_t value = 0;
    if (!current_programmer_value(value)) return;

    const std::string formatted = infiltrator::calc::format_programmer(
        value, programmer_base, programmer_width, programmer_signed);
    gtk_label_set_text(GTK_LABEL(result_label), formatted.c_str());

    const std::string state = programmer_status_text();
    status(state.c_str());
}

void calculate() {
    if (mode == Mode::Programmer) {
        calculate_programmer();
        return;
    }

    double value = 0.0;
    if (!current_value(value)) return;
    gtk_label_set_text(GTK_LABEL(result_label), format_value(value).c_str());

    if (mode == Mode::Scientific) {
        status(degrees ? "SCIENTIFIC · DEGREES" : "SCIENTIFIC · RADIANS");
    } else {
        status("READY");
    }
}

void clear_calculation() {
    set_expression("");
    gtk_label_set_text(GTK_LABEL(result_label), "0");

    if (mode == Mode::Programmer) {
        const std::string state = programmer_status_text();
        status(state.c_str());
    } else if (mode == Mode::Scientific) {
        status(degrees ? "SCIENTIFIC · DEGREES" : "SCIENTIFIC · RADIANS");
    } else {
        status("READY");
    }
}

void show_history(GtkWidget*, gpointer) {
    GtkWidget* window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), "Calculation History");
    gtk_window_set_default_size(GTK_WINDOW(window), 560, 500);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
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

    for (auto it = session.history().rbegin(); it != session.history().rend(); ++it) {
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
            session.clear_history();
            gtk_window_destroy(history_window);
        }),
        window);
    gtk_box_append(GTK_BOX(root), clear);

    apply_css(window);
    gtk_window_present(GTK_WINDOW(window));
}

void unary_transform(const char* op) {
    double value = 0.0;
    if (!current_value(value)) return;

    if (g_strcmp0(op, "±") == 0) {
        value = -value;
    } else if (g_strcmp0(op, "x²") == 0) {
        value *= value;
    } else if (g_strcmp0(op, "√") == 0) {
        if (value < 0.0) {
            status("DOMAIN ERROR", true);
            return;
        }
        value = std::sqrt(value);
    } else if (g_strcmp0(op, "1/x") == 0) {
        if (value == 0.0) {
            status("DIVISION BY ZERO", true);
            return;
        }
        value = 1.0 / value;
    }

    set_expression(format_value(value));
    gtk_label_set_text(GTK_LABEL(result_label), format_value(value).c_str());
    status("READY");
}

void insert_function(const char* name) {
    std::string text = name;
    text += '(';
    insert_text(text.c_str());
}

void scientific_transform(const char* name) {
    double value = 0.0;
    if (!current_value(value)) return;

    double argument = value;
    if (degrees &&
        (g_strcmp0(name, "sin") == 0 ||
         g_strcmp0(name, "cos") == 0 ||
         g_strcmp0(name, "tan") == 0)) {
        argument = value * kPi / 180.0;
    }

    if (g_strcmp0(name, "sin") == 0) value = std::sin(argument);
    else if (g_strcmp0(name, "cos") == 0) value = std::cos(argument);
    else if (g_strcmp0(name, "tan") == 0) value = std::tan(argument);
    else if (g_strcmp0(name, "asin") == 0) {
        value = std::asin(value);
        if (degrees) value = value * 180.0 / kPi;
    } else if (g_strcmp0(name, "acos") == 0) {
        value = std::acos(value);
        if (degrees) value = value * 180.0 / kPi;
    } else if (g_strcmp0(name, "atan") == 0) {
        value = std::atan(value);
        if (degrees) value = value * 180.0 / kPi;
    } else if (g_strcmp0(name, "ln") == 0) value = std::log(value);
    else if (g_strcmp0(name, "log") == 0) value = std::log10(value);
    else if (g_strcmp0(name, "exp") == 0) value = std::exp(value);
    else if (g_strcmp0(name, "abs") == 0) value = std::fabs(value);

    if (!std::isfinite(value)) {
        status("DOMAIN ERROR", true);
        return;
    }

    set_expression(format_value(value));
    gtk_label_set_text(GTK_LABEL(result_label), format_value(value).c_str());
    status(degrees ? "SCIENTIFIC · DEGREES" : "SCIENTIFIC · RADIANS");
}

void update_mode_ui() {
    gtk_widget_set_visible(standard_panel, mode == Mode::Standard);
    gtk_widget_set_visible(scientific_grid, mode == Mode::Scientific);
    gtk_widget_set_visible(programmer_grid, mode == Mode::Programmer);

    for (int i = 0; i < 3; ++i) {
        if (!mode_buttons[i]) continue;
        if (i == static_cast<int>(mode)) {
            gtk_widget_add_css_class(mode_buttons[i], "selected");
        } else {
            gtk_widget_remove_css_class(mode_buttons[i], "selected");
        }
    }

    if (mode == Mode::Scientific) {
        status(degrees ? "SCIENTIFIC · DEGREES" : "SCIENTIFIC · RADIANS");
    } else if (mode == Mode::Programmer) {
        update_programmer_selector_state();
        const std::string state = programmer_status_text();
        status(state.c_str());
    } else {
        status("READY");
    }

    gtk_widget_grab_focus(expression_entry);
}

void on_mode_clicked(GtkButton*, gpointer data) {
    const int encoded = GPOINTER_TO_INT(data);
    mode = static_cast<Mode>(encoded - 1);
    update_mode_ui();
}

void programmer_mode_change(Command command) {
    switch (command) {
    case Command::BaseBin:
        programmer_base = infiltrator::calc::ProgrammerBase::Binary;
        break;
    case Command::BaseOct:
        programmer_base = infiltrator::calc::ProgrammerBase::Octal;
        break;
    case Command::BaseDec:
        programmer_base = infiltrator::calc::ProgrammerBase::Decimal;
        break;
    case Command::BaseHex:
        programmer_base = infiltrator::calc::ProgrammerBase::Hexadecimal;
        break;
    case Command::Width8:
        programmer_width = infiltrator::calc::IntegerWidth::Bits8;
        break;
    case Command::Width16:
        programmer_width = infiltrator::calc::IntegerWidth::Bits16;
        break;
    case Command::Width32:
        programmer_width = infiltrator::calc::IntegerWidth::Bits32;
        break;
    case Command::Width64:
        programmer_width = infiltrator::calc::IntegerWidth::Bits64;
        break;
    case Command::ToggleSigned:
        programmer_signed = !programmer_signed;
        break;
    default:
        return;
    }

    update_programmer_selector_state();
    const char* expression =
        gtk_editable_get_text(GTK_EDITABLE(expression_entry));

    if (expression && *expression) {
        calculate_programmer();
    } else {
        const std::string state = programmer_status_text();
        status(state.c_str());
    }
}

void on_activate(GtkEntry*) {
    calculate();
}

void on_button_clicked(GtkButton* button, gpointer data) {
    const auto* spec = static_cast<const ButtonSpec*>(data);
    if (spec == nullptr) return;

    const Command command = spec->command;
    const std::string label(spec->label);

    if (mode == Mode::Programmer) {
        if (infiltrator::calc::ui::is_programmer_selector(command)) {
            programmer_mode_change(command);
            return;
        }
        if (command == Command::Equals) {
            calculate_programmer();
            return;
        }
        if (command == Command::AllClear) {
            clear_calculation();
            return;
        }
        if (command == Command::Backspace) {
            backspace();
            return;
        }

        const std::string_view insertion =
            infiltrator::calc::ui::insertion_text(command);
        if (!insertion.empty()) {
            insert_text(std::string(insertion).c_str());
        }
        return;
    }

    switch (command) {
    case Command::ToggleDegrees:
        degrees = !degrees;
        gtk_button_set_label(button, degrees ? "DEG" : "RAD");
        status(degrees ? "SCIENTIFIC · DEGREES" : "SCIENTIFIC · RADIANS");
        return;
    case Command::Equals:
        calculate();
        return;
    case Command::Clear:
        clear_calculation();
        return;
    case Command::Backspace:
        backspace();
        return;
    case Command::Negate:
    case Command::Square:
    case Command::SquareRoot:
    case Command::Reciprocal:
        unary_transform(label.c_str());
        return;
    case Command::MemoryClear:
        session.memory_clear();
        status("MEMORY CLEARED");
        return;
    case Command::MemoryRecall:
        insert_text(format_value(session.memory_recall()).c_str());
        status("MEMORY RECALL");
        return;
    case Command::MemoryAdd:
    case Command::MemorySubtract: {
        double value = 0.0;
        if (current_value(value)) {
            if (command == Command::MemoryAdd) session.memory_add(value);
            else session.memory_subtract(value);
            status("MEMORY UPDATED");
        }
        return;
    }
    case Command::Sin:
    case Command::Cos:
    case Command::Tan:
    case Command::Asin:
    case Command::Acos:
    case Command::Atan:
    case Command::Ln:
    case Command::Log10:
    case Command::Exp:
    case Command::Abs:
        scientific_transform(label.c_str());
        return;
    case Command::Pi:
        insert_text("pi");
        return;
    case Command::Euler:
        insert_text("e");
        return;
    case Command::Factorial:
        insert_text("!");
        return;
    case Command::CubeRoot:
        insert_function("cbrt");
        return;
    default:
        break;
    }

    const std::string_view insertion =
        infiltrator::calc::ui::insertion_text(command);
    if (!insertion.empty()) {
        insert_text(std::string(insertion).c_str());
    }
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
    default: break;
    }
}

void fill_grid(GtkWidget* grid, const ButtonSpec* specs, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        const int row = static_cast<int>(index / 4U);
        const int column = static_cast<int>(index % 4U);
        GtkWidget* button = calc_button(specs[index]);
        gtk_grid_attach(GTK_GRID(grid), button, column, row, 1, 1);
        remember_programmer_button(specs[index].command, button);
    }
}

void apply_css(GtkWidget* window) {
    const std::string ui = ui_font();
    const std::string brand = brand_font();

    const std::string css =
        "*{font-family:\"" + ui + "\";font-weight:400}"
        "window,.shell{background:#050608;color:#E8ECEF}"
        ".shell{padding:10px}"
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
        ".calc-button{border:1px solid #353A40;border-radius:8px;min-height:32px;"
            "font-size:13px;font-weight:700;padding:0}"
        ".calc-button.number{background:#171B20;color:#EEF1F3}"
        ".calc-button.number:hover{background:#22272D;border-color:#6A737C}"
        ".calc-button.operation{background:#20252B;color:#D7DDE2}"
        ".calc-button.operation:hover{background:#2B3137;border-color:#6A737C}"
        ".calc-button.utility{background:#0D1014;color:#AEB6BD;font-size:11px}"
        ".calc-button.utility:hover{background:#171B20;color:#EEF1F3;border-color:#6A737C}"
        ".calc-button.utility.selected{background:#2B3137;color:#EEF1F3;border-color:#BEC7CF}"
        ".calc-button.memory-button{background:transparent;color:#AEB6BD;border-color:transparent;"
            "border-radius:5px;min-height:22px;font-size:10px}"
        ".calc-button.memory-button:hover{background:#171B20;color:#EEF1F3;border-color:transparent}"
        ".calc-button.clear{background:#171B20;color:#D19E47}"
        ".calc-button.clear:hover{background:#22272D;border-color:#D19E47}"
        ".calc-button.equals{background:#D7DDE2;color:#111418;border-color:#D7DDE2;font-size:16px}"
        ".calc-button.equals:hover{background:#EEF1F3;border-color:#EEF1F3}"
        ".history-list{background:#101318;border:1px solid #353A40;border-radius:10px}"
        ".history-row{padding:10px;border-bottom:1px solid #353A40;color:#D7DDE2;font-size:12px}"
        ".footer{color:#899198;font-size:9px;padding-top:0}";

    css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider, css.c_str(), -1);
    gtk_style_context_add_provider_for_display(
        gtk_widget_get_display(window),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void activate(GtkApplication* app, gpointer) {
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Infiltrator Calc");
    gtk_window_set_default_size(
        GTK_WINDOW(window),
        infiltrator::calc::ui::kDesktopMetrics.default_width,
        infiltrator::calc::ui::kDesktopMetrics.default_height);

    GtkWidget* shell = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        infiltrator::calc::ui::kDesktopMetrics.section_gap);
    gtk_widget_add_css_class(shell, "shell");
    gtk_window_set_child(GTK_WINDOW(window), shell);
    apply_css(window);

    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(header, "header");
    gtk_box_append(GTK_BOX(shell), header);

    GtkWidget* title_stack = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(title_stack, TRUE);
    gtk_box_append(GTK_BOX(header), title_stack);

    GtkWidget* title = gtk_label_new("Infiltrator Calc");
    gtk_widget_add_css_class(title, "brand-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(title_stack), title);

    GtkWidget* subtitle = gtk_label_new("PRECISION DESKTOP CALCULATOR");
    gtk_widget_add_css_class(subtitle, "brand-subtitle");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(title_stack), subtitle);
    gtk_widget_set_visible(subtitle, FALSE);

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
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(expression_entry),
        "Expression");
    gtk_widget_add_css_class(expression_entry, "expression");
    gtk_entry_set_alignment(GTK_ENTRY(expression_entry), 1);
    g_signal_connect(expression_entry, "activate", G_CALLBACK(on_activate), nullptr);
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
        GtkWidget* button = calc_button(spec);
        gtk_widget_set_hexpand(button, TRUE);
        gtk_box_append(GTK_BOX(memory_strip), button);
    }

    standard_grid = new_grid();
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

    GtkWidget* footer =
        gtk_label_new("Keyboard ready · Variables, memory and history retained");
    gtk_widget_add_css_class(footer, "footer");
    gtk_widget_set_halign(footer, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(shell), footer);
    gtk_widget_set_visible(footer, FALSE);

    gtk_widget_set_visible(scientific_grid, FALSE);
    gtk_widget_set_visible(programmer_grid, FALSE);
    update_mode_ui();

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
