/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../core/session.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::filesystem::path user_function_path() {
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        return std::filesystem::path(xdg) /
               "infiltrator-calc" / "custom-functions";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) /
               ".local" / "share" / "infiltrator-calc" /
               "custom-functions";
    }
    return {};
}

void load_functions(calculator::Session& session) {
    const auto path = user_function_path();
    if (path.empty()) return;
    std::ifstream input(path, std::ios::binary);
    if (!input) return;
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!session.load_function_definitions_text(contents.str())) {
        std::cerr << "Warning: ignored malformed custom-functions file: "
                  << path.string() << '\n';
    }
}

void save_functions(const calculator::Session& session) {
    const auto path = user_function_path();
    if (path.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return;
    output << session.function_definitions_text();
}

bool solve(calculator::Session& session, const std::string& expression) {
    const auto result = session.evaluate(
        expression, calculator::AngleUnit::Degrees);
    if (!result.ok) {
        std::cerr << "Error: " << result.error << '\n';
        return false;
    }
    std::cout << (result.display.empty()
                      ? calculator::format_value(result.value)
                      : result.display)
              << '\n';
    return true;
}

} // namespace

int main(int argc, char** argv) {
    calculator::Session session;
    load_functions(session);

    if (argc == 2) {
        const bool ok = solve(session, argv[1]);
        save_functions(session);
        return ok ? 0 : 1;
    }
    if (argc > 2) {
        std::cerr << "Usage: infiltrator-calc-cli [expression]\n";
        return 2;
    }

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            break;
        }
        if (line.empty() || line == "exit" || line == "quit") break;
        (void)solve(session, line);
    }
    save_functions(session);
    return 0;
}
