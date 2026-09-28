// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 1993-2026 Shannon Smith
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

namespace {

constexpr const char *kPayloadMarker =
    "\n__INFILTRATOR_CALCULATOR_NATIVE_PAYLOAD_V1__\n";

[[noreturn]] void fail(const std::string &message)
{
    std::cerr << "Error: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void append_file(std::ofstream &output, const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) fail("cannot open " + path);
    char buffer[64U * 1024U];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const std::streamsize count = input.gcount();
        if (count > 0) output.write(buffer, count);
    }
    if (!input.eof()) fail("cannot read " + path);
    if (!output) fail("cannot write native installer output");
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        std::cout << "Usage: " << argv[0]
                  << " INSTALLER_BINARY SOURCE_ARCHIVE OUTPUT.run\n";
        return EXIT_SUCCESS;
    }
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " INSTALLER_BINARY SOURCE_ARCHIVE OUTPUT.run\n";
        return EXIT_FAILURE;
    }

    const std::string bootstrap = argv[1];
    const std::string archive = argv[2];
    const std::string output_path = argv[3];

    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) fail("cannot create " + output_path);
    append_file(output, bootstrap);
    output.write(kPayloadMarker, static_cast<std::streamsize>(std::strlen(kPayloadMarker)));
    if (!output) fail("cannot append native installer payload marker");
    append_file(output, archive);
    output.close();
    if (!output) fail("cannot close " + output_path);
    if (::chmod(output_path.c_str(), 0755) != 0)
        fail("cannot make output executable: " + std::string(std::strerror(errno)));

    std::cout << output_path << '\n';
    return EXIT_SUCCESS;
}
