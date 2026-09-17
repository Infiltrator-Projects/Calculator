/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../core/calculator.hpp"

#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace {

GtkWidget* expression_entry = nullptr;
GtkWidget* result_label = nullptr;
GtkWidget* status_label = nullptr;
GtkCssProvider* css_provider = nullptr;

constexpr const char* kUiFont = "MB Corpo S Title WEB";
constexpr const char* kBrandFont = "MB Corpo A Title Cond WEB";

std::string format_value(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fmtflags(0), std::ios::floatfield);
    stream << std::setprecision(15) << value;
    return stream.str();
}

bool font_family_available(const char* wanted) {
    PangoFontMap* font_map = PANGO_FONT_MAP(pango_cairo_font_map_get_default());
    PangoFontFamily** families = nullptr;
    int family_count = 0;
    pango_font_map_list_families(font_map, &families, &family_count);

    bool found = false;
    for (int i = 0; i < family_count; ++i) {
        const char* name = pango_font_family_get_name(families[i]);
        if (name != nullptr && g_ascii_strcasecmp(name, wanted) == 0) {
            found = true;
            break;
        }
    }
    g_free(families);
    return found;
}

const char* select_ui_font() {
    return font_family_available(kUiFont) ? kUiFont : "Sans";
}

const char* select_brand_font() {
    return font_family_available(kBrandFont) ? kBrandFont : select_ui_font();
}

void calculate() {
    const char* text = gtk_editable_get_text(GTK_EDITABLE(expression_entry));
    const infiltrator::calc::Result result =
        infiltrator::calc::evaluate(text != nullptr ? text : "");

    if (result.ok) {
        const std::string formatted = format_value(result.value);
        gtk_label_set_text(GTK_LABEL(result_label), formatted.c_str());
        gtk_label_set_text(GTK_LABEL(status_label), "READY");
        gtk_widget_remove_css_class(status_label, "fault");
    } else {
        std::string message = "Error: ";
        message += result.error;
        gtk_label_set_text(GTK_LABEL(result_label), message.c_str());
        gtk_label_set_text(GTK_LABEL(status_label), "CALCULATION ERROR");
        gtk_widget_add_css_class(status_label, "fault");
    }
}

void clear_calculation() {
    gtk_editable_set_text(GTK_EDITABLE(expression_entry), "");
    gtk_label_set_text(GTK_LABEL(result_label), "0");
    gtk_label_set_text(GTK_LABEL(status_label), "READY");
    gtk_widget_remove_css_class(status_label, "fault");
    gtk_widget_grab_focus(expression_entry);
}

void on_activate(GtkEntry*) {
    calculate();
}

void on_button_clicked(GtkButton* button, gpointer) {
    const char* label = gtk_button_get_label(button);
    if (label == nullptr) return;

    if (g_strcmp0(label, "=") == 0) {
        calculate();
        return;
    }

    if (g_strcmp0(label, "C") == 0) {
        clear_calculation();
        return;
    }

    const char* existing = gtk_editable_get_text(GTK_EDITABLE(expression_entry));
    std::string next = existing != nullptr ? existing : "";
    next += label;
    gtk_editable_set_text(GTK_EDITABLE(expression_entry), next.c_str());
    gtk_widget_grab_focus(expression_entry);
}

GtkWidget* make_button(const char* text, const char* css_class) {
    GtkWidget* button = gtk_button_new_with_label(text);
    gtk_widget_add_css_class(button, "calc-button");
    if (css_class != nullptr) gtk_widget_add_css_class(button, css_class);
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), nullptr);
    gtk_widget_set_hexpand(button, TRUE);
    gtk_widget_set_vexpand(button, TRUE);
    return button;
}

void apply_css(GtkWidget* window) {
    const std::string ui_font = select_ui_font();
    const std::string brand_font = select_brand_font();

    const std::string css =
        "* {"
        " font-family: \"" + ui_font + "\";"
        " font-weight: 400;"
        "}"
        "window {"
        " background: #050608;"
        " color: #E8ECEF;"
        "}"
        ".shell {"
        " background: #050608;"
        " padding: 20px;"
        "}"
        ".brand-title {"
        " font-family: \"" + brand_font + "\";"
        " font-size: 26px;"
        " font-weight: 400;"
        " color: #EEF1F3;"
        "}"
        ".mode {"
        " font-size: 11px;"
        " font-weight: 700;"
        " letter-spacing: 0.12em;"
        " color: #AEB6BD;"
        "}"
        ".display {"
        " background: #101318;"
        " border: 1px solid #353A40;"
        " border-radius: 12px;"
        " padding: 14px;"
        "}"
        ".expression {"
        " background: #0E1115;"
        " color: #AEB6BD;"
        " border: 1px solid #353A40;"
        " border-radius: 8px;"
        " padding: 9px 11px;"
        " min-height: 22px;"
        "}"
        ".result {"
        " font-family: \"" + brand_font + "\";"
        " font-size: 36px;"
        " color: #EEF1F3;"
        " padding-top: 8px;"
        "}"
        ".status {"
        " font-size: 10px;"
        " font-weight: 700;"
        " letter-spacing: 0.10em;"
        " color: #899198;"
        "}"
        ".status.fault {"
        " color: #C96B6B;"
        "}"
        ".calc-button {"
        " background: #171B20;"
        " color: #E8ECEF;"
        " border: 1px solid #353A40;"
        " border-radius: 10px;"
        " min-height: 52px;"
        " font-size: 16px;"
        "}"
        ".calc-button:hover {"
        " background: #22272D;"
        " border-color: #6A737C;"
        "}"
        ".calc-button:active {"
        " background: #2B3137;"
        "}"
        ".operation {"
        " background: #20252B;"
        " color: #D7DDE2;"
        " font-weight: 700;"
        "}"
        ".clear {"
        " color: #D19E47;"
        " font-weight: 700;"
        "}"
        ".equals {"
        " background: #D7DDE2;"
        " color: #111418;"
        " font-weight: 700;"
        "}"
        ".equals:hover {"
        " background: #EEF1F3;"
        "}"
        ".footer {"
        " color: #899198;"
        " font-size: 10px;"
        "}"
        ;

    css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider, css.c_str(), -1);
    gtk_style_context_add_provider_for_display(
        gtk_widget_get_display(window), GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void activate(GtkApplication* app, gpointer) {
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Infiltrator Calc");
    gtk_window_set_default_size(GTK_WINDOW(window), 390, 610);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    GtkWidget* shell = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(shell, "shell");
    gtk_window_set_child(GTK_WINDOW(window), shell);

    apply_css(window);

    GtkWidget* heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_bottom(2, 0);
    gtk_box_append(GTK_BOX(shell), heading);

    GtkWidget* title = gtk_label_new("Infiltrator Calc");
    gtk_widget_add_css_class(title, "brand-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(heading), title);

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(heading), spacer);

    GtkWidget* mode = gtk_label_new("STANDARD");
    gtk_widget_add_css_class(mode, "mode");
    gtk_widget_set_valign(mode, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(heading), mode);

    GtkWidget* display = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
    gtk_widget_add_css_class(display, "display");
    gtk_box_append(GTK_BOX(shell), display);

    expression_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(expression_entry), "Enter expression");
    gtk_widget_add_css_class(expression_entry, "expression");
    gtk_entry_set_alignment(GTK_ENTRY(expression_entry), 1.0f);
    gtk_widget_set_hexpand(expression_entry, TRUE);
    g_signal_connect(expression_entry, "activate", G_CALLBACK(on_activate), nullptr);
    gtk_box_append(GTK_BOX(display), expression_entry);

    result_label = gtk_label_new("0");
    gtk_widget_add_css_class(result_label, "result");
    gtk_widget_set_halign(result_label, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(display), result_label);

    status_label = gtk_label_new("READY");
    gtk_widget_add_css_class(status_label, "status");
    gtk_widget_set_halign(status_label, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(display), status_label);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 7);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 7);
    gtk_widget_set_vexpand(grid, TRUE);
    gtk_box_append(GTK_BOX(shell), grid);

    struct Key {
        const char* label;
        const char* css;
    };

    const Key keys[][4] = {
        {{"7", nullptr}, {"8", nullptr}, {"9", nullptr}, {"/", "operation"}},
        {{"4", nullptr}, {"5", nullptr}, {"6", nullptr}, {"*", "operation"}},
        {{"1", nullptr}, {"2", nullptr}, {"3", nullptr}, {"-", "operation"}},
        {{"0", nullptr}, {".", nullptr}, {"C", "clear"}, {"+", "operation"}},
        {{"(", "operation"}, {")", "operation"}, {"^", "operation"}, {"=", "equals"}},
    };

    for (guint row = 0; row < 5; ++row) {
        for (guint column = 0; column < 4; ++column) {
            GtkWidget* button = make_button(keys[row][column].label, keys[row][column].css);
            gtk_grid_attach(GTK_GRID(grid), button, static_cast<int>(column),
                            static_cast<int>(row), 1, 1);
        }
    }

    GtkWidget* footer = gtk_label_new("Standard arithmetic  ·  Keyboard ready");
    gtk_widget_add_css_class(footer, "footer");
    gtk_widget_set_halign(footer, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(shell), footer);

    gtk_window_present(GTK_WINDOW(window));
    gtk_widget_grab_focus(expression_entry);
}

} // namespace

int main(int argc, char** argv) {
    GtkApplication* app = gtk_application_new(
        "net.ssmith.infiltrator.calc", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    const int status = g_application_run(G_APPLICATION(app), argc, argv);
    if (css_provider != nullptr) g_object_unref(css_provider);
    g_object_unref(app);
    return status;
}
