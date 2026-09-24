/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "advanced_tools_internal.hpp"

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/utf8.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace calculator::tools::detail {

// Financial, Unicode/number utilities and explicit complex-pair operations are independent tool domains sharing only the internal parsing/formatting adapters.
ToolResult financial_tool(std::string_view input) {
    const auto f = split_ws(input);
    if (f.empty()) return failure("Enter a financial operation.");

    auto parse3 = [&](double& a, double& b, double& c) {
        return f.size() == 4U &&
               parse_double(f[1], a) &&
               parse_double(f[2], b) &&
               parse_double(f[3], c);
    };

    double a = 0.0, b = 0.0, c = 0.0, d = 0.0;
    double result = 0.0;

    if (f[0] == "ctrm") {
        if (!parse3(a, b, c) || a <= -1.0 || a == 0.0 ||
            b <= 0.0 || c <= 0.0) {
            return failure("Usage: ctrm rate future-value present-value; rate must be > -1 and non-zero, values positive.");
        }
        result = std::log(b / c) / std::log1p(a);
    } else if (f[0] == "ddb") {
        if (!parse3(a, b, c) || a < 0.0 || b <= 0.0 ||
            c < 1.0 || std::floor(c) != c || c > b) {
            return failure("Usage: ddb cost life period; life positive and period an integer within life.");
        }
        double book = a;
        for (std::uint64_t period = 0U;
             period < static_cast<std::uint64_t>(c); ++period) {
            result = book * 2.0 / b;
            book -= result;
        }
    } else if (f[0] == "fv") {
        if (!parse3(a, b, c) || c < 0.0 || b <= -1.0) {
            return failure("Usage: fv payment rate periods; periods non-negative and rate > -1.");
        }
        result = b == 0.0 ? a * c
                          : a * std::expm1(c * std::log1p(b)) / b;
    } else if (f[0] == "gpm") {
        if (f.size() != 3U || !parse_double(f[1], a) ||
            !parse_double(f[2], b) || b >= 1.0) {
            return failure("Usage: gpm cost margin; margin must be less than 1.");
        }
        result = a / (1.0 - b);
    } else if (f[0] == "pmt") {
        if (!parse3(a, b, c) || c <= 0.0 || b <= -1.0) {
            return failure("Usage: pmt principal rate periods; periods positive and rate > -1.");
        }
        if (b == 0.0) {
            result = a / c;
        } else {
            const double discount =
                std::exp(-c * std::log1p(b));
            result = a * b / (1.0 - discount);
        }
    } else if (f[0] == "pv") {
        if (!parse3(a, b, c) || c < 0.0 || b <= -1.0) {
            return failure("Usage: pv payment rate periods; periods non-negative and rate > -1.");
        }
        if (b == 0.0) {
            result = a * c;
        } else {
            const double discount =
                std::exp(-c * std::log1p(b));
            result = a * (1.0 - discount) / b;
        }
    } else if (f[0] == "rate") {
        if (!parse3(a, b, c) || a <= 0.0 || b <= 0.0 || c <= 0.0) {
            return failure("Usage: rate future-value present-value periods; values and periods must be positive.");
        }
        result = std::expm1(std::log(a / b) / c);
    } else if (f[0] == "sln") {
        if (!parse3(a, b, c) || c <= 0.0) {
            return failure("Usage: sln cost salvage life; life must be positive.");
        }
        result = (a - b) / c;
    } else if (f[0] == "syd") {
        if (f.size() != 5U ||
            !parse_double(f[1], a) || !parse_double(f[2], b) ||
            !parse_double(f[3], c) || !parse_double(f[4], d) ||
            c <= 0.0 || d < 1.0 || d > c) {
            return failure("Usage: syd cost salvage life period; period must be within positive life.");
        }
        result = (a - b) * (c - d + 1.0) /
                 (c * (c + 1.0) / 2.0);
    } else if (f[0] == "term") {
        if (!parse3(a, b, c) || a == 0.0 || b < 0.0 || c <= -1.0) {
            return failure("Usage: term payment future-value rate; payment non-zero, future value non-negative, rate > -1.");
        }
        if (c == 0.0) {
            result = b / a;
        } else {
            const double inside = 1.0 + b * c / a;
            if (inside <= 0.0) return failure("Financial term has no real solution for those values.");
            result = std::log(inside) / std::log1p(c);
        }
    } else {
        return failure("Unknown financial operation.");
    }

    if (!std::isfinite(result)) return failure("Financial result is non-finite.");
    return success("Result  " + number(result));
}

bool parse_code_point(std::string_view text, std::uint32_t& value) {
    int base = 10;
    if (text.size() > 2U && text.substr(0, 2) == "U+") {
        text.remove_prefix(2U);
        base = 16;
    } else if (text.size() > 2U && text.substr(0, 2) == "0x") {
        text.remove_prefix(2U);
        base = 16;
    }
    if (text.empty()) return false;
    std::uint64_t parsed = 0U;
    if (!parse_u64(text, parsed, static_cast<unsigned int>(base)) ||
        parsed > 0x10ffffU ||
        (parsed >= 0xd800U && parsed <= 0xdfffU)) {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool decode_single_utf8(std::string_view text, std::uint32_t& code) {
    if (text.empty()) return false;
    const auto byte = [&](std::size_t index) {
        return static_cast<unsigned char>(text[index]);
    };
    const unsigned char first = byte(0);
    std::size_t length = 0U;
    std::uint32_t value = 0U;
    if (first < 0x80U) {
        length = 1U;
        value = first;
    } else if ((first & 0xe0U) == 0xc0U) {
        length = 2U;
        value = first & 0x1fU;
    } else if ((first & 0xf0U) == 0xe0U) {
        length = 3U;
        value = first & 0x0fU;
    } else if ((first & 0xf8U) == 0xf0U) {
        length = 4U;
        value = first & 0x07U;
    } else {
        return false;
    }
    if (text.size() != length) return false;
    for (std::size_t i = 1U; i < length; ++i) {
        const unsigned char continuation = byte(i);
        if ((continuation & 0xc0U) != 0x80U) return false;
        value = (value << 6U) | (continuation & 0x3fU);
    }
    if ((length == 2U && value < 0x80U) ||
        (length == 3U && value < 0x800U) ||
        (length == 4U && value < 0x10000U) ||
        value > 0x10ffffU ||
        (value >= 0xd800U && value <= 0xdfffU)) {
        return false;
    }
    code = value;
    return true;
}


bool checked_permutation(std::uint64_t n, std::uint64_t r,
                         std::uint64_t& result) {
    if (r > n) return false;
    result = 1U;
    for (std::uint64_t i = 0U; i < r; ++i) {
        const std::uint64_t factor = n - i;
        std::uint64_t multiplied = 0U;
        if (!infiltratr_u64_multiply_checked(result, factor, &multiplied)) {
            return false;
        }
        result = multiplied;
    }
    return true;
}

bool checked_combination(std::uint64_t n, std::uint64_t r,
                         std::uint64_t& result) {
    if (r > n) return false;
    r = std::min(r, n - r);
    result = 1U;
    for (std::uint64_t i = 1U; i <= r; ++i) {
        std::uint64_t numerator = n - r + i;
        std::uint64_t denominator = i;
        const std::uint64_t first =
            std::gcd(numerator, denominator);
        numerator /= first;
        denominator /= first;
        const std::uint64_t second =
            std::gcd(result, denominator);
        result /= second;
        denominator /= second;
        std::uint64_t multiplied = 0U;
        if (!infiltratr_u64_multiply_checked(
                result, numerator, &multiplied)) {
            return false;
        }
        result = multiplied;
        if (denominator != 1U) result /= denominator;
    }
    return true;
}

ToolResult number_utilities_tool(std::string_view input) {
    const auto f = split_ws(input);
    if (f.empty()) return failure("Enter a number utility operation.");

    if (f[0] == "mod") {
        double a = 0.0, b = 0.0;
        if (f.size() != 3U || !parse_double(f[1], a) ||
            !parse_double(f[2], b) || b == 0.0) {
            return failure("Usage: mod a b; divisor must be non-zero.");
        }
        return success("Remainder  " + number(std::fmod(a, b)));
    }

    if (f[0] == "factor") {
        std::uint64_t value = 0U;
        if (f.size() != 2U ||
            !parse_u64_range(
                f[1], 2U, 1000000000000ULL, value)) {
            return failure("Usage: factor integer from 2 through 1000000000000.");
        }
        std::uint64_t remaining = value;
        std::vector<std::uint64_t> factors;
        while ((remaining % 2U) == 0U) {
            factors.push_back(2U);
            remaining /= 2U;
        }
        for (std::uint64_t divisor = 3U;
             divisor <= remaining / divisor;
             divisor += 2U) {
            while ((remaining % divisor) == 0U) {
                factors.push_back(divisor);
                remaining /= divisor;
            }
        }
        if (remaining > 1U) factors.push_back(remaining);
        std::ostringstream out;
        out << "Prime factors  ";
        for (std::size_t i = 0; i < factors.size(); ++i) {
            if (i != 0U) out << " x ";
            out << factors[i];
        }
        return success(out.str());
    }

    if (f[0] == "gcd" || f[0] == "lcm") {
        std::uint64_t a = 0U, b = 0U;
        if (f.size() != 3U || !parse_u64(f[1], a) ||
            !parse_u64(f[2], b)) {
            return failure("Usage: gcd|lcm non-negative-integer non-negative-integer.");
        }
        const std::uint64_t divisor = std::gcd(a, b);
        if (f[0] == "gcd") {
            return success("GCD  " + std::to_string(divisor));
        }
        if (a == 0U || b == 0U) return success("LCM  0");
        const std::uint64_t reduced = a / divisor;
        std::uint64_t lcm = 0U;
        if (!infiltratr_u64_multiply_checked(reduced, b, &lcm)) {
            return failure("LCM exceeds the 64-bit utility domain.");
        }
        return success("LCM  " + std::to_string(lcm));
    }

    if (f[0] == "perm" || f[0] == "comb") {
        std::uint64_t n = 0U, r = 0U, result = 0U;
        if (f.size() != 3U || !parse_u64(f[1], n) ||
            !parse_u64(f[2], r) || r > n) {
            return failure("Usage: perm|comb n r with 0 <= r <= n.");
        }
        const bool ok = f[0] == "perm"
            ? checked_permutation(n, r, result)
            : checked_combination(n, r, result);
        if (!ok) return failure("Combinatorial result exceeds the 64-bit utility domain.");
        return success(
            std::string(f[0] == "perm" ? "Permutations  " : "Combinations  ") +
            std::to_string(result));
    }

    if (f[0] == "root") {
        std::uint64_t degree = 0U;
        if (f.size() != 3U || !parse_u64(f[1], degree) || degree == 0U) {
            return failure("Usage: root positive-integer-degree value.");
        }
        const ScientificResult source = calculator::evaluate_scientific(
            f[2], {}, {}, AngleUnit::Radians, kToolScientificDigits);
        if (!source.ok || !source.display.empty() || source.value.imag != "0") {
            return failure("Usage: root positive-integer-degree real-value.");
        }
        if ((degree % 2U) == 0U && !source.value.real.empty() &&
            source.value.real.front() == '-') {
            return failure("Even root of a negative value has no real result.");
        }

        const std::string expression =
            "root" + std::to_string(degree) + "(" +
            calculator::scientific_value_expression(source.value) + ")";
        const ScientificResult result = calculator::evaluate_scientific(
            expression, {}, {}, AngleUnit::Radians, kToolScientificDigits);
        if (!result.ok || !result.display.empty() || result.value.imag != "0") {
            return failure(
                result.error.empty()
                    ? "Root result is outside the real tool domain."
                    : result.error);
        }
        return success(
            "Root  " +
            calculator::format_scientific_value(result.value, 25U));
    }

    if (f[0] == "char") {
        if (f.size() != 2U) return failure("Usage: char single-UTF-8-character.");
        std::uint32_t code = 0U;
        if (!decode_single_utf8(f[1], code)) {
            return failure("Character input must be exactly one valid Unicode scalar.");
        }
        std::ostringstream out;
        out << "Character  " << f[1] << "\nCode point  U+"
            << std::uppercase << std::hex << std::setfill('0')
            << std::setw(code <= 0xffffU ? 4 : 6) << code
            << "\nDecimal  " << std::dec << code;
        return success(out.str());
    }

    if (f[0] == "code") {
        if (f.size() != 2U) return failure("Usage: code U+NNNN | 0xNNNN | decimal.");
        std::uint32_t code = 0U;
        if (!parse_code_point(f[1], code)) {
            return failure("Code point must be a valid Unicode scalar.");
        }
        char encoded[4];
        std::size_t encoded_length = 0U;
        if (!infiltratr_utf8_encode_codepoint(
                code, encoded, sizeof(encoded), &encoded_length)) {
            return failure("Code point must be a valid Unicode scalar.");
        }
        std::ostringstream out;
        out << "Character  " << std::string(encoded, encoded_length)
            << "\nCode point  U+"
            << std::uppercase << std::hex << std::setfill('0')
            << std::setw(code <= 0xffffU ? 4 : 6) << code
            << "\nDecimal  " << std::dec << code;
        return success(out.str());
    }

    if (f[0] == "ones" || f[0] == "twos") {
        std::uint64_t value = 0U, bits = 0U;
        if (f.size() != 3U || !parse_u64(f[1], value) ||
            !parse_u64(f[2], bits) ||
            (bits != 8U && bits != 16U && bits != 32U && bits != 64U)) {
            return failure("Usage: ones|twos value bits where bits is 8, 16, 32 or 64.");
        }
        const std::uint64_t mask =
            bits == 64U ? std::numeric_limits<std::uint64_t>::max()
                        : ((std::uint64_t{1} << bits) - 1U);
        const std::uint64_t narrowed = value & mask;
        const std::uint64_t result = f[0] == "ones"
            ? (~narrowed & mask)
            : ((~narrowed + 1U) & mask);
        std::ostringstream out;
        out << (f[0] == "ones" ? "One's complement  " : "Two's complement  ")
            << result << "\nHex  0x" << std::uppercase << std::hex << result;
        return success(out.str());
    }

    return failure("Unknown number utility operation.");
}

bool parse_complex_pair_scientific(
    std::string_view text, ScientificValue& value) {
    const std::size_t comma = text.find(',');
    if (comma == std::string_view::npos ||
        text.find(',', comma + 1U) != std::string_view::npos) {
        return false;
    }

    const std::string real_text = trim(text.substr(0U, comma));
    const std::string imag_text = trim(text.substr(comma + 1U));
    if (real_text.empty() || imag_text.empty()) return false;

    const ScientificResult real = calculator::evaluate_scientific(
        real_text, {}, {}, AngleUnit::Radians, kToolScientificDigits);
    const ScientificResult imag = calculator::evaluate_scientific(
        imag_text, {}, {}, AngleUnit::Radians, kToolScientificDigits);
    if (!real.ok || !imag.ok ||
        !real.display.empty() || !imag.display.empty() ||
        real.value.imag != "0" || imag.value.imag != "0") {
        return false;
    }

    value.real = real.value.real;
    value.imag = imag.value.real;
    return true;
}

ScientificResult evaluate_tool_complex(std::string expression) {
    return calculator::evaluate_scientific(
        expression, {}, {}, AngleUnit::Radians, kToolScientificDigits);
}

std::string tool_complex_text(const ScientificValue& value) {
    return calculator::format_scientific_value(value, 25U);
}

bool scientific_zero(const ScientificValue& value) {
    return value.real == "0" && value.imag == "0";
}

ToolResult complex_tool(std::string_view input) {
    const auto f = split_ws(input);
    if (f.empty()) return failure("Enter a complex operation.");

    ScientificValue left;
    ScientificValue right;
    if (f[0] == "conj" || f[0] == "abs" ||
        f[0] == "arg" || f[0] == "polar") {
        if (f.size() != 2U || !parse_complex_pair_scientific(f[1], left)) {
            return failure("Usage: conj|abs|arg|polar real,imag");
        }

        const std::string operand =
            calculator::scientific_value_expression(left);
        if (f[0] == "conj") {
            const ScientificResult result =
                evaluate_tool_complex("conj(" + operand + ")");
            if (!result.ok || !result.display.empty()) {
                return failure(
                    result.error.empty()
                        ? "Complex conjugate failed."
                        : result.error);
            }
            return success(tool_complex_text(result.value));
        }

        const ScientificResult magnitude =
            evaluate_tool_complex("abs(" + operand + ")");
        if (!magnitude.ok || !magnitude.display.empty()) {
            return failure(
                magnitude.error.empty()
                    ? "Complex magnitude failed."
                    : magnitude.error);
        }

        ScientificValue phase{};
        if (!scientific_zero(left)) {
            const ScientificResult logarithm =
                evaluate_tool_complex("ln(" + operand + ")");
            if (!logarithm.ok || !logarithm.display.empty()) {
                return failure(
                    logarithm.error.empty()
                        ? "Complex argument failed."
                        : logarithm.error);
            }
            phase.real = logarithm.value.imag;
            phase.imag = "0";
        }

        if (f[0] == "abs") {
            return success("Magnitude  " + tool_complex_text(magnitude.value));
        }
        if (f[0] == "arg") {
            return success("Argument  " + tool_complex_text(phase) + " rad");
        }
        return success(
            "Magnitude  " + tool_complex_text(magnitude.value) +
            "\nPhase  " + tool_complex_text(phase) + " rad");
    }

    if (f.size() != 3U ||
        !parse_complex_pair_scientific(f[1], left) ||
        !parse_complex_pair_scientific(f[2], right)) {
        return failure("Usage: add|sub|mul|div real,imag real,imag");
    }
    if (f[0] == "div" && scientific_zero(right)) {
        return failure("Complex division by zero.");
    }

    const char* op = nullptr;
    if (f[0] == "add") op = "+";
    else if (f[0] == "sub") op = "-";
    else if (f[0] == "mul") op = "*";
    else if (f[0] == "div") op = "/";
    else return failure("Unknown complex operation.");

    const std::string left_expression =
        calculator::scientific_value_expression(left);
    const std::string right_expression =
        calculator::scientific_value_expression(right);
    const ScientificResult result = evaluate_tool_complex(
        "(" + left_expression + ")" + op +
        "(" + right_expression + ")");
    if (!result.ok || !result.display.empty()) {
        return failure(
            result.error.empty()
                ? "Complex calculation failed."
                : result.error);
    }

    const std::string result_expression =
        calculator::scientific_value_expression(result.value);
    const ScientificResult magnitude =
        evaluate_tool_complex("abs(" + result_expression + ")");
    if (!magnitude.ok || !magnitude.display.empty()) {
        return failure("Complex magnitude failed.");
    }

    ScientificValue phase{};
    if (!scientific_zero(result.value)) {
        const ScientificResult logarithm =
            evaluate_tool_complex("ln(" + result_expression + ")");
        if (!logarithm.ok || !logarithm.display.empty()) {
            return failure("Complex argument failed.");
        }
        phase.real = logarithm.value.imag;
        phase.imag = "0";
    }

    return success(
        tool_complex_text(result.value) +
        "\nMagnitude  " + tool_complex_text(magnitude.value) +
        "\nPhase  " + tool_complex_text(phase) + " rad");
}

} // namespace calculator::tools::detail
