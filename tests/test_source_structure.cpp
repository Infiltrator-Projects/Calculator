// SPDX-License-Identifier: GPL-3.0-or-later
// Native C++17 regression contracts for Calculator's source-module ownership.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <array>
#include <vector>
#include <algorithm>
namespace fs = std::filesystem;

static void require(bool condition, const std::string& explanation) {
    if (!condition) throw std::runtime_error(explanation);
}
static std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "cannot read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}
static void required(const std::string& source, const std::string& needle,
                     const std::string& reason) {
    require(contains(source, needle), reason + ": " + needle);
}
static void forbidden(const std::string& source, const std::string& needle,
                      const std::string& reason) {
    require(!contains(source, needle), reason + ": " + needle);
}
static void file_exists(const fs::path& root, const std::string& relative) {
    require(fs::is_regular_file(root / relative), "missing architecture module: " + relative);
}
static std::size_t splitline_count(const std::string& data) {
    if (data.empty()) return 0;
    return std::count(data.begin(), data.end(), '\n') + (data.back() != '\n');
}

int main(int argc, char** argv) {
    try {
        require(argc <= 2, "Usage: test_source_structure [REPOSITORY_ROOT]");
        const fs::path root = argc == 2 ? argv[1] : ".";
        const std::string cmake = read(root / "CMakeLists.txt");
        const std::string ios = read(root / "ios/project.yml");
        const std::array<const char*, 5> advanced{{
            "src/core/advanced_tools.cpp",
            "src/core/advanced_tools_conversion.cpp",
            "src/core/advanced_tools_analysis.cpp",
            "src/core/advanced_tools_exact.cpp",
            "src/core/advanced_tools_number.cpp",
        }};
        for (const char* module : advanced) {
            file_exists(root, module);
            required(cmake, module, "CMake omits Advanced Tools module");
            forbidden(ios, std::string("../") + module,
                      "iOS regained per-file Calculator manifest duplication");
        }
        required(cmake, "src/core/evaluation_budget.cpp", "CMake omits evaluation-budget implementation");
        for (const char* tree : {"../src/core", "../src/ui"})
            required(ios, std::string("- path: ") + tree,
                     "iPhone no longer owns Calculator source tree");

        const std::string dispatcher = read(root / "src/core/advanced_tools.cpp");
        const std::array<const char*, 14> handlers{{
            "engineering_tool", "unit_tool", "network_tool", "storage_tool",
            "datetime_tool", "constants_tool", "statistics_tool", "graph_tool",
            "equation_tool", "exact_decimal_tool", "arbitrary_precision_tool",
            "complex_tool", "financial_tool", "number_utilities_tool",
        }};
        const std::array<const char*, 5> budgeted_handlers{{
            "graph_tool", "equation_tool", "exact_decimal_tool",
            "arbitrary_precision_tool", "financial_tool",
        }};
        for (const char* handler : handlers) {
            forbidden(dispatcher, std::string("ToolResult ") + handler + "(",
                      "Advanced Tools implementation leaked into dispatcher");
            const bool budgeted = std::any_of(
                budgeted_handlers.begin(), budgeted_handlers.end(),
                [handler](const char* entry) { return std::string(entry) == handler; });
            const std::string args = budgeted ? "input, active_budget" : "input";
            required(dispatcher, std::string("detail::") + handler + "(" + args + ")",
                     "Advanced Tools handler lost private routing");
        }
        require(splitline_count(dispatcher) < 200,
                "Advanced Tools dispatcher has grown into a god file");
        for (const char* module : {
                 "src/ui/calculator_ui_controller.cpp", "src/ui/calculator_ui_state.cpp",
                 "src/ui/calculator_ui_presentation.cpp", "src/app/linux_persistence.cpp",
                 "src/app/windows_platform.cpp", "src/app/windows_persistence.cpp"}) {
            file_exists(root, module);
            required(cmake, module, "CMake omits architecture module");
        }
        for (const char* module : {
                 "src/app/linux_entry.cpp", "src/app/cli_mode.cpp", "src/app/cli_mode.hpp"})
            file_exists(root, module);
        require(!fs::exists(root / "src/app/cli_main.cpp"),
                "obsolete separate CLI entry point returned");
        for (const char* token : {"add_executable(calculator-cli", "install(TARGETS calculator-cli"})
            forbidden(cmake, token, "CMake must not produce a separate Calculator CLI executable");
        for (const char* module : {"src/app/linux_entry.cpp", "src/app/cli_mode.cpp"})
            required(cmake, module, "CMake omits the Calculator process dispatcher");
        const std::string linux_entry = read(root / "src/app/linux_entry.cpp");
        required(linux_entry, "\"/cli\"", "Calculator lost its /cli mode");
        required(linux_entry, "calculator_gui_main", "Calculator entry no longer routes to GTK");

        const std::string controller = read(root / "src/ui/calculator_ui_controller.cpp");
        const std::string state = read(root / "src/ui/calculator_ui_state.cpp");
        const std::string presentation = read(root / "src/ui/calculator_ui_presentation.cpp");
        for (const char* method : {
                 "persistent_state_text", "load_persistent_state_text",
                 "recall_history", "recall_memory"}) {
            const auto signature = std::string("Controller::") + method + "(";
            forbidden(controller, signature, "Controller state leaked into dispatch unit");
            required(state, signature, "Controller state unit lost ownership");
        }
        for (const char* method : {
                 "additional_results", "programmer_bits", "format_display",
                 "button_label", "command_enabled"}) {
            const auto signature = std::string("Controller::") + method + "(";
            forbidden(controller, signature, "Controller presentation leaked into dispatch unit");
            required(presentation, signature, "Controller presentation unit lost ownership");
        }
        const std::string linux_main = read(root / "src/app/main.cpp");
        const std::string linux_persistence = read(root / "src/app/linux_persistence.cpp");
        for (const char* mechanism : {"g_get_user_data_dir", "g_get_user_config_dir",
                                      "infiltratr_atomic_file_write_bytes",
                                      "infiltratr_read_text_file_alloc"}) {
            forbidden(linux_main, mechanism, "Linux persistence leaked into main.cpp");
            required(linux_persistence, mechanism, "Linux persistence unit lost mechanism");
        }
        const std::string windows_main = read(root / "src/app/windows_main.cpp");
        const std::string windows_persistence = read(root / "src/app/windows_persistence.cpp");
        const std::string windows_platform = read(root / "src/app/windows_platform.cpp");
        const std::string registry = R"CALC(L"Software\\Infiltrator\\Calc")CALC";
        forbidden(windows_main, registry, "Windows Registry identity leaked to main.cpp");
        required(windows_persistence, registry, "Windows persistence lost Registry identity");
        for (const char* api : {"MultiByteToWideChar", "GetDpiForWindow"}) {
            forbidden(windows_main, api, "Win32 primitive leaked into windows_main.cpp");
            required(windows_platform, api, "Windows platform unit lost Win32 primitive");
        }
        require(!fs::exists(root / "tests/test_source_structure.py"),
                "retired Python source-structure test is still present");
        std::cout << "Calculator source modularity contract passed (native C++17).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Calculator source modularity contract failed: " << error.what() << '\n';
        return 1;
    }
}
