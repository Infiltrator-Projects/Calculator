/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator.hpp"

#include <infiltratr/token.h>

#include <cctype>
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#include <system_error>

namespace calculator {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kE = 2.718281828459045235360287471352662498;
constexpr std::size_t kMaxParseDepth = 256;

enum class DecimalTokenStatus {
    None,
    Invalid,
    Ok
};

// Common owns exact locale-independent decimal token conversion. Calculator
// supplies only the expression cursor and retains all expression grammar.
DecimalTokenStatus parse_decimal_token(std::string_view input,
                                       std::size_t& position,
                                       bool allow_sign,
                                       double& value) {
    if (position >= input.size()) return DecimalTokenStatus::None;

    const char first = input[position];
    const bool plausible =
        (first >= '0' && first <= '9') || first == '.' ||
        (allow_sign && (first == '+' || first == '-'));
    if (!plausible) return DecimalTokenStatus::None;

    const char* const begin = input.data() + position;
    const char* cursor = begin;
    double parsed = 0.0;
    if (!infiltratr_parse_double_token(&cursor, allow_sign, &parsed)) {
        return DecimalTokenStatus::Invalid;
    }

    position += static_cast<std::size_t>(cursor - begin);
    value = parsed;
    return DecimalTokenStatus::Ok;
}

// Scientific/expression grammar (highest-level production first):
// expression -> term {(+|-) term}
// term       -> unary {(*|/) unary}
// unary      -> (+|-) unary | power
// power      -> postfix [^ unary]
// postfix    -> primary {%|!}
//
// Parsing the right side of '^' as unary makes exponentiation right-associative
// and gives exponentiation higher precedence than a leading sign: -2^2 is
// -(2^2), while 2^-2 remains valid.
class Parser {
public:
    Parser(std::string_view input, const Variables& variables)
        : input_(input), variables_(variables) {}

    Result run() {
        skip_space();
        if (input_.empty()) return fail("empty expression");
        const double value = parse_expression();
        skip_space();
        if (!error_.empty()) return {false, 0.0, error_};
        if (position_ != input_.size()) return fail("unexpected input");
        if (!std::isfinite(value)) return fail("non-finite result");
        return {true, value, {}};
    }

private:
    std::string_view input_;
    const Variables& variables_;
    std::size_t position_ = 0;
    std::size_t recursion_depth_ = 0;
    std::string error_;

    Result fail(const char* message) {
        if (error_.empty()) error_ = message;
        return {false, 0.0, error_};
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

    double parse_expression() {
        double left = parse_term();
        while (error_.empty()) {
            if (consume('+')) left += parse_term();
            else if (consume('-')) left -= parse_term();
            else break;
        }
        return left;
    }

    double parse_term() {
        double left = parse_unary();
        while (error_.empty()) {
            if (consume('*')) left *= parse_unary();
            else if (consume('/')) {
                const double right = parse_unary();
                if (right == 0.0) { error_ = "division by zero"; return 0.0; }
                left /= right;
            } else break;
        }
        return left;
    }

    double parse_unary() {
        if (recursion_depth_ >= kMaxParseDepth) {
            error_ = "expression nesting too deep";
            return 0.0;
        }

        ++recursion_depth_;
        double value = 0.0;
        if (consume('+')) value = parse_unary();
        else if (consume('-')) value = -parse_unary();
        else value = parse_power();
        --recursion_depth_;
        return value;
    }

    double parse_power() {
        double left = parse_postfix();
        if (error_.empty() && consume('^')) {
            const double right = parse_unary();
            left = std::pow(left, right);
            if (!std::isfinite(left)) error_ = "invalid power result";
        }
        return left;
    }

    double parse_postfix() {
        double value = parse_primary();
        while (error_.empty()) {
            if (consume('%')) value /= 100.0;
            else if (consume('!')) value = factorial(value);
            else break;
        }
        return value;
    }

    std::string parse_identifier() {
        skip_space();
        const std::size_t start = position_;
        while (position_ < input_.size() &&
               (std::isalnum(static_cast<unsigned char>(input_[position_])) || input_[position_] == '_')) ++position_;
        return std::string(input_.substr(start, position_ - start));
    }

    double factorial(double value) {
        if (value < 0.0 || std::floor(value) != value) { error_ = "factorial requires a non-negative integer"; return 0.0; }
        if (value > 170.0) { error_ = "factorial overflow"; return 0.0; }
        double result = 1.0;
        for (unsigned int i = 2; i <= static_cast<unsigned int>(value); ++i) result *= i;
        return result;
    }

    double apply_function(const std::string& name, double x) {
        RealFunction function = RealFunction::Abs;
        if (name == "sin") function = RealFunction::Sin;
        else if (name == "cos") function = RealFunction::Cos;
        else if (name == "tan") function = RealFunction::Tan;
        else if (name == "asin") function = RealFunction::Asin;
        else if (name == "acos") function = RealFunction::Acos;
        else if (name == "atan") function = RealFunction::Atan;
        else if (name == "sinh") function = RealFunction::Sinh;
        else if (name == "cosh") function = RealFunction::Cosh;
        else if (name == "tanh") function = RealFunction::Tanh;
        else if (name == "asinh") function = RealFunction::Asinh;
        else if (name == "acosh") function = RealFunction::Acosh;
        else if (name == "atanh") function = RealFunction::Atanh;
        else if (name == "sqrt") function = RealFunction::SquareRoot;
        else if (name == "cbrt") function = RealFunction::Cbrt;
        else if (name == "square") function = RealFunction::Square;
        else if (name == "cube") function = RealFunction::Cube;
        else if (name == "ln") function = RealFunction::Ln;
        else if (name == "log") function = RealFunction::Log10;
        else if (name == "exp") function = RealFunction::Exp;
        else if (name == "exp2") function = RealFunction::TwoPower;
        else if (name == "exp10") function = RealFunction::TenPower;
        else if (name == "abs") function = RealFunction::Abs;
        else if (name == "floor") function = RealFunction::Floor;
        else if (name == "ceil") function = RealFunction::Ceil;
        else { error_ = "unknown function"; return 0.0; }

        const Result result =
            apply_real_function(function, x, AngleUnit::Radians);
        if (!result.ok) {
            error_ = result.error;
            return 0.0;
        }
        return result.value;
    }

    double parse_primary() {
        skip_space();
        if (consume('(')) {
            const double value = parse_expression();
            if (!consume(')') && error_.empty()) error_ = "missing closing parenthesis";
            return value;
        }

        if (position_ < input_.size() &&
            (std::isalpha(static_cast<unsigned char>(input_[position_])) || input_[position_] == '_')) {
            const std::string name = parse_identifier();
            if (name == "pi") return kPi;
            if (name == "e") return kE;
            if (consume('(')) {
                const double argument = parse_expression();
                if (!consume(')') && error_.empty()) error_ = "missing closing parenthesis";
                if (!error_.empty()) return 0.0;
                return apply_function(name, argument);
            }
            const auto it = variables_.find(name);
            if (it == variables_.end()) { error_ = "unknown variable"; return 0.0; }
            return it->second;
        }

        double value = 0.0;
        const DecimalTokenStatus status =
            parse_decimal_token(input_, position_, false, value);
        if (status == DecimalTokenStatus::None) {
            error_ = "expected a number";
            return 0.0;
        }
        if (status == DecimalTokenStatus::Invalid) {
            error_ = "invalid number";
            return 0.0;
        }
        return value;
    }
};

// Standard mode intentionally models a desktop calculator rather than the
// Scientific grammar: binary operations are committed left-to-right as entered,
// and a percentage operand is interpreted in the context of the pending
// operator. Keeping this separate prevents mode-specific exceptions from
// contaminating the mathematical expression parser.
class ImmediateParser {
public:
    explicit ImmediateParser(std::string_view input) : input_(input) {}

    Result run() {
        skip_space();
        if (input_.empty()) return fail("empty expression");

        bool percent = false;
        double value = parse_operand(percent);
        if (!error_.empty()) return fail(error_.c_str());
        if (percent) value /= 100.0;

        while (error_.empty()) {
            skip_space();
            if (position_ == input_.size()) break;

            const char op = input_[position_];
            if (op != '+' && op != '-' && op != '*' &&
                op != '/' && op != '^') {
                return fail("unsupported immediate expression");
            }
            ++position_;

            bool right_percent = false;
            double right = parse_operand(right_percent);
            if (!error_.empty()) return fail(error_.c_str());

            if (right_percent) {
                if (op == '+' || op == '-') right = value * right / 100.0;
                else right /= 100.0;
            }

            switch (op) {
            case '+': value += right; break;
            case '-': value -= right; break;
            case '*': value *= right; break;
            case '/':
                if (right == 0.0) return fail("division by zero");
                value /= right;
                break;
            case '^':
                value = std::pow(value, right);
                break;
            default:
                break;
            }

            if (!std::isfinite(value)) return fail("non-finite result");
        }

        return {true, value, {}};
    }

private:
    std::string_view input_;
    std::size_t position_ = 0;
    std::string error_;

    Result fail(const char* message) const {
        return {false, 0.0, message ? message : "invalid expression"};
    }

    void skip_space() {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    double parse_operand(bool& percent) {
        skip_space();
        if (position_ >= input_.size()) {
            error_ = "expected a number";
            return 0.0;
        }

        double value = 0.0;
        const DecimalTokenStatus status =
            parse_decimal_token(input_, position_, true, value);
        if (status == DecimalTokenStatus::None) {
            error_ = "unsupported immediate expression";
            return 0.0;
        }
        if (status == DecimalTokenStatus::Invalid) {
            error_ = "invalid number";
            return 0.0;
        }

        skip_space();
        percent = position_ < input_.size() && input_[position_] == '%';
        if (percent) ++position_;
        return value;
    }
};

} // namespace

Result apply_real_function(RealFunction function, double value,
                           AngleUnit angle_unit) {
    const auto to_radians = [angle_unit](double input) {
        if (angle_unit == AngleUnit::Degrees) return input * kPi / 180.0;
        if (angle_unit == AngleUnit::Gradians) return input * kPi / 200.0;
        return input;
    };
    const auto from_radians = [angle_unit](double input) {
        if (angle_unit == AngleUnit::Degrees) return input * 180.0 / kPi;
        if (angle_unit == AngleUnit::Gradians) return input * 200.0 / kPi;
        return input;
    };

    double argument = value;
    if (function == RealFunction::Sin ||
        function == RealFunction::Cos ||
        function == RealFunction::Tan) {
        argument = to_radians(value);
    }

    switch (function) {
    case RealFunction::Square:
        value *= value;
        break;
    case RealFunction::Cube:
        value = value * value * value;
        break;
    case RealFunction::SquareRoot:
        value = std::sqrt(value);
        break;
    case RealFunction::Reciprocal:
        if (value == 0.0) return {false, 0.0, "division by zero"};
        value = 1.0 / value;
        break;
    case RealFunction::Sin:
        value = std::sin(argument);
        break;
    case RealFunction::Cos:
        value = std::cos(argument);
        break;
    case RealFunction::Tan:
        value = std::tan(argument);
        break;
    case RealFunction::Asin:
        value = from_radians(std::asin(value));
        break;
    case RealFunction::Acos:
        value = from_radians(std::acos(value));
        break;
    case RealFunction::Atan:
        value = from_radians(std::atan(value));
        break;
    case RealFunction::Sinh:
        value = std::sinh(value);
        break;
    case RealFunction::Cosh:
        value = std::cosh(value);
        break;
    case RealFunction::Tanh:
        value = std::tanh(value);
        break;
    case RealFunction::Asinh:
        value = std::asinh(value);
        break;
    case RealFunction::Acosh:
        value = std::acosh(value);
        break;
    case RealFunction::Atanh:
        value = std::atanh(value);
        break;
    case RealFunction::Cbrt:
        value = std::cbrt(value);
        break;
    case RealFunction::Ln:
        value = std::log(value);
        break;
    case RealFunction::Log10:
        value = std::log10(value);
        break;
    case RealFunction::Exp:
        value = std::exp(value);
        break;
    case RealFunction::TwoPower:
        value = std::exp2(value);
        break;
    case RealFunction::TenPower:
        value = std::pow(10.0, value);
        break;
    case RealFunction::Abs:
        value = std::fabs(value);
        break;
    case RealFunction::Floor:
        value = std::floor(value);
        break;
    case RealFunction::Ceil:
        value = std::ceil(value);
        break;
    }

    if (!std::isfinite(value)) {
        return {false, 0.0, "function domain error"};
    }
    return {true, value, {}};
}

std::string format_value(double value) {
    char buffer[64] = {};
    const auto converted = std::to_chars(
        buffer, buffer + sizeof(buffer),
        value, std::chars_format::general, 15);
    if (converted.ec != std::errc{}) return "0";
    return std::string(buffer, converted.ptr);
}

std::string format_scientific_value(double value) {
    char buffer[64] = {};
    const auto converted = std::to_chars(
        buffer, buffer + sizeof(buffer),
        value, std::chars_format::scientific, 12);
    if (converted.ec != std::errc{}) return "0";
    return std::string(buffer, converted.ptr);
}

std::string format_engineering_value(double value) {
    if (!std::isfinite(value)) return "0";
    if (value == 0.0) return "0e+00";

    const int exponent = static_cast<int>(
        std::floor(std::log10(std::fabs(value))));
    const int engineering_exponent =
        exponent >= 0
            ? (exponent / 3) * 3
            : -(((-exponent + 2) / 3) * 3);

    const double scale = std::pow(10.0, engineering_exponent);
    const double mantissa = value / scale;

    char mantissa_buffer[64] = {};
    const auto mantissa_converted = std::to_chars(
        mantissa_buffer, mantissa_buffer + sizeof(mantissa_buffer),
        mantissa, std::chars_format::general, 12);
    if (mantissa_converted.ec != std::errc{}) return "0";

    std::string formatted(
        mantissa_buffer, mantissa_converted.ptr);
    formatted += 'e';
    formatted += engineering_exponent < 0 ? '-' : '+';

    const unsigned absolute_exponent = static_cast<unsigned>(
        engineering_exponent < 0
            ? -engineering_exponent
            : engineering_exponent);
    if (absolute_exponent < 10U) formatted += '0';

    char exponent_buffer[16] = {};
    const auto exponent_converted = std::to_chars(
        exponent_buffer, exponent_buffer + sizeof(exponent_buffer),
        absolute_exponent);
    if (exponent_converted.ec != std::errc{}) return "0";
    formatted.append(
        exponent_buffer, exponent_converted.ptr);
    return formatted;
}

Result evaluate(const std::string& expression) {
    static const Variables empty_variables;
    return Parser(expression, empty_variables).run();
}

Result evaluate(const std::string& expression, const Variables& variables) {
    return Parser(expression, variables).run();
}

Result evaluate_immediate(const std::string& expression) {
    return ImmediateParser(expression).run();
}

} // namespace calculator
