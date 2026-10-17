/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cli_mode.hpp"

#include <cstring>

// The GTK shell remains in main.cpp. CMake gives that translation unit a
// source-local main=calculator_gui_main definition so this small dispatcher is
// the single process entry point without duplicating or forking the GUI shell.
int calculator_gui_main(int argc, char** argv);

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "/cli") == 0) {
        return calculator::app::run_cli_mode(argc - 2, argv + 2);
    }
    return calculator_gui_main(argc, argv);
}
