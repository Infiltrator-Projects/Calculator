// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 1993-2026 Shannon Smith
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr const char *kPayloadMarker =
    "\n__INFILTRATOR_CALCULATOR_NATIVE_PAYLOAD_V1__\n";
constexpr const char *kPackageName = "infiltrator-calculator";
constexpr const char *kExecutable = "/usr/bin/infiltrator-calc";
constexpr const char *kBuildInfo =
    "/usr/share/doc/infiltrator-calculator/BUILD-INFO";

[[noreturn]] void fail(const std::string &message)
{
    std::cerr << "Error: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

bool executable(const std::string &path)
{
    struct stat status {};
    return ::access(path.c_str(), X_OK) == 0 &&
           ::stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode);
}

int run_process(const std::vector<std::string> &arguments,
                const std::optional<fs::path> &working_directory = std::nullopt,
                bool quiet = false)
{
    if (arguments.empty()) return 125;

    const pid_t child = ::fork();
    if (child < 0) fail(std::string("fork failed: ") + std::strerror(errno));
    if (child == 0) {
        if (working_directory && ::chdir(working_directory->c_str()) != 0)
            _exit(126);
        if (quiet) {
            FILE *null_file = std::fopen("/dev/null", "w");
            if (null_file) {
                const int descriptor = fileno(null_file);
                (void)::dup2(descriptor, STDOUT_FILENO);
                (void)::dup2(descriptor, STDERR_FILENO);
            }
        }
        std::vector<char *> argv;
        argv.reserve(arguments.size() + 1U);
        for (const std::string &argument : arguments)
            argv.push_back(const_cast<char *>(argument.c_str()));
        argv.push_back(nullptr);
        ::execv(argv.front(), argv.data());
        _exit(errno == ENOENT ? 127 : 126);
    }

    int status = 0;
    while (::waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        fail(std::string("waitpid failed: ") + std::strerror(errno));
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 125;
}

void run_required(const std::vector<std::string> &arguments,
                  const std::optional<fs::path> &working_directory = std::nullopt)
{
    const int result = run_process(arguments, working_directory, false);
    if (result != 0) {
        std::ostringstream message;
        message << "command failed with status " << result << ": "
                << arguments.front();
        fail(message.str());
    }
}

std::vector<std::string> privileged(const std::vector<std::string> &arguments,
                                    bool system_package_mode)
{
    if (system_package_mode || ::geteuid() == 0) return arguments;
    if (!executable("/usr/bin/sudo"))
        fail("sudo is required for prerequisite and package installation");
    std::vector<std::string> result{ "/usr/bin/sudo", "--" };
    result.insert(result.end(), arguments.begin(), arguments.end());
    return result;
}

std::string read_first_line(const fs::path &path)
{
    std::ifstream input(path);
    std::string line;
    if (!input || !std::getline(input, line))
        fail("cannot read " + path.string());
    while (!line.empty() &&
           (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' ||
            line.back() == '\t'))
        line.pop_back();
    if (line.empty()) fail("empty value in " + path.string());
    return line;
}

fs::path self_path()
{
    std::vector<char> buffer(4096U);
    for (;;) {
        const ssize_t length =
            ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1U);
        if (length < 0)
            fail(std::string("cannot resolve /proc/self/exe: ") +
                 std::strerror(errno));
        if (static_cast<std::size_t>(length) < buffer.size() - 1U) {
            buffer[static_cast<std::size_t>(length)] = '\0';
            return fs::path(buffer.data());
        }
        buffer.resize(buffer.size() * 2U);
    }
}

void extract_embedded_payload(const fs::path &archive_path)
{
    const fs::path executable_path = self_path();
    std::ifstream input(executable_path, std::ios::binary);
    if (!input) fail("cannot open running installer payload");
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size <= 0) fail("running installer has invalid size");
    input.seekg(0, std::ios::beg);

    std::vector<char> bytes(static_cast<std::size_t>(size));
    if (!input.read(bytes.data(), size)) fail("cannot read running installer");

    const std::string marker(kPayloadMarker);
    auto found = std::find_end(bytes.begin(), bytes.end(), marker.begin(), marker.end());
    if (found == bytes.end())
        fail("embedded Calculator source payload marker is missing");
    const auto payload_begin = found + static_cast<std::ptrdiff_t>(marker.size());
    if (payload_begin == bytes.end()) fail("embedded Calculator source payload is empty");

    std::ofstream output(archive_path, std::ios::binary | std::ios::trunc);
    if (!output) fail("cannot create temporary Calculator source archive");
    output.write(&*payload_begin, static_cast<std::streamsize>(bytes.end() - payload_begin));
    if (!output) fail("cannot write temporary Calculator source archive");
}

fs::path make_temp_directory()
{
    std::string pattern = "/tmp/infiltrator-calculator-native-XXXXXX";
    std::vector<char> storage(pattern.begin(), pattern.end());
    storage.push_back('\0');
    char *created = ::mkdtemp(storage.data());
    if (!created)
        fail(std::string("mkdtemp failed: ") + std::strerror(errno));
    return fs::path(created);
}

bool gtk4_available()
{
    if (!executable("/usr/bin/pkg-config")) return false;
    return run_process({ "/usr/bin/pkg-config", "--exists", "gtk4" }, std::nullopt,
                       true) == 0;
}

bool prerequisites_available()
{
    static const char *paths[] = {
        "/usr/bin/g++",          "/usr/bin/cmake",        "/usr/bin/ctest",
        "/usr/bin/dpkg-buildpackage", "/usr/bin/dpkg-query", "/usr/bin/apt-get",
        "/usr/bin/tar",          "/usr/bin/gzip",         "/usr/bin/xvfb-run",
        "/usr/bin/timeout"
    };
    for (const char *path : paths)
        if (!executable(path)) return false;
    return gtk4_available();
}

void ensure_prerequisites(bool system_package_mode, bool dry_run)
{
    if (prerequisites_available()) return;
    if (dry_run) {
        std::cout << "Dry run: build prerequisites are missing; no packages were changed.\n";
        return;
    }

    std::cout << "Installing Calculator native-build prerequisites...\n";
    run_required(privileged({ "/usr/bin/apt-get", "-o", "DPkg::Lock::Timeout=300",
                              "update" },
                            system_package_mode));
    run_required(privileged(
        { "/usr/bin/apt-get", "-o", "DPkg::Lock::Timeout=300", "install", "-y",
          "build-essential", "cmake", "debhelper-compat", "dpkg-dev", "libgtk-4-dev",
          "pkg-config", "xvfb", "tar", "gzip", "ca-certificates" },
        system_package_mode));

    if (!prerequisites_available())
        fail("required Calculator native-build prerequisites remain unavailable");
}

bool package_installed(const std::string &package)
{
    std::ifstream input("/var/lib/dpkg/status");
    if (!input) return false;
    std::string line;
    bool matching_package = false;
    bool installed_status = false;
    while (std::getline(input, line)) {
        if (line.empty() || line == "\r") {
            if (matching_package && installed_status) return true;
            matching_package = false;
            installed_status = false;
            continue;
        }
        if (line.rfind("Package: ", 0) == 0)
            matching_package = line.substr(9U) == package;
        else if (line == "Status: install ok installed")
            installed_status = true;
    }
    return matching_package && installed_status;
}

bool tree_has_suffix(const fs::path &root, const std::string &suffix)
{
    std::error_code error;
    if (!fs::exists(root, error)) return false;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied,
                                             error),
         end;
         it != end; it.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (!it->is_regular_file(error)) continue;
        const std::string name = it->path().filename().string();
        if (name.size() >= suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
            return true;
    }
    return false;
}

bool build_tree_contains(const fs::path &root, const std::string &needle)
{
    std::error_code error;
    if (!fs::exists(root, error)) return false;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied,
                                             error),
         end;
         it != end; it.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (!it->is_regular_file(error)) continue;
        const std::string filename = it->path().filename().string();
        if (filename != "flags.make" && filename != "CMakeCache.txt") continue;
        std::ifstream input(it->path());
        std::ostringstream text;
        text << input.rdbuf();
        if (text.str().find(needle) != std::string::npos) return true;
    }
    return false;
}

fs::path find_debian_package(const fs::path &directory, const std::string &version)
{
    const std::string prefix = "infiltrator-calculator_" + version + "_";
    std::error_code error;
    for (const auto &entry : fs::directory_iterator(directory, error)) {
        if (error) break;
        if (!entry.is_regular_file(error)) continue;
        const std::string name = entry.path().filename().string();
        if (name.size() > prefix.size() + 4U && name.rfind(prefix, 0) == 0 &&
            name.compare(name.size() - 4U, 4U, ".deb") == 0)
            return entry.path();
    }
    fail("final Calculator Debian package was not produced");
}

void write_build_info(const fs::path &source_root, const std::string &profile,
                      const std::string &flags, bool trained_pgo)
{
    const fs::path path = source_root / ".calculator-native-build-info";
    std::ofstream output(path, std::ios::trunc);
    if (!output) fail("cannot create native build information");
    output << "Profile: " << profile << '\n';
    output << "PGO: " << (trained_pgo ? "trained two-pass profile" : "disabled") << '\n';
    output << "C/C++ flags: " << flags << '\n';
    output << "Installed by: Calculator hardware-native installer\n";
    if (!output) fail("cannot write native build information");
}

std::string join_options(const std::vector<std::string> &options)
{
    std::ostringstream text;
    for (std::size_t index = 0; index < options.size(); ++index) {
        if (index) text << ' ';
        text << options[index];
    }
    return text.str();
}

void usage()
{
    std::cout
        << "Calculator hardware-native installer\n\n"
        << "Usage: infiltrator-calculator-<version>-native-installer.run [options]\n\n"
        << "Options:\n"
        << "  --profile native|aggressive|portable\n"
        << "  --jobs NUMBER\n"
        << "  --skip-tests\n"
        << "  --no-strip\n"
        << "  --dry-run\n"
        << "  --system-package-mode\n"
        << "  -h, --help\n";
}

int positive_integer(const std::string &text)
{
    if (text.empty()) return 0;
    char *end = nullptr;
    errno = 0;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || !end || *end != '\0' || value < 1L || value > 1024L) return 0;
    return static_cast<int>(value);
}

} // namespace

int main(int argc, char **argv)
{
    std::string profile = "native";
    int jobs = 0;
    bool skip_tests = false;
    bool no_strip = false;
    bool dry_run = false;
    bool system_package_mode = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "-h" || argument == "--help") {
            usage();
            return EXIT_SUCCESS;
        }
        if (argument == "--profile") {
            if (++index >= argc) fail("--profile requires a value");
            profile = argv[index];
        } else if (argument.rfind("--profile=", 0) == 0) {
            profile = argument.substr(10U);
        } else if (argument == "--jobs") {
            if (++index >= argc) fail("--jobs requires a value");
            jobs = positive_integer(argv[index]);
            if (jobs == 0) fail("--jobs requires a positive integer up to 1024");
        } else if (argument.rfind("--jobs=", 0) == 0) {
            jobs = positive_integer(argument.substr(7U));
            if (jobs == 0) fail("--jobs requires a positive integer up to 1024");
        } else if (argument == "--skip-tests") {
            skip_tests = true;
        } else if (argument == "--no-strip") {
            no_strip = true;
        } else if (argument == "--dry-run") {
            dry_run = true;
        } else if (argument == "--system-package-mode") {
            system_package_mode = true;
        } else {
            fail("unknown option: " + argument);
        }
    }

    if (profile != "native" && profile != "aggressive" && profile != "portable")
        fail("profile must be native, aggressive, or portable");
    if (system_package_mode && ::geteuid() != 0)
        fail("--system-package-mode requires root");
    if (::geteuid() == 0 && !system_package_mode)
        fail("do not run this installer as root without --system-package-mode");

    ensure_prerequisites(system_package_mode, dry_run);

    std::cout << "Calculator native installation plan\n"
              << "  Profile: " << profile << '\n'
              << "  Tests:   " << (skip_tests ? "skipped" : "enabled") << '\n'
              << "  Strip:   " << (no_strip ? "disabled" : "enabled") << '\n';
    if (jobs > 0) std::cout << "  Jobs:    " << jobs << '\n';
    if (dry_run) {
        std::cout << "Dry run: no source was built and no package was installed.\n";
        return EXIT_SUCCESS;
    }

    const fs::path work = make_temp_directory();
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    } cleanup{ work };

    const fs::path archive = work / "source.tar.gz";
    extract_embedded_payload(archive);
    run_required({ "/usr/bin/tar", "-xzf", archive.string(), "-C", work.string() });

    fs::path source_root;
    std::error_code error;
    for (const auto &entry : fs::directory_iterator(work, error)) {
        if (error) break;
        if (!entry.is_directory(error)) continue;
        const std::string name = entry.path().filename().string();
        if (name.rfind("Calculator-", 0) == 0) {
            source_root = entry.path();
            break;
        }
    }
    if (source_root.empty()) fail("embedded source archive has no Calculator source root");

    const std::string version = read_first_line(source_root / "VERSION");
    const fs::path pgo_directory = work / "pgo";
    if (!fs::create_directory(pgo_directory, error) || error)
        fail("cannot create PGO profile directory");

    std::vector<std::string> build_options;
    if (skip_tests) build_options.emplace_back("nocheck");
    if (no_strip) build_options.emplace_back("nostrip");
    if (jobs > 0) build_options.emplace_back("parallel=" + std::to_string(jobs));
    if (!build_options.empty())
        ::setenv("DEB_BUILD_OPTIONS", join_options(build_options).c_str(), 1);

    const bool aggressive = profile == "aggressive";
    std::string base_flags;
    std::string base_ldflags;
    if (profile == "portable") {
        base_flags = "-O2";
    } else {
        base_flags = "-O3 -march=native -mtune=native -flto=auto";
        base_ldflags = "-flto=auto";
    }

    auto set_build_flags = [](const std::string &flags, const std::string &ldflags) {
        if (::setenv("DEB_CFLAGS_MAINT_APPEND", flags.c_str(), 1) != 0 ||
            ::setenv("DEB_CXXFLAGS_MAINT_APPEND", flags.c_str(), 1) != 0 ||
            ::setenv("DEB_LDFLAGS_MAINT_APPEND", ldflags.c_str(), 1) != 0)
            fail("cannot set native compiler flags");
    };

    const std::vector<std::string> build_command{
        "/usr/bin/dpkg-buildpackage", "-us", "-uc", "-b"
    };

    std::string final_flags = base_flags;
    if (aggressive) {
        const std::string pgo = pgo_directory.string();
        const std::string generate_flags =
            base_flags + " -fprofile-generate=" + pgo + " -fprofile-update=atomic";
        const std::string generate_ldflags =
            base_ldflags + " -fprofile-generate=" + pgo + " -fprofile-update=atomic";
        std::cout << "Building instrumented Calculator and collecting PGO data...\n";
        set_build_flags(generate_flags, generate_ldflags);
        run_required(build_command, source_root);

        const fs::path gui = source_root / "build" / "infiltrator-calc";
        if (executable(gui.string())) {
            std::cout << "Exercising Calculator startup and GTK rendering paths...\n";
            const int result = run_process(
                { "/usr/bin/xvfb-run", "-a", "/usr/bin/timeout", "5s", gui.string() },
                source_root, true);
            if (result != 0 && result != 124)
                fail("instrumented Calculator GUI training failed");
        }
        if (!tree_has_suffix(pgo_directory, ".gcda"))
            fail("PGO training completed without producing profile data");

        final_flags = base_flags + " -fprofile-use=" + pgo +
                      " -fprofile-correction -fprofile-partial-training -Wno-missing-profile";
        const std::string use_ldflags =
            base_ldflags + " -fprofile-use=" + pgo +
            " -fprofile-correction -fprofile-partial-training -Wno-missing-profile";
        write_build_info(source_root, profile, final_flags, true);
        std::cout << "Rebuilding Calculator using the measured PGO profile...\n";
        set_build_flags(final_flags, use_ldflags);
        run_required(build_command, source_root);
    } else {
        write_build_info(source_root, profile, final_flags, false);
        std::cout << "Building Calculator for this computer...\n";
        set_build_flags(base_flags, base_ldflags);
        run_required(build_command, source_root);
    }

    if (profile != "portable") {
        if (!build_tree_contains(source_root / "build", "-march=native") ||
            !build_tree_contains(source_root / "build", "-flto=auto"))
            fail("final Calculator build did not retain native CPU/LTO flags");
    }
    if (aggressive &&
        !build_tree_contains(source_root / "build", "-fprofile-use="))
        fail("final Calculator build did not retain PGO-use flags");

    const fs::path package = find_debian_package(work, version);
    std::cout << "Installing locally built package: " << package << '\n';
    run_required(privileged({ "/usr/bin/apt-get", "-o", "DPkg::Lock::Timeout=300",
                              "install", "-y", package.string() },
                            system_package_mode));

    if (!package_installed(kPackageName) || !executable(kExecutable) ||
        !fs::exists(kBuildInfo))
        fail("Calculator package, executable or native build metadata is missing after installation");
    std::cout << "\nCalculator " << version << " is installed system-wide.\n";
    if (profile == "aggressive")
        std::cout << "The installed package is a measured two-pass PGO, LTO and CPU-native build.\n";
    else if (profile == "native")
        std::cout << "The installed package is an LTO and CPU-native build.\n";
    else
        std::cout << "The installed package uses the portable optimisation profile.\n";
    return EXIT_SUCCESS;
}
