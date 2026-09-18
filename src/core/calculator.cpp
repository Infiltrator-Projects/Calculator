/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

namespace infiltrator::calc {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kE = 2.718281828459045235360287471352662498;

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
        if (consume('+')) return parse_unary();
        if (consume('-')) return -parse_unary();
        return parse_power();
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
        double value = 0.0;
        if (name == "sin") value = std::sin(x);
        else if (name == "cos") value = std::cos(x);
        else if (name == "tan") value = std::tan(x);
        else if (name == "asin") value = std::asin(x);
        else if (name == "acos") value = std::acos(x);
        else if (name == "atan") value = std::atan(x);
        else if (name == "sinh") value = std::sinh(x);
        else if (name == "cosh") value = std::cosh(x);
        else if (name == "tanh") value = std::tanh(x);
        else if (name == "sqrt") value = std::sqrt(x);
        else if (name == "cbrt") value = std::cbrt(x);
        else if (name == "ln") value = std::log(x);
        else if (name == "log") value = std::log10(x);
        else if (name == "exp") value = std::exp(x);
        else if (name == "abs") value = std::fabs(x);
        else { error_ = "unknown function"; return 0.0; }
        if (!std::isfinite(value)) { error_ = "function domain error"; return 0.0; }
        return value;
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

        const char* begin = input_.data() + position_;
        char* end = nullptr;
        const double value = std::strtod(begin, &end);
        if (end == begin) { error_ = "expected a number"; return 0.0; }
        position_ += static_cast<std::size_t>(end - begin);
        if (!std::isfinite(value)) error_ = "invalid number";
        return value;
    }
};

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

        const char* begin = input_.data() + position_;
        char* end = nullptr;
        const double value = std::strtod(begin, &end);
        if (end == begin) {
            error_ = "unsupported immediate expression";
            return 0.0;
        }

        position_ += static_cast<std::size_t>(end - begin);
        if (!std::isfinite(value)) {
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

} // namespace infiltrator::calc
