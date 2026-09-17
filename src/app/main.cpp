/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../core/calculator.hpp"

#include <gtk/gtk.h>

#include <iomanip>
#include <sstream>
#include <string>

namespace {

GtkWidget* expression_entry = nullptr;
GtkWidget* result_label = nullptr;

std::string format_value(double value) {
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    return stream.str();
}

void calculate() {
    const char* text = gtk_editable_get_text(GTK_EDITABLE(expression_entry));
    const infiltrator::calc::Result result =
        infiltrator::calc::evaluate(text != nullptr ? text : "");

    if (result.ok) {
        const std::string formatted = format_value(result.value);
        gtk_label_set_text(GTK_LABEL(result_label), formatted.c_str());
    } else {
        std::string message = "Error: ";
        message += result.error;
        gtk_label_set_text(GTK_LABEL(result_label), message.c_str());
    }
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
        gtk_editable_set_text(GTK_EDITABLE(expression_entry), "");
        gtk_label_set_text(GTK_LABEL(result_label), "0");
        return;
    }

    const char* existing = gtk_editable_get_text(GTK_EDITABLE(expression_entry));
    std::string next = existing != nullptr ? existing : "";
    next += label;
    gtk_editable_set_text(GTK_EDITABLE(expression_entry), next.c_str());
    gtk_widget_grab_focus(expression_entry);
}

GtkWidget* make_button(const char* text) {
    GtkWidget* button = gtk_button_new_with_label(text);
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), nullptr);
    return button;
}

void apply_css() {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(
        provider,
        "* { font-family: 'MB Corpo S Title WEB', 'MB Corpo S', sans-serif; }"
        ".title { font-family: 'MB Corpo A Title Cond WEB', 'MB Corpo A', sans-serif; font-size: 22px; }"
        ".result { font-size: 30px; font-weight: 700; }"
        ".key { min-width: 56px; min-height: 46px; }",
        -1);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

void activate(GtkApplication* app, gpointer) {
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Infiltrator Calc");
    gtk_window_set_default_size(GTK_WINDOW(window), 420, 560);

    apply_css();

    GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(outer, 16);
    gtk_widget_set_margin_end(outer, 16);
    gtk_widget_set_margin_top(outer, 16);
    gtk_widget_set_margin_bottom(outer, 16);
    gtk_window_set_child(GTK_WINDOW(window), outer);

    GtkWidget* title = gtk_label_new("Infiltrator Calc");
    gtk_widget_add_css_class(title, "title");
    gtk_box_append(GTK_BOX(outer), title);

    expression_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(expression_entry), "Expression");
    gtk_widget_set_hexpand(expression_entry, TRUE);
    g_signal_connect(expression_entry, "activate", G_CALLBACK(on_activate), nullptr);
    gtk_box_append(GTK_BOX(outer), expression_entry);

    result_label = gtk_label_new("0");
    gtk_widget_set_halign(result_label, GTK_ALIGN_END);
    gtk_widget_add_css_class(result_label, "result");
    gtk_box_append(GTK_BOX(outer), result_label);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_widget_set_vexpand(grid, TRUE);
    gtk_widget_set_halign(grid, GTK_ALIGN_FILL);
    gtk_widget_set_valign(grid, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(outer), grid);

    const char* keys[][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "*"},
        {"1", "2", "3", "-"},
        {"0", ".", "C", "+"},
        {"(", ")", "^", "="},
    };

    for (gint row = 0; row < 5; ++row) {
        for (gint column = 0; column < 4; ++column) {
            GtkWidget* button = make_button(keys[row][column]);
            gtk_widget_add_css_class(button, "key");
            gtk_widget_set_hexpand(button, TRUE);
            gtk_grid_attach(GTK_GRID(grid), button, column, row, 1, 1);
        }
    }

    gtk_window_present(GTK_WINDOW(window));
    gtk_widget_grab_focus(expression_entry);
}

} // namespace

int main(int argc, char** argv) {
    GtkApplication* app = gtk_application_new(
        "net.ssmith.infiltrator.calc", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    const int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
