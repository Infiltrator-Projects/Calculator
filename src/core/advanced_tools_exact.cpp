/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "advanced_tools_internal.hpp"

#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace calculator::tools::detail {
namespace {

using BigInt = boost::multiprecision::cpp_int;
constexpr std::size_t kMaxBigIntDigits = 20000U;

BigInt absolute(BigInt value) {
    if (value < 0) value = -value;
    return value;
}

std::size_t decimal_digits(const BigInt& value) {
    const std::string text = value.str();
    return !text.empty() && text.front() == '-'
        ? text.size() - 1U
        : text.size();
}

bool check_bigint(const BigInt& value, EvaluationBudget& budget) {
    return budget.check_output_digits(decimal_digits(value));
}

bool charge_or_error(EvaluationBudget& budget, std::string& error,
                     std::uint64_t count = 1U) {
    if (budget.charge(count)) return true;
    if (error.empty()) error = budget.error();
    return false;
}

bool parse_bigint_digits(std::string_view digits, BigInt& value,
                         EvaluationBudget& budget, std::string& error) {
    if (digits.empty() || digits.size() > kMaxBigIntDigits) return false;
    value = 0;
    for (const char character : digits) {
        if (character < '0' || character > '9') return false;
        if (!charge_or_error(budget, error)) return false;
        value *= 10;
        value += static_cast<unsigned>(character - '0');
    }
    return check_bigint(value, budget);
}

BigInt gcd_bigint(BigInt left, BigInt right, EvaluationBudget& budget,
                  std::string& error) {
    left = absolute(std::move(left));
    right = absolute(std::move(right));
    while (right != 0) {
        if (!charge_or_error(budget, error)) return 0;
        BigInt remainder = left % right;
        left = std::move(right);
        right = std::move(remainder);
    }
    return left;
}

struct Exact {
    BigInt numerator = 0;
    BigInt denominator = 1;
};

bool normalize(Exact& value, EvaluationBudget& budget, std::string& error) {
    if (value.denominator == 0) {
        error = "Division by zero.";
        return false;
    }
    if (value.numerator == 0) {
        value.denominator = 1;
        return true;
    }
    if (value.denominator < 0) {
        value.denominator = -value.denominator;
        value.numerator = -value.numerator;
    }

    const BigInt divisor = gcd_bigint(
        value.numerator, value.denominator, budget, error);
    if (!error.empty()) return false;
    if (divisor != 0 && divisor != 1) {
        value.numerator /= divisor;
        value.denominator /= divisor;
    }
    if (!check_bigint(value.numerator, budget) ||
        !check_bigint(value.denominator, budget)) {
        if (error.empty()) error = budget.error();
        return false;
    }
    return true;
}

bool multiply_checked(const BigInt& left, const BigInt& right,
                      BigInt& result, EvaluationBudget& budget,
                      std::string& error) {
    if (!charge_or_error(budget, error)) return false;
    result = left * right;
    if (!check_bigint(result, budget)) {
        if (error.empty()) error = budget.error();
        return false;
    }
    return true;
}

bool power_checked(BigInt base, std::uint64_t exponent, BigInt& result,
                   EvaluationBudget& budget, std::string& error) {
    if (!budget.check_power_exponent(exponent)) {
        error = budget.error();
        return false;
    }

    result = 1;
    std::uint64_t remaining = exponent;
    while (remaining != 0U) {
        if (!charge_or_error(budget, error)) return false;
        if ((remaining & 1U) != 0U) {
            if (!multiply_checked(result, base, result, budget, error)) {
                return false;
            }
        }
        remaining >>= 1U;
        if (remaining != 0U) {
            if (!multiply_checked(base, base, base, budget, error)) {
                return false;
            }
        }
    }
    return true;
}

bool power10(std::size_t exponent, BigInt& result,
             EvaluationBudget& budget, std::string& error) {
    if (exponent > 4096U) {
        error = "Decimal scale exceeds safety limit.";
        return false;
    }
    return power_checked(
        BigInt(10), static_cast<std::uint64_t>(exponent),
        result, budget, error);
}

bool exact_add(const Exact& left, const Exact& right, bool subtract,
               Exact& result, EvaluationBudget& budget, std::string& error) {
    BigInt left_scaled;
    BigInt right_scaled;
    BigInt denominator;
    if (!multiply_checked(
            left.numerator, right.denominator,
            left_scaled, budget, error) ||
        !multiply_checked(
            right.numerator, left.denominator,
            right_scaled, budget, error) ||
        !multiply_checked(
            left.denominator, right.denominator,
            denominator, budget, error)) {
        return false;
    }
    result.numerator = subtract
        ? left_scaled - right_scaled
        : left_scaled + right_scaled;
    result.denominator = std::move(denominator);
    if (!check_bigint(result.numerator, budget)) {
        error = budget.error();
        return false;
    }
    return normalize(result, budget, error);
}

bool exact_multiply(const Exact& left, const Exact& right,
                    Exact& result, EvaluationBudget& budget,
                    std::string& error) {
    if (!multiply_checked(
            left.numerator, right.numerator,
            result.numerator, budget, error) ||
        !multiply_checked(
            left.denominator, right.denominator,
            result.denominator, budget, error)) {
        return false;
    }
    return normalize(result, budget, error);
}

bool exact_divide(const Exact& left, const Exact& right,
                  Exact& result, EvaluationBudget& budget,
                  std::string& error) {
    if (right.numerator == 0) {
        error = "Division by zero.";
        return false;
    }
    BigInt divisor = right.numerator;
    if (divisor < 0) divisor = -divisor;
    if (!multiply_checked(
            left.numerator, right.denominator,
            result.numerator, budget, error) ||
        !multiply_checked(
            left.denominator, divisor,
            result.denominator, budget, error)) {
        return false;
    }
    if (right.numerator < 0) result.numerator = -result.numerator;
    return normalize(result, budget, error);
}

bool terminating_decimal(const Exact& value, std::string& decimal,
                         EvaluationBudget& budget, std::string& error) {
    BigInt denominator = value.denominator;
    std::size_t twos = 0U;
    std::size_t fives = 0U;

    while ((denominator % 2) == 0) {
        if (!charge_or_error(budget, error)) return false;
        denominator /= 2;
        ++twos;
    }
    while ((denominator % 5) == 0) {
        if (!charge_or_error(budget, error)) return false;
        denominator /= 5;
        ++fives;
    }
    if (denominator != 1) return true;

    const std::size_t scale = std::max(twos, fives);
    if (scale > kMaxBigIntDigits) return true;

    BigInt scaled = absolute(value.numerator);
    BigInt factor;
    if (twos < scale) {
        if (!power_checked(
                BigInt(2), static_cast<std::uint64_t>(scale - twos),
                factor, budget, error) ||
            !multiply_checked(scaled, factor, scaled, budget, error)) {
            return false;
        }
    }
    if (fives < scale) {
        if (!power_checked(
                BigInt(5), static_cast<std::uint64_t>(scale - fives),
                factor, budget, error) ||
            !multiply_checked(scaled, factor, scaled, budget, error)) {
            return false;
        }
    }

    std::string digits = scaled.str();
    if (scale != 0U) {
        if (digits.size() <= scale) {
            digits.insert(0, scale + 1U - digits.size(), '0');
        }
        digits.insert(digits.size() - scale, 1U, '.');
        while (digits.size() > 1U && digits.back() == '0') {
            digits.pop_back();
        }
        if (!digits.empty() && digits.back() == '.') digits.pop_back();
    }
    if (value.numerator < 0 && digits != "0") {
        digits.insert(digits.begin(), '-');
    }
    decimal = std::move(digits);
    return true;
}

class ExactParser {
public:
    ExactParser(std::string_view input, EvaluationBudget& budget)
        : input_(input), budget_(budget) {}

    ToolResult run() {
        if (!budget_.check_input(input_.size())) {
            return failure(budget_.error());
        }
        Exact value = expression();
        skip();
        if (!error_.empty()) return failure(error_);
        if (position_ != input_.size()) {
            return failure("Unexpected input in exact-decimal expression.");
        }
        if (!normalize(value, budget_, error_)) return failure(error_);

        std::string output =
            "Exact fraction  " + value.numerator.str() + "/" +
            value.denominator.str();
        std::string decimal;
        if (!terminating_decimal(value, decimal, budget_, error_)) {
            return failure(error_);
        }
        if (!decimal.empty()) output += "\nExact decimal  " + decimal;
        return success(std::move(output));
    }

private:
    std::string_view input_;
    EvaluationBudget& budget_;
    std::size_t position_ = 0U;
    std::string error_;

    void skip() {
        while (position_ < input_.size() &&
               infiltratr_ascii_is_space(
                   static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    bool take(char character) {
        skip();
        if (position_ < input_.size() && input_[position_] == character) {
            ++position_;
            return true;
        }
        return false;
    }

    Exact expression() {
        Exact left = term();
        while (error_.empty()) {
            if (take('+') || take('-')) {
                const char operation = input_[position_ - 1U];
                Exact right = term();
                Exact combined;
                if (!exact_add(
                        left, right, operation == '-', combined,
                        budget_, error_)) {
                    return {};
                }
                left = std::move(combined);
            } else {
                break;
            }
        }
        return left;
    }

    Exact term() {
        Exact left = unary();
        while (error_.empty()) {
            if (take('*')) {
                Exact right = unary();
                Exact combined;
                if (!exact_multiply(
                        left, right, combined, budget_, error_)) {
                    return {};
                }
                left = std::move(combined);
            } else if (take('/')) {
                Exact right = unary();
                Exact combined;
                if (!exact_divide(
                        left, right, combined, budget_, error_)) {
                    return {};
                }
                left = std::move(combined);
            } else {
                break;
            }
        }
        return left;
    }

    Exact unary() {
        if (take('+')) return unary();
        if (take('-')) {
            Exact value = unary();
            value.numerator = -value.numerator;
            return value;
        }
        return primary();
    }

    Exact primary() {
        if (take('(')) {
            Exact value = expression();
            if (!take(')') && error_.empty()) {
                error_ = "Missing closing parenthesis.";
            }
            return value;
        }

        skip();
        const std::size_t start = position_;
        bool decimal_point = false;
        bool any_digit = false;
        std::size_t fractional_digits = 0U;
        std::string digits;

        while (position_ < input_.size()) {
            const char character = input_[position_];
            if (character >= '0' && character <= '9') {
                any_digit = true;
                digits.push_back(character);
                if (decimal_point) ++fractional_digits;
                ++position_;
            } else if (character == '.' && !decimal_point) {
                decimal_point = true;
                ++position_;
            } else {
                break;
            }
        }

        if (!any_digit || position_ == start) {
            error_ = "Expected a decimal literal.";
            return {};
        }
        if (digits.empty()) {
            error_ = "Invalid decimal literal.";
            return {};
        }

        BigInt numerator;
        if (!parse_bigint_digits(digits, numerator, budget_, error_)) {
            if (error_.empty()) error_ = "Invalid or oversized decimal literal.";
            return {};
        }

        int exponent = 0;
        if (position_ < input_.size() &&
            (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            bool negative = false;
            if (position_ < input_.size() &&
                (input_[position_] == '+' || input_[position_] == '-')) {
                negative = input_[position_] == '-';
                ++position_;
            }
            const std::size_t exponent_start = position_;
            while (position_ < input_.size() &&
                   infiltratr_ascii_is_digit(
                       static_cast<unsigned char>(input_[position_]))) {
                ++position_;
            }
            if (exponent_start == position_) {
                error_ = "Malformed decimal exponent.";
                return {};
            }
            std::uint64_t parsed = 0U;
            if (!parse_u64_range(
                    input_.substr(
                        exponent_start, position_ - exponent_start),
                    0U, 4096U, parsed)) {
                error_ = "Decimal exponent exceeds safety limit.";
                return {};
            }
            exponent = static_cast<int>(parsed);
            if (negative) exponent = -exponent;
        }

        const long long scale =
            static_cast<long long>(fractional_digits) - exponent;
        Exact value;
        if (scale >= 0) {
            value.numerator = std::move(numerator);
            if (!power10(
                    static_cast<std::size_t>(scale),
                    value.denominator, budget_, error_)) {
                return {};
            }
        } else {
            BigInt multiplier;
            if (!power10(
                    static_cast<std::size_t>(-scale),
                    multiplier, budget_, error_) ||
                !multiply_checked(
                    numerator, multiplier,
                    value.numerator, budget_, error_)) {
                return {};
            }
            value.denominator = 1;
        }
        if (!normalize(value, budget_, error_)) return {};
        return value;
    }
};

class IntegerParser {
public:
    IntegerParser(std::string_view input, EvaluationBudget& budget)
        : input_(input), budget_(budget) {}

    ToolResult run() {
        if (!budget_.check_input(input_.size())) {
            return failure(budget_.error());
        }
        BigInt value = expression();
        skip();
        if (!error_.empty()) return failure(error_);
        if (position_ != input_.size()) {
            return failure("Unexpected arbitrary-precision input.");
        }
        if (!check_bigint(value, budget_)) {
            return failure("Result exceeds the 20,000-digit safety limit.");
        }
        return success(value.str());
    }

private:
    std::string_view input_;
    EvaluationBudget& budget_;
    std::size_t position_ = 0U;
    std::string error_;

    void skip() {
        while (position_ < input_.size() &&
               infiltratr_ascii_is_space(
                   static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    bool take(char character) {
        skip();
        if (position_ < input_.size() && input_[position_] == character) {
            ++position_;
            return true;
        }
        return false;
    }

    BigInt expression() {
        BigInt left = term();
        while (error_.empty()) {
            if (take('+')) {
                if (!charge_or_error(budget_, error_)) return 0;
                left += term();
            } else if (take('-')) {
                if (!charge_or_error(budget_, error_)) return 0;
                left -= term();
            } else {
                break;
            }
            if (!check_bigint(left, budget_)) {
                error_ = "Result exceeds the 20,000-digit safety limit.";
                return 0;
            }
        }
        return left;
    }

    BigInt term() {
        BigInt left = unary();
        while (error_.empty() && take('*')) {
            BigInt right = unary();
            BigInt product;
            if (!multiply_checked(
                    left, right, product, budget_, error_)) {
                if (error_ == budget_.error() &&
                    error_ == "result exceeds evaluation output budget") {
                    error_ = "Result exceeds the 20,000-digit safety limit.";
                }
                return 0;
            }
            left = std::move(product);
        }
        return left;
    }

    BigInt unary() {
        if (take('+')) return unary();
        if (take('-')) return -unary();
        return power();
    }

    BigInt power() {
        BigInt base = postfix();
        if (!error_.empty() || !take('^')) return base;

        BigInt exponent_value = unary();
        if (!error_.empty()) return 0;
        if (exponent_value < 0 || exponent_value > 100000) {
            error_ = "Exponent must be a non-negative integer <= 100000.";
            return 0;
        }
        const std::uint64_t exponent =
            exponent_value.convert_to<std::uint64_t>();
        BigInt result;
        if (!power_checked(base, exponent, result, budget_, error_)) {
            if (error_ == "power exponent exceeds evaluation budget") {
                error_ = "Exponent must be a non-negative integer <= 100000.";
            } else if (error_ == "result exceeds evaluation output budget") {
                error_ = "Result exceeds the 20,000-digit safety limit.";
            }
            return 0;
        }
        return result;
    }

    BigInt postfix() {
        BigInt value = primary();
        while (error_.empty() && take('!')) {
            if (value < 0 || value > 1000) {
                error_ = "Factorial requires an integer from 0 to 1000.";
                return 0;
            }
            const std::uint64_t count = value.convert_to<std::uint64_t>();
            if (!budget_.check_factorial(count) ||
                !budget_.charge(count > 1U ? count - 1U : 1U)) {
                error_ = budget_.error();
                return 0;
            }
            BigInt factorial = 1;
            for (std::uint64_t index = 2U; index <= count; ++index) {
                factorial *= index;
                if (!check_bigint(factorial, budget_)) {
                    error_ = "Result exceeds the 20,000-digit safety limit.";
                    return 0;
                }
            }
            value = std::move(factorial);
        }
        return value;
    }

    BigInt primary() {
        if (take('(')) {
            BigInt value = expression();
            if (!take(')') && error_.empty()) {
                error_ = "Missing closing parenthesis.";
            }
            return value;
        }

        skip();
        const std::size_t start = position_;
        while (position_ < input_.size() &&
               infiltratr_ascii_is_digit(
                   static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
        if (start == position_) {
            error_ = "Expected an integer.";
            return 0;
        }

        BigInt value;
        if (!parse_bigint_digits(
                input_.substr(start, position_ - start),
                value, budget_, error_)) {
            if (error_.empty()) error_ = "Invalid or oversized integer.";
            return 0;
        }
        return value;
    }
};

} // namespace

ToolResult exact_decimal_tool(std::string_view input) {
    EvaluationBudget budget = make_tool_evaluation_budget();
    return ExactParser(input, budget).run();
}

ToolResult arbitrary_precision_tool(std::string_view input) {
    EvaluationBudget budget = make_tool_evaluation_budget();
    return IntegerParser(input, budget).run();
}

} // namespace calculator::tools::detail
