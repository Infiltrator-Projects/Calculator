/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace infiltrator::calc {
namespace {

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    Result run() {
        skip_space();
        if (input_.empty()) return fail("empty expression");
        double value = parse_expression();
        skip_space();
        if (!error_.empty()) return {false, 0.0, error_};
        if (position_ != input_.size()) return fail("unexpected input");
        if (!std::isfinite(value)) return fail("non-finite result");
        return {true, value, {}};
    }

private:
    std::string_view input_;
    std::size_t position_ = 0;
    std::string error_;

    Result fail(const char* message) {
        if (error_.empty()) error_ = message;
        return {false, 0.0, error_};
    }

    void skip_space() {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    bool consume(char expected) {
        skip_space();
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    double parse_expression() {
        double left = parse_term();
        while (error_.empty()) {
            if (consume('+')) {
                left += parse_term();
            } else if (consume('-')) {
                left -= parse_term();
            } else {
                break;
            }
        }
        return left;
    }

    double parse_term() {
        double left = parse_power();
        while (error_.empty()) {
            if (consume('*')) {
                left *= parse_power();
            } else if (consume('/')) {
                double right = parse_power();
                if (right == 0.0) {
                    error_ = "division by zero";
                    return 0.0;
                }
                left /= right;
            } else {
                break;
            }
        }
        return left;
    }

    double parse_power() {
        double left = parse_unary();
        if (error_.empty() && consume('^')) {
            double right = parse_power();
            left = std::pow(left, right);
            if (!std::isfinite(left)) error_ = "invalid power result";
        }
        return left;
    }

    double parse_unary() {
        if (consume('+')) return parse_unary();
        if (consume('-')) return -parse_unary();
        return parse_primary();
    }

    double parse_primary() {
        skip_space();
        if (consume('(')) {
            double value = parse_expression();
            if (!consume(')') && error_.empty()) error_ = "missing closing parenthesis";
            return value;
        }

        const char* begin = input_.data() + position_;
        char* end = nullptr;
        double value = std::strtod(begin, &end);
        if (end == begin) {
            error_ = "expected a number";
            return 0.0;
        }
        position_ += static_cast<std::size_t>(end - begin);
        if (!std::isfinite(value)) error_ = "invalid number";
        return value;
    }
};

} // namespace

Result evaluate(const std::string& expression) {
    return Parser(expression).run();
}

} // namespace infiltrator::calc
