#!/usr/bin/env python3
from pathlib import Path

source = Path("src/app/main.cpp").read_text(encoding="utf-8")

required = {
    "compact desktop default": "gtk_window_set_default_size(GTK_WINDOW(window), 380, 620);",
    "compact key height": "min-height:32px;",
    "compact row spacing": "gtk_grid_set_row_spacing(GTK_GRID(grid), 5);",
    "compact column spacing": "gtk_grid_set_column_spacing(GTK_GRID(grid), 6);",
    "keys do not vertically stretch": "gtk_widget_set_vexpand(button, FALSE);",
    "keypad does not vertically stretch": "gtk_widget_set_vexpand(grid, FALSE);",
    "standard memory strip": 'const char* memory_keys[] = {"MC", "MR", "M+", "M−"};',
    "compact six-row standard keypad": '{"±", "0", ".", "="}',
    "hidden subtitle": "gtk_widget_set_visible(subtitle, FALSE);",
    "hidden footer": "gtk_widget_set_visible(footer, FALSE);",
    "memory strip styling": ".calc-button.memory-button",
}
for name, needle in required.items():
    assert needle in source, f"{name} contract missing: {needle}"

for forbidden in (
    "gtk_window_set_default_size(GTK_WINDOW(window), 480, 820);",
    "gtk_window_set_default_size(GTK_WINDOW(window), 440, 690);",
    "min-height:50px;",
    "gtk_grid_set_row_spacing(GTK_GRID(grid), 10);",
    "gtk_widget_set_vexpand(button, TRUE);",
    '{"MC", "MR", "M+", "M−"},\n        {"C", "⌫", "%", "÷"}',
):
    assert forbidden not in source, f"oversized/stale Linux desktop layout regressed: {forbidden}"

assert 'fill_grid(standard_grid, standard_keys, 6);' in source
assert '{"1", "2", "3", "="}' in source

print("Linux desktop compact-layout contract passed.")
