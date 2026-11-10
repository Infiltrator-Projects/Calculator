// SPDX-License-Identifier: GPL-3.0-or-later
#define main calculator_native_installer_entrypoint
#include "../support/tools/native_installer.cpp"
#undef main
#include <cctype>
#include <functional>
#include <stdexcept>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>

static void check(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
static std::string read_text(const fs::path &path) {
    std::ifstream in(path);
    check(bool(in), "Missing file under qualification");
    std::ostringstream result;
    result << in.rdbuf();
    return result.str();
}
struct Result { int status; std::string output; };
static Result capture(const std::function<void()> &action) {
    int pipefd[2];
    if (::pipe(pipefd) != 0) throw std::runtime_error("pipe failed");
    pid_t pid = ::fork();
    if (pid < 0) throw std::runtime_error("fork failed");
    if (pid == 0) {
        ::close(pipefd[0]);
        if (::dup2(pipefd[1], STDOUT_FILENO) < 0 ||
            ::dup2(pipefd[1], STDERR_FILENO) < 0) ::_exit(126);
        ::close(pipefd[1]);
        try {
            action();
            std::cout.flush();
            std::cerr.flush();
            ::_exit(0);
        } catch (...) {
            ::_exit(125);
        }
    }
    ::close(pipefd[1]);
    std::string output;
    char data[4096];
    for (;;) {
        const ssize_t bytes = ::read(pipefd[0], data, sizeof(data));
        if (bytes == 0) break;
        if (bytes == -1 && errno == EINTR) continue;
        if (bytes == -1) throw std::runtime_error("pipe read failed");
        output.append(data, static_cast<std::size_t>(bytes));
    }
    ::close(pipefd[0]);
    int status = 0;
    while (::waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) throw std::runtime_error("waitpid failed");
    }
    return {WIFEXITED(status) ? WEXITSTATUS(status) : 128, output};
}
static Result plan(const std::string &installed, const std::string &target) {
    return capture([&] {
        std::optional<std::string> version;
        if (installed != "not-installed") version = installed;
        const auto args = package_install_arguments(
            "/tmp/calculator rebuilt package.deb", target, version);
        for (const auto &arg : args) std::cout << arg << '\n';
    });
}
static Result installer(const std::vector<std::string> &arguments) {
    return capture([&] {
        std::vector<std::string> owned{"installer"};
        owned.insert(owned.end(), arguments.begin(), arguments.end());
        std::vector<char *> argv;
        for (auto &entry : owned) argv.push_back(entry.data());
        (void)calculator_native_installer_entrypoint(
            static_cast<int>(argv.size()), argv.data());
    });
}
int main() {
    try {
        const fs::path root = fs::current_path();
        const auto native = read_text(root/"support/tools/native_installer.cpp");
        const auto packaging = read_text(root/"debian/rules");
        const auto cmake = read_text(root/"CMakeLists.txt");
        auto manual = read_text(root/"docs/MANUAL.md");
        const auto generator = read_text(root/"support/release/build_manual_pdf.py");
        check(native.find("write_build_info(") == std::string::npos &&
              native.find(".calculator-native-build-info") == std::string::npos &&
              packaging.find(".calculator-native-build-info") == std::string::npos &&
              native.find("/Documentation/Calculator/BUILD-INFO") == std::string::npos,
              "Build metadata leaked into installed product");
        check(cmake.find("install(FILES LICENSE") == std::string::npos &&
              cmake.find("install(DIRECTORY DESTINATION \"/System/Resources/Documentation/Calculator\")") == std::string::npos &&
              cmake.find("add_custom_target(calculator-manual ALL") != std::string::npos &&
              cmake.find("DESTINATION \"/System/Resources/Documentation\" RENAME \"Calculator.pdf\"") != std::string::npos &&
              cmake.find("install(FILES \"${CALCULATOR_MANUAL_PDF}\"") != std::string::npos &&
              cmake.find("docs/MANUAL.txt") == std::string::npos,
              "Manual install contract drifted");
        check(generator.find("ModePicture") != std::string::npos &&
              generator.find("reportlab") != std::string::npos,
              "Illustrated manual generator unexpectedly changed");
        std::transform(manual.begin(), manual.end(), manual.begin(),
            [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
        for (const char *word : {"OPERATING INSTRUCTIONS", "STANDARD", "SCIENTIFIC",
            "PROGRAMMER", "ADVANCED TOOLS", "OPERATING PRINCIPLES AND ASSUMPTIONS",
            "LIMITS AND FAILURE POLICY"}) {
            check(manual.find(word) != std::string::npos, "Manual section missing");
        }
        const auto equal = plan("0.2.53", "0.2.53");
        check(equal.status == 0 &&
              equal.output.find("--reinstall") != std::string::npos &&
              equal.output.find("--allow-downgrades") != std::string::npos &&
              equal.output.find("/tmp/calculator rebuilt package.deb") != std::string::npos,
              "Same-version native package must force local reinstall");
        for (const char *v : {"not-installed","0.2.52","0.2.53~rc1"}) {
            const auto result=plan(v,"0.2.53");
            check(result.status == 0 &&
                  result.output.find("--allow-downgrades") == std::string::npos,
                  "New package installation must not permit downgrade");
        }
        for (const char *v : {"0.2.54","0.2.53+local1","1:0.2.52"}) {
            const auto result=plan(v,"0.2.53");
            check(result.status != 0 &&
                  result.output.find("refusing to replace installed Calculator") != std::string::npos,
                  "Installer incorrectly permitted downgrade");
        }
        const auto help=installer({"--help"});
        check(help.status == 0 &&
              help.output.find("--build-only --output DIRECTORY") != std::string::npos &&
              help.output.find("--profile") == std::string::npos &&
              help.output.find("--skip-tests") == std::string::npos,
              "Installer help should require qualified optimisation");
        for (const char *v : {"--profile=portable","--profile=native","--skip-tests"}) {
            const auto result=installer({v});
            check(result.status != 0 &&
                  result.output.find("unknown option") != std::string::npos,
                  "Unqualified option must be rejected");
        }
        for (const std::vector<std::string> args : {
            std::vector<std::string>{"--build-only"},
            {"--output","/tmp/calculator-build"},
            {"--build-only","--dry-run","--output","/tmp/calculator-build"}}) {
            const auto result=installer(args);
            check(result.status != 0 &&
                  result.output.find("--build-only") != std::string::npos,
                  "Partial build-only option accepted");
        }
        std::istringstream installed(
            "Package: unrelated\r\nStatus: install ok installed\r\nVersion: 99\r\n\r\n"
            "Package: calculator\r\nStatus: install ok installed\r\nVersion: 0.2.53\r\n");
        const auto active=package_version_from_status(installed,"calculator");
        check(active && *active == "0.2.53",
              "Canonical Calculator package status parse failed");
        std::istringstream removed(
            "Package: calculator\r\nStatus: deinstall ok config-files\r\nVersion: 0.2.53\r\n");
        check(!package_version_from_status(removed,"calculator"),
              "Deinstalled Calculator must not appear installed");
        std::cout << "Native Calculator installer policy regression passed\n";
    } catch (const std::exception &error) {
        std::cerr << "Installer regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
