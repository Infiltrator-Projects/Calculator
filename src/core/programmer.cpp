/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "programmer.hpp"

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
               std::isspace(static_cast<unsigned char>(input_[position_]))) ++position_;
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
        while (error_.empty() && consume('|')) left |= parse_xor();
        return left & mask_for(width_);
    }

    std::uint64_t parse_xor() {
        auto left = parse_and();
        while (error_.empty() && consume('^')) left ^= parse_and();
        return left & mask_for(width_);
    }

    std::uint64_t parse_and() {
        auto left = parse_shift();
        while (error_.empty() && consume('&')) left &= parse_shift();
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
            } else break;
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
        if (consume('~')) value = (~parse_unary()) & mask_for(width_);
        else if (consume('+')) value = parse_unary() & mask_for(width_);
        else if (consume('-')) value = (0ULL - parse_unary()) & mask_for(width_);
        else value = parse_primary();

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

} // namespace calculator
