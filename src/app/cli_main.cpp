/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../core/session.hpp"

#include <infiltratr/posix.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

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

    const std::string native_path = path.string();
    char* contents = nullptr;
    std::size_t length = 0U;
    const InfiltratrIoResult status =
        infiltratr_read_text_file_alloc(
            native_path.c_str(), &contents, &length);
    if (status != INFILTRATR_IO_OK) {
        std::free(contents);
        return;
    }

    const bool loaded = session.load_function_definitions_text(
        std::string_view(contents, length));
    std::free(contents);
    if (!loaded) {
        std::cerr << "Warning: ignored malformed custom-functions file: "
                  << native_path << '\n';
    }
}

void save_functions(const calculator::Session& session) {
    const auto path = user_function_path();
    if (path.empty()) return;

    const std::string directory = path.parent_path().string();
    if (infiltratr_mkdir_parents(directory.c_str(), 0700U) != 0) return;

    const std::string text = session.function_definitions_text();
    const std::string native_path = path.string();
    (void)infiltratr_atomic_file_write_bytes(
        native_path.c_str(), INFILTRATR_ATOMIC_FILE_PRIVATE,
        text.data(), text.size());
}

bool solve(calculator::Session& session, const std::string& expression) {
    const auto result = session.evaluate_scientific(
        expression, calculator::AngleUnit::Degrees,
        calculator::kScientificDefaultDigits);
    if (!result.ok) {
        std::cerr << "Error: " << result.error << '\n';
        return false;
    }
    std::cout << (result.display.empty()
                      ? calculator::format_scientific_value(
                            result.value,
                            calculator::kScientificDefaultDigits)
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
