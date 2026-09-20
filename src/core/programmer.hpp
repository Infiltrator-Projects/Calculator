/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <string>

namespace calculator {

enum class ProgrammerBase { Binary, Octal, Decimal, Hexadecimal };

enum class IntegerWidth { Bits8 = 8, Bits16 = 16, Bits32 = 32, Bits64 = 64 };

struct ProgrammerResult {
    bool ok = false;
    std::uint64_t value = 0;
    std::string error;
};

struct ProgrammerRepresentations {
    std::string binary;
    std::string octal;
    std::string decimal;
    std::string hexadecimal;
};

// Evaluate a fixed-width bit-pattern expression. Arithmetic intentionally
// wraps by masking to the selected width; overflow is part of Programmer-mode
// semantics, not an error condition. Numeric literals use the selected radix.
ProgrammerResult evaluate_programmer(const std::string& expression,
                                     ProgrammerBase base,
                                     IntegerWidth width);

// signed_display changes interpretation of the final masked bit pattern for
// presentation only. It does not alter evaluation or the stored uint64_t value.
std::string format_programmer(std::uint64_t value,
                              ProgrammerBase base,
                              IntegerWidth width,
                              bool signed_display);

// Render the same masked bit pattern in all four common Programmer radices.
// signed_display affects decimal interpretation only; non-decimal forms remain
// exact unsigned bit-pattern representations.
ProgrammerRepresentations programmer_representations(
    std::uint64_t value, IntegerWidth width, bool signed_display);

} // namespace calculator
