/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cli_mode.hpp"
#include "linux_persistence.hpp"

#include "../core/session.hpp"

#include <iostream>
#include <string>
#include <string_view>

namespace calculator::app {
namespace {

bool solve(calculator::Session& session, const ui::Controller& settings,
           const std::string& expression) {
    if(expression.size()>ui::Controller::kMaxExpressionBytes) {
        std::cerr << "Error: expression exceeds the input limit\n";
        return false;
    }
    const auto result = session.evaluate_scientific(
        expression, settings.state().angle_unit, settings.scientific_digits());
    if (!result.ok) {
        std::cerr << "Error: " << result.error << '\n';
        return false;
    }
    std::cout << (result.display.empty()
                      ? calculator::format_scientific_value(
                            result.value,
                            settings.scientific_digits())
                      : result.display)
              << '\n';
    return true;
}

} // namespace

int run_cli_mode(int argc, char** argv) {
    if (argc > 1) {
        std::cerr << "Usage: calculator /cli [expression]\n";
        return 2;
    }

    // The CLI has process-local evaluation/history but consumes and updates
    // the same durable Controller document as GTK. It changes only reusable
    // names, preserving GUI mode, precision and other semantic preferences.
    ui::Controller settings;
    LinuxPersistence persistence(settings);
    (void)persistence.load_controller_state();
    calculator::Session session;
    if(!session.load_function_definitions_text(settings.function_definitions_text()) ||
       !session.load_variables_text(settings.variables_text())) {
        std::cerr << "Error: cannot load reusable calculator state\n";
        return 1;
    }
    const std::string original_functions=session.function_definitions_text();
    const std::string original_variables=session.variables_text();
    const auto save=[&]() {
        const std::string functions=session.function_definitions_text();
        const std::string variables=session.variables_text();
        if(functions==original_functions && variables==original_variables) return true;
        if(!settings.load_function_definitions_text(functions) ||
           !settings.load_variables_text(variables) ||
           !persistence.save_controller_state()) {
            std::cerr << "Error: reusable state was not saved (I/O or concurrent edit conflict)\n";
            return false;
        }
        return true;
    };

    if (argc == 1) {
        const bool ok = solve(session, settings, argv[0]);
        return save() && ok ? 0 : 1;
    }

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            break;
        }
        if (line.empty() || line == "exit" || line == "quit") break;
        (void)solve(session, settings, line);
    }
    return save() ? 0 : 1;
}

} // namespace calculator::app
