/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "programmer.hpp"

#include <infiltratr/core.h>

#include <cctype>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

namespace calculator {
namespace {

constexpr std::size_t kMaxParseDepth = 256;

std::uint64_t mask_for(IntegerWidth width) {
    switch (width) {
    case IntegerWidth::Bits8: return 0xffULL;
    case IntegerWidth::Bits16: return 0xffffULL;
    case IntegerWidth::Bits32: return 0xffffffffULL;
    case IntegerWidth::Bits64: return std::numeric_limits<std::uint64_t>::max();
    }
    return std::numeric_limits<std::uint64_t>::max();
}

unsigned base_value(ProgrammerBase base) {
    switch (base) {
    case ProgrammerBase::Binary: return 2;
    case ProgrammerBase::Octal: return 8;
    case ProgrammerBase::Decimal: return 10;
    case ProgrammerBase::Hexadecimal: return 16;
    }
    return 10;
}

unsigned width_bits(IntegerWidth width) {
    return static_cast<unsigned>(width);
}

std::uint64_t rotate_left(std::uint64_t value, unsigned amount,
                          IntegerWidth width) {
    const unsigned bits = width_bits(width);
    const std::uint64_t mask = mask_for(width);
    value &= mask;
    amount %= bits;
    if (amount == 0U) return value;
    if (bits == 64U) {
        return (value << amount) | (value >> (64U - amount));
    }
    return ((value << amount) | (value >> (bits - amount))) & mask;
}

std::uint64_t rotate_right(std::uint64_t value, unsigned amount,
                           IntegerWidth width) {
    const unsigned bits = width_bits(width);
    const std::uint64_t mask = mask_for(width);
    value &= mask;
    amount %= bits;
    if (amount == 0U) return value;
    if (bits == 64U) {
        return (value >> amount) | (value << (64U - amount));
    }
    return ((value >> amount) | (value << (bits - amount))) & mask;
}

int digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

class Parser {
public:
    Parser(std::string_view input, ProgrammerBase base, IntegerWidth width)
        : input_(input), base_(base), width_(width) {}

    ProgrammerResult run() {
        skip_space();
        if (input_.empty()) return fail("empty expression");
        const auto value = parse_or();
        skip_space();
        if (!error_.empty()) return {false, 0, error_};
        if (position_ != input_.size()) return fail("unexpected input");
        return {true, value & mask_for(width_), {}};
    }

private:
    std::string_view input_;
    ProgrammerBase base_;
    IntegerWidth width_;
    std::size_t position_ = 0;
    std::size_t recursion_depth_ = 0;
    std::string error_;

    ProgrammerResult fail(const char* message) {
        if (error_.empty()) error_ = message;
        return {false, 0, error_};
    }

    void skip_space() {
        while (position_ < input_.size() &&
               infiltratr_ascii_is_space(static_cast<unsigned char>(input_[position_]))) ++position_;
    }

    bool consume(char c) {
        skip_space();
        if (position_ < input_.size() && input_[position_] == c) {
            ++position_;
            return true;
        }
        return false;
    }

    bool consume_pair(char a, char b) {
        skip_space();
        if (position_ + 1 < input_.size() && input_[position_] == a && input_[position_ + 1] == b) {
            position_ += 2;
            return true;
        }
        return false;
    }

    bool consume_word(std::string_view word) {
        skip_space();
        if (position_ + word.size() > input_.size()) return false;

        for (std::size_t i = 0; i < word.size(); ++i) {
            const unsigned char actual =
                static_cast<unsigned char>(input_[position_ + i]);
            const unsigned char expected =
                static_cast<unsigned char>(word[i]);
            if (infiltratr_ascii_to_lower(actual) != infiltratr_ascii_to_lower(expected)) return false;
        }

        const std::size_t end = position_ + word.size();
        if (end < input_.size()) {
            const unsigned char next =
                static_cast<unsigned char>(input_[end]);
            if (infiltratr_ascii_is_alnum(next) || next == '_') return false;
        }

        position_ = end;
        return true;
    }

    bool enter_recursion() {
        if (recursion_depth_ >= kMaxParseDepth) {
            if (error_.empty()) error_ = "expression nesting too deep";
            return false;
        }
        ++recursion_depth_;
        return true;
    }

    void leave_recursion() noexcept {
        if (recursion_depth_ > 0) --recursion_depth_;
    }

    std::uint64_t parse_or() {
        auto left = parse_xor();
        while (error_.empty()) {
            if (consume('|')) {
                left |= parse_xor();
            } else if (consume_word("nor")) {
                left = ~(left | parse_xor()) & mask_for(width_);
            } else {
                break;
            }
        }
        return left & mask_for(width_);
    }

    std::uint64_t parse_xor() {
        auto left = parse_and();
        while (error_.empty() && consume('^')) left ^= parse_and();
        return left & mask_for(width_);
    }

    std::uint64_t parse_and() {
        auto left = parse_shift();
        while (error_.empty()) {
            if (consume('&')) {
                left &= parse_shift();
            } else if (consume_word("nand")) {
                left = ~(left & parse_shift()) & mask_for(width_);
            } else {
                break;
            }
        }
        return left & mask_for(width_);
    }

    std::uint64_t parse_shift_amount() {
        // Shift counts are conventional decimal quantities, independent of
        // the currently selected input base. Evaluate them at full width so
        // a large count cannot wrap through the selected 8/16/32-bit mask.
        const auto saved_base = base_;
        const auto saved_width = width_;
        base_ = ProgrammerBase::Decimal;
        width_ = IntegerWidth::Bits64;
        const auto amount = parse_additive();
        base_ = saved_base;
        width_ = saved_width;
        return amount;
    }

    std::uint64_t parse_shift() {
        auto left = parse_additive();
        while (error_.empty()) {
            if (consume_pair('<', '<')) {
                const auto amount = parse_shift_amount();
                if (amount >= 64) { error_ = "shift count out of range"; return 0; }
                left = (left << static_cast<unsigned>(amount)) & mask_for(width_);
            } else if (consume_pair('>', '>')) {
                const auto amount = parse_shift_amount();
                if (amount >= 64) { error_ = "shift count out of range"; return 0; }
                left = left >> static_cast<unsigned>(amount);
            } else if (consume_word("rol")) {
                const auto amount = parse_shift_amount();
                left = rotate_left(
                    left, static_cast<unsigned>(amount % 64U), width_);
            } else if (consume_word("ror")) {
                const auto amount = parse_shift_amount();
                left = rotate_right(
                    left, static_cast<unsigned>(amount % 64U), width_);
            } else {
                break;
            }
        }
        return left & mask_for(width_);
    }

    std::uint64_t parse_additive() {
        auto left = parse_multiplicative();
        while (error_.empty()) {
            if (consume('+')) left = (left + parse_multiplicative()) & mask_for(width_);
            else if (consume('-')) left = (left - parse_multiplicative()) & mask_for(width_);
            else break;
        }
        return left & mask_for(width_);
    }

    std::uint64_t parse_multiplicative() {
        auto left = parse_unary();
        while (error_.empty()) {
            if (consume('*')) left = (left * parse_unary()) & mask_for(width_);
            else if (consume('/')) {
                const auto right = parse_unary();
                if (right == 0) { error_ = "division by zero"; return 0; }
                left = (left / right) & mask_for(width_);
            } else break;
        }
        return left & mask_for(width_);
    }

    std::uint64_t parse_unary() {
        if (!enter_recursion()) return 0;

        std::uint64_t value = 0;
        if (consume_word("bswap")) {
            value = swap_programmer_endianness(parse_unary(), width_);
        } else if (consume('~')) {
            value = (~parse_unary()) & mask_for(width_);
        } else if (consume('+')) {
            value = parse_unary() & mask_for(width_);
        } else if (consume('-')) {
            value = (0ULL - parse_unary()) & mask_for(width_);
        } else {
            value = parse_primary();
        }

        leave_recursion();
        return value & mask_for(width_);
    }

    std::uint64_t parse_primary() {
        skip_space();
        if (consume('(')) {
            const auto value = parse_or();
            if (!consume(')') && error_.empty()) error_ = "missing closing parenthesis";
            return value;
        }

        const auto start = position_;
        unsigned base = base_value(base_);
        if (position_ + 1 < input_.size() && input_[position_] == '0') {
            const char p = input_[position_ + 1];
            if ((p == 'x' || p == 'X') && base_ == ProgrammerBase::Hexadecimal) { base = 16; position_ += 2; }
            else if ((p == 'b' || p == 'B') && base_ == ProgrammerBase::Binary) { base = 2; position_ += 2; }
            else if ((p == 'o' || p == 'O') && base_ == ProgrammerBase::Octal) { base = 8; position_ += 2; }
        }

        bool saw_digit = false;
        std::uint64_t value = 0;
        while (position_ < input_.size()) {
            const int digit = digit_value(input_[position_]);
            if (digit < 0 || static_cast<unsigned>(digit) >= base) break;
            saw_digit = true;
            value = value * base + static_cast<unsigned>(digit);
            ++position_;
        }
        if (!saw_digit) {
            position_ = start;
            error_ = "expected integer";
            return 0;
        }
        return value & mask_for(width_);
    }
};

} // namespace

ProgrammerResult evaluate_programmer(const std::string& expression,
                                     ProgrammerBase base,
                                     IntegerWidth width) {
    return Parser(expression, base, width).run();
}

std::string format_programmer(std::uint64_t value,
                              ProgrammerBase base,
                              IntegerWidth width,
                              bool signed_display) {
    value &= mask_for(width);
    if (signed_display) {
        const unsigned bits = static_cast<unsigned>(width);
        std::int64_t signed_value = static_cast<std::int64_t>(value);
        if (bits < 64 && (value & (1ULL << (bits - 1U)))) {
            const std::uint64_t extension = ~mask_for(width);
            signed_value = static_cast<std::int64_t>(value | extension);
        }
        return std::to_string(signed_value);
    }

    std::ostringstream out;
    switch (base) {
    case ProgrammerBase::Binary: {
        const unsigned bits = static_cast<unsigned>(width);
        for (unsigned i = bits; i-- > 0;) out << ((value >> i) & 1ULL);
        break;
    }
    case ProgrammerBase::Octal: out << std::oct << value; break;
    case ProgrammerBase::Decimal: out << value; break;
    case ProgrammerBase::Hexadecimal: out << std::uppercase << std::hex << value; break;
    }
    return out.str();
}

ProgrammerRepresentations programmer_representations(
    std::uint64_t value, IntegerWidth width, bool signed_display) {
    return {
        format_programmer(
            value, ProgrammerBase::Binary, width, false),
        format_programmer(
            value, ProgrammerBase::Octal, width, false),
        format_programmer(
            value, ProgrammerBase::Decimal, width, signed_display),
        format_programmer(
            value, ProgrammerBase::Hexadecimal, width, false)
    };
}

std::uint64_t swap_programmer_endianness(
    std::uint64_t value, IntegerWidth width) noexcept {
    const unsigned bytes = static_cast<unsigned>(width) / 8U;
    value &= mask_for(width);
    std::uint64_t swapped = 0U;
    for (unsigned index = 0U; index < bytes; ++index) {
        swapped <<= 8U;
        swapped |= (value >> (index * 8U)) & 0xffU;
    }
    return swapped & mask_for(width);
}

std::string group_programmer_digits(
    std::string_view digits, ProgrammerBase base) {
    if (base == ProgrammerBase::Decimal || digits.size() <= 1U) {
        return std::string(digits);
    }

    const std::size_t group =
        base == ProgrammerBase::Octal ? 3U : 4U;
    std::string grouped;
    grouped.reserve(digits.size() + digits.size() / group);
    for (std::size_t i = 0U; i < digits.size(); ++i) {
        if (i != 0U && (digits.size() - i) % group == 0U) {
            grouped.push_back(' ');
        }
        grouped.push_back(digits[i]);
    }
    return grouped;
}

} // namespace calculator
