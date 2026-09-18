#!/usr/bin/env python3
from pathlib import Path

source = Path("src/app/main.cpp").read_text(encoding="utf-8")

required = {
    "compact desktop default": "gtk_window_set_default_size(GTK_WINDOW(window), 440, 690);",
    "compact key height": "min-height:34px;",
    "compact row spacing": "gtk_grid_set_row_spacing(GTK_GRID(grid), 5);",
    "compact column spacing": "gtk_grid_set_column_spacing(GTK_GRID(grid), 6);",
    "keys do not vertically stretch": "gtk_widget_set_vexpand(button, FALSE);",
    "keypad does not vertically stretch": "gtk_widget_set_vexpand(grid, FALSE);",
}
for name, needle in required.items():
    assert needle in source, f"{name} contract missing: {needle}"

for forbidden in (
    "gtk_window_set_default_size(GTK_WINDOW(window), 480, 820);",
    "min-height:50px;",
    "gtk_grid_set_row_spacing(GTK_GRID(grid), 10);",
    "gtk_widget_set_vexpand(button, TRUE);",
):
    assert forbidden not in source, f"oversized Linux desktop layout regressed: {forbidden}"

assert '{"±", "0", ".", "="}' in source
assert '{"1", "2", "3", "="}' in source

print("Linux desktop compact-layout contract passed.")
