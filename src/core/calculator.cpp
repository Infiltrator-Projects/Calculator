/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator.hpp"

#include <infiltratr/core.h>
#include <infiltratr/token.h>

#include <cctype>
#include <charconv>
#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace calculator {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kE = 2.718281828459045235360287471352662498;
constexpr std::size_t kMaxParseDepth = 256;
constexpr std::size_t kMaxFunctionDepth = 64;

constexpr std::array<std::string_view, 24> kBuiltinFunctions{{
    "frac", "int", "round", "sgn",
    "sin", "cos", "tan", "asin", "acos", "atan",
    "sinh", "cosh", "tanh", "asinh", "acosh", "atanh",
    "sqrt", "cbrt", "square", "cube",
    "ln", "log", "exp", "abs"
}};

std::string normalize_expression_spelling(std::string_view input) {
    const auto subscript_digit = [](std::string_view remaining,
                                    int& digit,
                                    std::size_t& bytes) {
        static constexpr std::array<std::pair<std::string_view, int>, 10> map{{
            {"₀", 0}, {"₁", 1}, {"₂", 2}, {"₃", 3}, {"₄", 4},
            {"₅", 5}, {"₆", 6}, {"₇", 7}, {"₈", 8}, {"₉", 9}
        }};
        for (const auto& [symbol, value] : map) {
            if (remaining.substr(0, symbol.size()) == symbol) {
                digit = value;
                bytes = symbol.size();
                return true;
            }
        }
        return false;
    };

    std::string normalized;
    normalized.reserve(input.size() + 8U);
    for (std::size_t i = 0; i < input.size();) {
        const auto remaining = input.substr(i);
        if (remaining.substr(0, std::string_view("log").size()) == "log") {
            std::size_t cursor = 3U;
            std::string base;
            int digit = 0;
            std::size_t bytes = 0U;
            while (cursor < remaining.size() &&
                   subscript_digit(remaining.substr(cursor), digit, bytes)) {
                base.push_back(static_cast<char>('0' + digit));
                cursor += bytes;
            }
            if (!base.empty()) {
                normalized += "log";
                normalized += base;
                i += cursor;
                continue;
            }
        }

        int root_digit = 0;
        std::size_t root_bytes = 0U;
        if (subscript_digit(remaining, root_digit, root_bytes)) {
            std::size_t cursor = root_bytes;
            std::string degree(1, static_cast<char>('0' + root_digit));
            int next_digit = 0;
            std::size_t next_bytes = 0U;
            while (cursor < remaining.size() &&
                   subscript_digit(
                       remaining.substr(cursor), next_digit, next_bytes)) {
                degree.push_back(static_cast<char>('0' + next_digit));
                cursor += next_bytes;
            }
            if (remaining.substr(cursor, std::string_view("√").size()) == "√") {
                normalized += "root";
                normalized += degree;
                normalized.push_back(' ');
                i += cursor + std::string_view("√").size();
                continue;
            }
        }

        if (remaining.substr(0, std::string_view("×").size()) == "×") {
            normalized.push_back('*');
            i += std::string_view("×").size();
        } else if (remaining.substr(0, std::string_view("÷").size()) == "÷") {
            normalized.push_back('/');
            i += std::string_view("÷").size();
        } else if (remaining.substr(0, std::string_view("−").size()) == "−") {
            normalized.push_back('-');
            i += std::string_view("−").size();
        } else if (remaining.substr(0, std::string_view("π").size()) == "π") {
            normalized += "pi";
            i += std::string_view("π").size();
        } else if (remaining.substr(0, std::string_view("τ").size()) == "τ") {
            normalized += "tau";
            i += std::string_view("τ").size();
        } else if (remaining.substr(0, std::string_view("√").size()) == "√") {
            normalized += "sqrt ";
            i += std::string_view("√").size();
        } else {
            normalized.push_back(input[i]);
            ++i;
        }
    }
    return normalized;
}

constexpr std::array<ConstantInfo, 17> kConstants{{
    {"pi", "π", kPi, ""},
    {"e", "euler", kE, ""},
    {"tau", "τ", 2.0 * kPi, ""},
    {"phi", "golden", 1.6180339887498948482, ""},
    {"c0", "c", 299792458.0, "m/s"},
    {"G", "grav", 6.67430e-11, "m^3 kg^-1 s^-2"},
    {"h", "planck", 6.62607015e-34, "J s"},
    {"hbar", "ħ", 1.0545718176461565e-34, "J s"},
    {"kB", "boltzmann", 1.380649e-23, "J/K"},
    {"NA", "avogadro", 6.02214076e23, "mol^-1"},
    {"qe", "electron-charge", 1.602176634e-19, "C"},
    {"me", "electron-mass", 9.1093837139e-31, "kg"},
    {"mp", "proton-mass", 1.67262192595e-27, "kg"},
    {"g0", "gravity", 9.80665, "m/s^2"},
    {"eps0", "epsilon0", 8.8541878188e-12, "F/m"},
    {"mu0", "permeability", 1.25663706127e-6, "N/A^2"},
    {"Rgas", "gas", 8.31446261815324, "J mol^-1 K^-1"}
}};

const ConstantInfo* lookup_constant(std::string_view name) noexcept {
    for (const auto& constant : kConstants) {
        if (constant.name == name || constant.alias == name) return &constant;
    }
    return nullptr;
}

std::string inverse_function_name(std::string_view name) {
    if (name == "sin") return "asin";
    if (name == "cos") return "acos";
    if (name == "tan") return "atan";
    if (name == "sinh") return "asinh";
    if (name == "cosh") return "acosh";
    if (name == "tanh") return "atanh";
    return {};
}

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

// Binary64 expression grammar (highest-level production first). Standard and
// supporting real-expression evaluation share these arithmetic precedence
// rules; Standard disables named constants/functions/variables while retaining
// the same mathematical interpretation of an expression:
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
    Parser(std::string_view input, const Variables& variables,
           const Functions* functions = nullptr,
           std::size_t function_depth = 0,
           AngleUnit angle_unit = AngleUnit::Radians,
           bool allow_named_terms = true)
        : input_(input), variables_(variables), functions_(functions),
          function_depth_(function_depth), angle_unit_(angle_unit),
          allow_named_terms_(allow_named_terms) {}

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
    const Functions* functions_ = nullptr;
    std::size_t function_depth_ = 0;
    AngleUnit angle_unit_ = AngleUnit::Radians;
    bool allow_named_terms_ = true;
    std::size_t position_ = 0;
    std::size_t recursion_depth_ = 0;
    std::string error_;

    Result fail(const char* message) {
        if (error_.empty()) error_ = message;
        return {false, 0.0, error_};
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

    bool consume_text(std::string_view text) {
        skip_space();
        if (position_ + text.size() > input_.size() ||
            input_.substr(position_, text.size()) != text) {
            return false;
        }
        position_ += text.size();
        return true;
    }

    bool consume_keyword(std::string_view keyword) {
        skip_space();
        if (position_ + keyword.size() > input_.size() ||
            input_.substr(position_, keyword.size()) != keyword) {
            return false;
        }
        const std::size_t end = position_ + keyword.size();
        if (end < input_.size() &&
            (infiltratr_ascii_is_alnum(static_cast<unsigned char>(input_[end])) ||
             input_[end] == '_')) {
            return false;
        }
        position_ = end;
        return true;
    }

    bool starts_implicit_factor() {
        skip_space();
        if (position_ >= input_.size()) return false;
        const unsigned char next =
            static_cast<unsigned char>(input_[position_]);
        if (input_[position_] == '(') return true;
        if (allow_named_terms_ &&
            (infiltratr_ascii_is_alpha(next) ||
             input_[position_] == '_')) {
            return true;
        }
        if (infiltratr_ascii_is_digit(next) || input_[position_] == '.') {
            std::size_t previous = position_;
            while (previous > 0U &&
                   infiltratr_ascii_is_space(static_cast<unsigned char>(
                       input_[previous - 1U]))) {
                --previous;
            }
            if (previous > 0U) {
                const char prior = input_[previous - 1U];
                return prior == ')' || prior == '!' || prior == '%';
            }
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
            if (consume('*')) {
                left *= parse_unary();
            } else if (consume('/')) {
                const double right = parse_unary();
                if (right == 0.0) {
                    error_ = "division by zero";
                    return 0.0;
                }
                left /= right;
            } else if (allow_named_terms_ && consume_keyword("mod")) {
                const double right = parse_unary();
                if (right == 0.0) {
                    error_ = "modulus by zero";
                    return 0.0;
                }
                left = std::fmod(left, right);
            } else if (starts_implicit_factor()) {
                left *= parse_unary();
            } else {
                break;
            }
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
        if (error_.empty() &&
            (consume('^') || consume_text("**"))) {
            const double right = parse_unary();
            left = std::pow(left, right);
            if (!std::isfinite(left)) error_ = "invalid power result";
        }
        return left;
    }

    bool consume_superscript_digit(int& digit) {
        static constexpr std::array<std::pair<std::string_view, int>, 10> digits{{
            {"⁰", 0}, {"¹", 1}, {"²", 2}, {"³", 3}, {"⁴", 4},
            {"⁵", 5}, {"⁶", 6}, {"⁷", 7}, {"⁸", 8}, {"⁹", 9}
        }};
        for (const auto& [symbol, value] : digits) {
            if (consume_text(symbol)) {
                digit = value;
                return true;
            }
        }
        return false;
    }

    bool parse_superscript_exponent(double& exponent) {
        const std::size_t saved = position_;
        const bool negative = consume_text("⁻");
        int digit = 0;
        bool any = false;
        int value = 0;
        while (consume_superscript_digit(digit)) {
            any = true;
            if (value > 100000000) {
                position_ = saved;
                return false;
            }
            value = value * 10 + digit;
        }
        if (!any) {
            position_ = saved;
            return false;
        }
        exponent = negative ? -static_cast<double>(value)
                            : static_cast<double>(value);
        return true;
    }

    double parse_postfix() {
        double value = parse_primary();
        while (error_.empty()) {
            if (consume('%')) {
                value /= 100.0;
            } else if (consume('!')) {
                value = factorial(value);
            } else {
                double exponent = 0.0;
                if (!parse_superscript_exponent(exponent)) break;
                value = std::pow(value, exponent);
                if (!std::isfinite(value)) {
                    error_ = "invalid power result";
                    return 0.0;
                }
            }
        }
        return value;
    }

    std::string parse_identifier() {
        skip_space();
        const std::size_t start = position_;
        while (position_ < input_.size() &&
               (infiltratr_ascii_is_alnum(static_cast<unsigned char>(input_[position_])) || input_[position_] == '_')) ++position_;
        return std::string(input_.substr(start, position_ - start));
    }

    double factorial(double value) {
        if (value < 0.0 || std::floor(value) != value) { error_ = "factorial requires a non-negative integer"; return 0.0; }
        if (value > 170.0) { error_ = "factorial overflow"; return 0.0; }
        double result = 1.0;
        for (unsigned int i = 2; i <= static_cast<unsigned int>(value); ++i) result *= i;
        return result;
    }

    double apply_custom_function(
        const std::string& name, const FunctionDefinition& definition,
        const std::vector<double>& arguments) {
        if (arguments.size() != definition.parameters.size()) {
            error_ = "wrong function argument count";
            return 0.0;
        }
        if (function_depth_ >= kMaxFunctionDepth) {
            error_ = "function recursion too deep";
            return 0.0;
        }

        Variables scoped = variables_;
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            scoped[definition.parameters[i]] = arguments[i];
        }

        Parser nested(
            definition.expression, scoped, functions_, function_depth_ + 1U,
            angle_unit_, allow_named_terms_);
        const Result result = nested.run();
        if (!result.ok) {
            error_ = result.error;
            return 0.0;
        }
        return result.value;
    }

    double apply_function(const std::string& name, double x) {
        const auto positive_integer_suffix =
            [](std::string_view text, std::string_view prefix,
               unsigned& value) {
                if (text.size() <= prefix.size() ||
                    text.substr(0, prefix.size()) != prefix) return false;
                unsigned parsed = 0U;
                for (std::size_t i = prefix.size(); i < text.size(); ++i) {
                    const char ch = text[i];
                    if (ch < '0' || ch > '9') return false;
                    if (parsed > 1000000U) return false;
                    parsed = parsed * 10U +
                             static_cast<unsigned>(ch - '0');
                }
                value = parsed;
                return true;
            };

        unsigned dynamic_parameter = 0U;
        if (positive_integer_suffix(name, "log", dynamic_parameter)) {
            if (dynamic_parameter <= 1U || x <= 0.0) {
                error_ = "function domain error";
                return 0.0;
            }
            const double value =
                std::log(x) / std::log(static_cast<double>(dynamic_parameter));
            if (!std::isfinite(value)) {
                error_ = "function domain error";
                return 0.0;
            }
            return value;
        }
        if (positive_integer_suffix(name, "root", dynamic_parameter)) {
            if (dynamic_parameter == 0U) {
                error_ = "function domain error";
                return 0.0;
            }
            if (x < 0.0 && (dynamic_parameter % 2U) == 0U) {
                error_ = "function domain error";
                return 0.0;
            }
            const double magnitude = std::pow(
                std::fabs(x), 1.0 / static_cast<double>(dynamic_parameter));
            const double value = x < 0.0 ? -magnitude : magnitude;
            if (!std::isfinite(value)) {
                error_ = "function domain error";
                return 0.0;
            }
            return value;
        }

        if (name == "frac") return x - std::trunc(x);
        if (name == "int") return std::trunc(x);
        if (name == "round") return std::round(x);
        if (name == "sgn") {
            if (x < 0.0) return -1.0;
            if (x > 0.0) return 1.0;
            return 0.0;
        }

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
            apply_real_function(function, x, angle_unit_);
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
        if (consume('|')) {
            const double value = parse_expression();
            if (!consume('|') && error_.empty()) {
                error_ = "missing closing absolute-value bar";
                return 0.0;
            }
            return std::fabs(value);
        }

        if (position_ < input_.size() &&
            (infiltratr_ascii_is_alpha(static_cast<unsigned char>(input_[position_])) || input_[position_] == '_')) {
            if (!allow_named_terms_) {
                error_ = "unsupported standard expression";
                return 0.0;
            }
            const std::string name = parse_identifier();
            if (const ConstantInfo* constant = lookup_constant(name)) {
                return constant->value;
            }
            if (name == "rand") {
                static thread_local std::mt19937_64 engine([] {
                    std::random_device source;
                    const std::uint64_t high =
                        static_cast<std::uint64_t>(source()) << 32U;
                    const std::uint64_t low =
                        static_cast<std::uint64_t>(source());
                    return high ^ low;
                }());
                return std::generate_canonical<double, 53>(engine);
            }

            std::string function_name = name;
            if (consume_text("⁻¹")) {
                function_name = inverse_function_name(name);
                if (function_name.empty()) {
                    error_ = "inverse notation requires a trigonometric function";
                    return 0.0;
                }
            }

            const FunctionDefinition* custom = nullptr;
            if (functions_) {
                const auto fit = functions_->find(name);
                if (fit != functions_->end()) custom = &fit->second;
            }

            if (consume('(')) {
                if (custom) {
                    std::vector<double> arguments;
                    skip_space();
                    if (!consume(')')) {
                        while (error_.empty()) {
                            arguments.push_back(parse_expression());
                            if (consume(')')) break;
                            if (!consume(';')) {
                                error_ =
                                    "expected ';' or ')' in function arguments";
                                break;
                            }
                        }
                    }
                    if (!error_.empty()) return 0.0;
                    return apply_custom_function(name, *custom, arguments);
                }

                const double argument = parse_expression();
                if (!consume(')') && error_.empty())
                    error_ = "missing closing parenthesis";
                if (!error_.empty()) return 0.0;
                return apply_function(function_name, argument);
            }
            if (custom && custom->parameters.size() == 1U) {
                const double argument = parse_unary();
                if (!error_.empty()) return 0.0;
                return apply_custom_function(
                    name, *custom, std::vector<double>{argument});
            }
            if (is_builtin_function_name(function_name)) {
                const double argument = parse_unary();
                if (!error_.empty()) return 0.0;
                return apply_function(function_name, argument);
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


} // namespace

const std::array<ConstantInfo, 17>& constant_catalog() noexcept {
    return kConstants;
}

bool is_builtin_function_name(std::string_view name) noexcept {
    for (const auto function_name : kBuiltinFunctions) {
        if (function_name == name) return true;
    }
    if (name == "exp2" || name == "exp10" ||
        name == "floor" || name == "ceil") return true;

    const auto numbered = [name](std::string_view prefix) {
        if (name.size() <= prefix.size() ||
            name.substr(0, prefix.size()) != prefix) {
            return false;
        }
        for (std::size_t i = prefix.size(); i < name.size(); ++i) {
            if (name[i] < '0' || name[i] > '9') return false;
        }
        return true;
    };
    return numbered("log") || numbered("root");
}

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

std::string serialize_value(double value) {
    if (!std::isfinite(value)) return {};

    char buffer[64] = {};
    // The precision-free floating to_chars overload is specified to choose the
    // shortest representation that round-trips through the matching parser.
    // Common's decimal-token parser performs exact correctly-rounded binary64
    // conversion, so this is a lossless state boundary rather than formatting.
    const auto converted = std::to_chars(
        buffer, buffer + sizeof(buffer),
        value, std::chars_format::general);
    if (converted.ec != std::errc{}) return {};
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

    // Derive engineering notation from one rounded scientific representation
    // instead of scaling by pow(10, exponent). This keeps the smallest
    // binary64 subnormals representable and lets decimal rounding carry across
    // an engineering exponent boundary before the mantissa is rearranged.
    char scientific_buffer[96] = {};
    const auto scientific_converted = std::to_chars(
        scientific_buffer, scientific_buffer + sizeof(scientific_buffer),
        value, std::chars_format::scientific, 11);
    if (scientific_converted.ec != std::errc{}) return "0";

    const std::string scientific(
        scientific_buffer, scientific_converted.ptr);
    const std::size_t exponent_pos = scientific.find('e');
    if (exponent_pos == std::string::npos ||
        exponent_pos + 2U >= scientific.size()) {
        return "0";
    }

    const char exponent_sign_char = scientific[exponent_pos + 1U];
    if (exponent_sign_char != '+' && exponent_sign_char != '-') return "0";

    std::uint64_t exponent_magnitude = 0U;
    const std::string exponent_text =
        scientific.substr(exponent_pos + 2U);
    if (!infiltratr_parse_u64(
            exponent_text.c_str(), 10U, &exponent_magnitude)) {
        return "0";
    }

    const int scientific_exponent =
        exponent_sign_char == '-'
            ? -static_cast<int>(exponent_magnitude)
            : static_cast<int>(exponent_magnitude);
    int engineering_remainder = scientific_exponent % 3;
    if (engineering_remainder < 0) engineering_remainder += 3;
    const int engineering_exponent =
        scientific_exponent - engineering_remainder;

    const std::string scientific_mantissa =
        scientific.substr(0, exponent_pos);
    const bool negative =
        !scientific_mantissa.empty() && scientific_mantissa.front() == '-';
    const std::size_t mantissa_begin = negative ? 1U : 0U;

    std::string digits;
    digits.reserve(scientific_mantissa.size());
    for (std::size_t i = mantissa_begin;
         i < scientific_mantissa.size(); ++i) {
        if (scientific_mantissa[i] != '.') {
            digits.push_back(scientific_mantissa[i]);
        }
    }
    if (digits.empty()) return "0";

    const std::size_t integer_digits =
        1U + static_cast<std::size_t>(engineering_remainder);
    if (digits.size() < integer_digits) {
        digits.append(integer_digits - digits.size(), '0');
    }

    std::string mantissa;
    if (negative) mantissa.push_back('-');
    mantissa.append(digits.data(), integer_digits);
    if (digits.size() > integer_digits) {
        mantissa.push_back('.');
        mantissa.append(
            digits.data() + integer_digits,
            digits.size() - integer_digits);
    }

    if (mantissa.find('.') != std::string::npos) {
        while (!mantissa.empty() && mantissa.back() == '0') {
            mantissa.pop_back();
        }
        if (!mantissa.empty() && mantissa.back() == '.') {
            mantissa.pop_back();
        }
    }

    std::string formatted = mantissa;
    formatted += 'e';
    formatted += engineering_exponent < 0 ? '-' : '+';

    const unsigned absolute_exponent = static_cast<unsigned>(
        engineering_exponent < 0
            ? -engineering_exponent
            : engineering_exponent);
    if (absolute_exponent < 10U) formatted += '0';

    char exponent_buffer[16] = {};
    const auto engineering_exponent_converted = std::to_chars(
        exponent_buffer, exponent_buffer + sizeof(exponent_buffer),
        absolute_exponent);
    if (engineering_exponent_converted.ec != std::errc{}) return "0";
    formatted.append(
        exponent_buffer, engineering_exponent_converted.ptr);
    return formatted;
}

Result evaluate(const std::string& expression) {
    static const Variables empty_variables;
    const std::string normalized = normalize_expression_spelling(expression);
    return Parser(normalized, empty_variables).run();
}

Result evaluate(const std::string& expression, const Variables& variables) {
    const std::string normalized = normalize_expression_spelling(expression);
    return Parser(normalized, variables).run();
}

Result evaluate(const std::string& expression, const Variables& variables,
                const Functions& functions, AngleUnit angle_unit) {
    const std::string normalized = normalize_expression_spelling(expression);
    return Parser(normalized, variables, &functions, 0U, angle_unit).run();
}

Result evaluate_standard(const std::string& expression) {
    static const Variables empty_variables;
    const std::string normalized = normalize_expression_spelling(expression);
    return Parser(
        normalized, empty_variables, nullptr, 0U,
        AngleUnit::Radians, false).run();
}

} // namespace calculator
