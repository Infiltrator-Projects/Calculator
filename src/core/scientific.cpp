/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scientific.hpp"

#include <boost/multiprecision/cpp_complex.hpp>
#include <infiltratr/core.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace calculator {
namespace {

// The backend is compiled once at the maintained maximum precision. User
// precision controls parsing/serialization policy, not the C++ ABI or backend
// type. ScientificValue is the stable text boundary outside this translation
// unit, so no Boost.Multiprecision type leaks into Session/UI/platform code.
using Real = boost::multiprecision::number<
    boost::multiprecision::backends::cpp_bin_float<kScientificMaxDigits>>;
using Complex = boost::multiprecision::cpp_complex<kScientificMaxDigits>;

// Both expression nesting and user-function recursion are bounded separately:
// one protects native parser stack depth, the other prevents cyclic/custom
// function expansion from becoming an unbounded recursive evaluation.
// These limits are deliberately conservative across the smallest supported
// native stacks (notably Windows). They are product safety bounds, not
// mathematical limits: ordinary calculator expressions should never need this
// much recursive grammar or user-function expansion.
constexpr std::size_t kMaxParseDepth = 16;
constexpr std::size_t kMaxFunctionDepth = 16;

unsigned clamp_digits(unsigned digits) {
    return std::max(16U, std::min(digits, kScientificMaxDigits));
}

Real real_part(const Complex& value) {
    return boost::multiprecision::real(value);
}

Real imag_part(const Complex& value) {
    return boost::multiprecision::imag(value);
}

bool is_zero(const Real& value) {
    return value == 0;
}

bool is_zero(const Complex& value) {
    return is_zero(real_part(value)) && is_zero(imag_part(value));
}

bool is_real(const Complex& value) {
    return is_zero(imag_part(value));
}

Real complex_magnitude(const Complex& value) {
    const Real real = real_part(value);
    const Real imag = imag_part(value);
    return sqrt(real * real + imag * imag);
}

bool is_finite(const Real& value) {
    return boost::multiprecision::isfinite(value);
}

bool is_finite(const Complex& value) {
    return is_finite(real_part(value)) && is_finite(imag_part(value));
}

const Real& pi_value() {
    static const Real value = acos(Real(-1));
    return value;
}

const Real& e_value() {
    static const Real value = exp(Real(1));
    return value;
}

bool real_integer_exponent(const Complex& value, long long& exponent) {
    if (!is_real(value)) return false;
    const Real real = real_part(value);
    if (floor(real) != real) return false;

    const Real minimum(std::numeric_limits<long long>::min());
    const Real maximum(std::numeric_limits<long long>::max());
    if (real < minimum || real > maximum) return false;

    exponent = real.convert_to<long long>();
    return true;
}

Complex integer_power(
    Complex base, long long exponent, bool& valid) {
    valid = true;
    if (exponent == 0) return Complex(1);
    if (exponent < 0 && is_zero(base)) {
        valid = false;
        return {};
    }

    const bool reciprocal = exponent < 0;
    std::uint64_t remaining = reciprocal
        ? static_cast<std::uint64_t>(-(exponent + 1LL)) + 1ULL
        : static_cast<std::uint64_t>(exponent);

    Complex result(1);
    while (remaining != 0U) {
        if ((remaining & 1U) != 0U) result *= base;
        remaining >>= 1U;
        if (remaining != 0U) base *= base;
    }

    if (reciprocal) result = Complex(1) / result;
    if (!is_finite(result)) valid = false;
    return valid ? result : Complex{};
}

Complex principal_log(const Complex& value) {
    // Boost's complex log chooses the lower lip of the negative-real branch
    // cut for an exact +0 imaginary component on some platforms. Calculator
    // adopts the conventional principal argument (-pi, pi], so an exact
    // negative real value is on the +pi side of the cut.
    if (is_real(value) && real_part(value) < 0) {
        return Complex(
            boost::multiprecision::log(-real_part(value)),
            pi_value());
    }
    return boost::multiprecision::log(value);
}

Complex principal_power(
    const Complex& base, const Complex& exponent, bool& valid) {
    valid = true;

    long long integral = 0;
    if (real_integer_exponent(exponent, integral)) {
        return integer_power(base, integral, valid);
    }

    try {
        if (is_real(base) && real_part(base) < 0 &&
            is_real(exponent)) {
            const Real exponent_real = real_part(exponent);
            const Real doubled = exponent_real * 2;
            if (floor(doubled) == doubled) {
                // A real half-integer exponent on the negative-real branch
                // has an exact quadrantal phase. Preserve the exact zero
                // component instead of manufacturing a tiny cos(pi/2)
                // residue through exp(exponent*log(base)).
                Real phase = boost::multiprecision::fmod(
                    doubled, Real(4));
                if (phase < 0) phase += 4;
                const int quadrant = phase.convert_to<int>();

                const Real magnitude = boost::multiprecision::pow(
                    -real_part(base), exponent_real);
                if (!is_finite(magnitude)) {
                    valid = false;
                    return {};
                }
                if (quadrant == 0) return Complex(magnitude);
                if (quadrant == 1) return Complex(0, magnitude);
                if (quadrant == 2) return Complex(-magnitude);
                return Complex(0, -magnitude);
            }
        }

        Complex result;
        if (is_real(base) && real_part(base) < 0) {
            // Force the same upper-lip principal branch as principal_log().
            result = boost::multiprecision::exp(
                exponent * principal_log(base));
        } else {
            result = boost::multiprecision::pow(base, exponent);
        }
        if (!is_finite(result)) {
            valid = false;
            return {};
        }
        return result;
    } catch (...) {
        valid = false;
        return {};
    }
}

bool exact_quadrant(
    const Complex& input, AngleUnit unit, int& quadrant) {
    if (!is_real(input)) return false;
    const Real value = real_part(input);

    Real half_turn_units;
    if (unit == AngleUnit::Degrees) {
        half_turn_units = value / 90;
    } else if (unit == AngleUnit::Gradians) {
        half_turn_units = value / 100;
    } else {
        half_turn_units = value / (pi_value() / 2);
    }

    if (floor(half_turn_units) != half_turn_units) return false;

    Real remainder = boost::multiprecision::fmod(
        half_turn_units, Real(4));
    if (remainder < 0) remainder += 4;
    quadrant = remainder.convert_to<int>();
    return quadrant >= 0 && quadrant <= 3;
}

// Normalization accepts user-facing Unicode/convenience spellings while the
// parser itself stays ASCII and deterministic. This pass changes spelling only;
 // it must not introduce precedence or evaluation semantics of its own.
std::string normalize_expression_spelling(std::string_view input) {
    const auto subscript_digit = [](
        std::string_view remaining, int& digit, std::size_t& bytes) {
        static constexpr std::array<
            std::pair<std::string_view, int>, 10> map{{
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
        if (remaining.substr(0, 3U) == "log") {
            std::size_t cursor = 3U;
            std::string base;
            int digit = 0;
            std::size_t bytes = 0U;
            while (cursor < remaining.size() &&
                   subscript_digit(
                       remaining.substr(cursor), digit, bytes)) {
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
        if (subscript_digit(
                remaining, root_digit, root_bytes)) {
            std::size_t cursor = root_bytes;
            std::string degree(
                1U, static_cast<char>('0' + root_digit));
            int next_digit = 0;
            std::size_t next_bytes = 0U;
            while (cursor < remaining.size() &&
                   subscript_digit(
                       remaining.substr(cursor),
                       next_digit, next_bytes)) {
                degree.push_back(
                    static_cast<char>('0' + next_digit));
                cursor += next_bytes;
            }
            if (remaining.substr(
                    cursor, std::string_view("√").size()) == "√") {
                normalized += "root";
                normalized += degree;
                normalized.push_back(' ');
                i += cursor + std::string_view("√").size();
                continue;
            }
        }

        if (remaining.substr(
                0, std::string_view("×").size()) == "×") {
            normalized.push_back('*');
            i += std::string_view("×").size();
        } else if (remaining.substr(
                       0, std::string_view("÷").size()) == "÷") {
            normalized.push_back('/');
            i += std::string_view("÷").size();
        } else if (remaining.substr(
                       0, std::string_view("−").size()) == "−") {
            normalized.push_back('-');
            i += std::string_view("−").size();
        } else if (remaining.substr(
                       0, std::string_view("π").size()) == "π") {
            normalized += "pi";
            i += std::string_view("π").size();
        } else if (remaining.substr(
                       0, std::string_view("τ").size()) == "τ") {
            normalized += "tau";
            i += std::string_view("τ").size();
        } else if (remaining.substr(
                       0, std::string_view("√").size()) == "√") {
            normalized += "sqrt ";
            i += std::string_view("√").size();
        } else if (remaining.substr(
                       0, std::string_view("ħ").size()) == "ħ") {
            normalized += "hbar";
            i += std::string_view("ħ").size();
        } else {
            normalized.push_back(input[i]);
            ++i;
        }
    }
    return normalized;
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

bool is_builtin(std::string_view name) {
    static constexpr std::array<std::string_view, 31> names{{
        "frac", "int", "round", "sgn",
        "sin", "cos", "tan", "asin", "acos", "atan",
        "sinh", "cosh", "tanh", "asinh", "acosh", "atanh",
        "sqrt", "cbrt", "square", "cube",
        "ln", "log", "exp", "abs",
        "exp2", "exp10", "floor", "ceil",
        "conj", "real", "imag"
    }};
    for (const auto candidate : names) {
        if (candidate == name) return true;
    }

    const auto numbered = [name](std::string_view prefix) {
        if (name.size() <= prefix.size() ||
            name.substr(0, prefix.size()) != prefix) {
            return false;
        }
        for (std::size_t i = prefix.size(); i < name.size(); ++i) {
            if (!infiltratr_ascii_is_digit(
                    static_cast<unsigned char>(name[i]))) {
                return false;
            }
        }
        return true;
    };
    return numbered("log") || numbered("root");
}

Complex constant_value(std::string_view name, bool& found) {
    found = true;
    if (name == "pi") return Complex(pi_value());
    if (name == "e" || name == "euler") return Complex(e_value());
    if (name == "tau") return Complex(2 * pi_value());
    if (name == "phi" || name == "golden") {
        return Complex((Real(1) + sqrt(Real(5))) / 2);
    }
    if (name == "i") return Complex(0, 1);
    if (name == "c0" || name == "c") {
        return Complex(Real("299792458"));
    }
    if (name == "G" || name == "grav") {
        return Complex(Real("6.67430e-11"));
    }
    if (name == "h" || name == "planck") {
        return Complex(Real("6.62607015e-34"));
    }
    if (name == "hbar") {
        return Complex(Real("6.62607015e-34") / (2 * pi_value()));
    }
    if (name == "kB" || name == "boltzmann") {
        return Complex(Real("1.380649e-23"));
    }
    if (name == "NA" || name == "avogadro") {
        return Complex(Real("6.02214076e23"));
    }
    if (name == "qe") return Complex(Real("1.602176634e-19"));
    if (name == "me") return Complex(Real("9.1093837139e-31"));
    if (name == "mp") return Complex(Real("1.67262192595e-27"));
    if (name == "g0" || name == "gravity") {
        return Complex(Real("9.80665"));
    }
    if (name == "eps0" || name == "epsilon0") {
        return Complex(Real("8.8541878188e-12"));
    }
    if (name == "mu0" || name == "permeability") {
        return Complex(Real("1.25663706127e-6"));
    }
    if (name == "Rgas" || name == "gas") {
        return Complex(Real("8.31446261815324"));
    }
    found = false;
    return {};
}

std::string component_string(
    const Real& value, unsigned digits, bool scientific = false) {
    if (value == 0) return "0";

    std::ostringstream out;
    out.imbue(std::locale::classic());
    if (scientific) out << std::scientific;
    else out << std::defaultfloat;
    out << std::setprecision(
        static_cast<int>(clamp_digits(digits))) << value;

    std::string text = out.str();
    if (text == "-0") text = "0";
    return text;
}

std::string trim_fraction_zeroes(std::string text) {
    const std::size_t exponent = text.find_first_of("eE");
    const std::size_t mantissa_end =
        exponent == std::string::npos ? text.size() : exponent;
    const std::size_t dot = text.find('.');
    if (dot == std::string::npos || dot >= mantissa_end) {
        return text;
    }

    std::size_t end = mantissa_end;
    while (end > dot + 1U && text[end - 1U] == '0') --end;
    if (end == dot + 1U) --end;
    text.erase(end, mantissa_end - end);
    return text;
}

std::string group_integer_digits(std::string text) {
    const std::size_t exponent = text.find_first_of("eE");
    const std::size_t mantissa_end =
        exponent == std::string::npos ? text.size() : exponent;
    const std::size_t dot = text.find('.');
    const std::size_t integer_end =
        dot != std::string::npos && dot < mantissa_end
            ? dot : mantissa_end;
    const std::size_t first_digit =
        !text.empty() &&
        (text.front() == '-' || text.front() == '+')
            ? 1U : 0U;
    if (integer_end <= first_digit + 3U) return text;

    for (std::size_t position = integer_end;
         position > first_digit + 3U;) {
        position -= 3U;
        text.insert(position, 1U, ',');
    }
    return text;
}

std::string engineering_component(
    const Real& value, unsigned significant_digits);

std::string display_component(
    const Real& value,
    ScientificDisplayFormat format,
    unsigned precision,
    bool trailing_zeroes,
    bool group_thousands) {
    if (format == ScientificDisplayFormat::General) {
        std::string text = component_string(
            value, std::max(1U, precision));
        return group_thousands
            ? group_integer_digits(std::move(text))
            : text;
    }

    if (format == ScientificDisplayFormat::Engineering) {
        return engineering_component(
            value, std::max(2U, precision));
    }

    std::ostringstream out;
    out.imbue(std::locale::classic());
    if (format == ScientificDisplayFormat::Fixed) {
        out << std::fixed;
    } else {
        out << std::scientific;
    }
    out << std::setprecision(static_cast<int>(precision)) << value;

    std::string text = out.str();
    if (!trailing_zeroes) {
        text = trim_fraction_zeroes(std::move(text));
    }
    if (group_thousands &&
        format == ScientificDisplayFormat::Fixed) {
        text = group_integer_digits(std::move(text));
    }
    if (text == "-0") text = "0";
    return text;
}

std::string engineering_component(
    const Real& value, unsigned significant_digits) {
    if (value == 0) return "0e+00";

    const bool negative = value < 0;
    const Real magnitude = negative ? -value : value;
    std::ostringstream scientific;
    scientific.imbue(std::locale::classic());
    scientific << std::scientific
               << std::setprecision(
                      static_cast<int>(
                          std::max(1U, significant_digits) - 1U))
               << magnitude;
    const std::string text = scientific.str();
    const std::size_t exponent_position = text.find('e');
    if (exponent_position == std::string::npos) {
        return component_string(value, significant_digits, true);
    }

    int exponent = 0;
    try {
        exponent = std::stoi(text.substr(exponent_position + 1U));
    } catch (...) {
        return component_string(value, significant_digits, true);
    }

    int remainder = exponent % 3;
    if (remainder < 0) remainder += 3;
    const int engineering_exponent = exponent - remainder;

    std::string digits;
    for (std::size_t i = 0; i < exponent_position; ++i) {
        if (text[i] != '.') digits.push_back(text[i]);
    }
    while (!digits.empty() && digits.back() == '0') {
        digits.pop_back();
    }
    if (digits.empty()) digits = "0";

    const std::size_t integer_digits =
        1U + static_cast<std::size_t>(remainder);
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

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << mantissa << 'e'
        << (engineering_exponent < 0 ? '-' : '+')
        << std::setw(2) << std::setfill('0')
        << std::abs(engineering_exponent);
    return out.str();
}

// Serialize across the public boundary as decimal components. The chosen
// digit count is sufficient for the requested Scientific presentation contract
// while keeping third-party numeric types private to this implementation.
ScientificValue pack_value(const Complex& value, unsigned digits) {
    return {
        component_string(real_part(value), digits),
        component_string(imag_part(value), digits)
    };
}

Complex unpack_value(const ScientificValue& value) {
    return Complex(Real(value.real), Real(value.imag));
}

// Recursive-descent parser for the Scientific domain. Operator precedence is
// encoded by the call graph (expression -> term -> unary -> power -> postfix ->
// primary); callers must not reproduce or "fix up" precedence in UI code.
class Parser {
public:
    Parser(
        std::string_view input,
        const ScientificVariables& variables,
        const Functions* functions,
        std::size_t function_depth,
        AngleUnit angle_unit,
        unsigned digits)
        : input_(input),
          variables_(variables),
          functions_(functions),
          function_depth_(function_depth),
          angle_unit_(angle_unit),
          digits_(digits) {}

    ScientificResult run() {
        Complex value;
        if (!run_internal(value)) return fail(error_.c_str());
        return {true, pack_value(value, digits_), {}, {}};
    }

private:
    std::string_view input_;
    const ScientificVariables& variables_;
    const Functions* functions_ = nullptr;
    std::size_t function_depth_ = 0;
    AngleUnit angle_unit_ = AngleUnit::Radians;
    unsigned digits_ = kScientificDefaultDigits;
    std::size_t position_ = 0;
    std::size_t recursion_depth_ = 0;
    std::string error_;

    // Internal nested evaluation stays in the full backend precision. The
    // requested decimal precision is applied only when a value crosses the
    // public ScientificValue boundary; a user-defined function inside one
    // expression must not acquire an extra rounding step merely because it
    // was factored into a function.
    bool run_internal(Complex& value) {
        skip_space();
        if (input_.empty()) {
            error_ = "empty expression";
            return false;
        }

        value = parse_expression();
        skip_space();
        if (!error_.empty()) return false;
        if (position_ != input_.size()) {
            error_ = "unexpected input";
            return false;
        }
        if (!is_finite(value)) {
            error_ = "non-finite result";
            return false;
        }
        return true;
    }

    ScientificResult fail(const char* message) {
        if (error_.empty()) {
            error_ = message ? message : "invalid expression";
        }
        return {false, {}, error_, {}};
    }

    void skip_space() {
        while (position_ < input_.size() &&
               infiltratr_ascii_is_space(
                   static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    bool consume(char character) {
        skip_space();
        if (position_ < input_.size() &&
            input_[position_] == character) {
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
            (infiltratr_ascii_is_alnum(
                 static_cast<unsigned char>(input_[end])) ||
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
        if (input_[position_] == '(' ||
            infiltratr_ascii_is_alpha(next) ||
            input_[position_] == '_') {
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

    Complex parse_expression() {
        Complex left = parse_term();
        while (error_.empty()) {
            if (consume('+')) left += parse_term();
            else if (consume('-')) left -= parse_term();
            else break;
        }
        return left;
    }

    Complex parse_term() {
        Complex left = parse_unary();
        while (error_.empty()) {
            if (consume('*')) {
                left *= parse_unary();
            } else if (consume('/')) {
                const Complex right = parse_unary();
                if (is_zero(right)) {
                    error_ = "division by zero";
                    return {};
                }
                left /= right;
            } else if (consume_keyword("mod")) {
                const Complex right = parse_unary();
                if (!is_real(left) || !is_real(right)) {
                    error_ = "modulus requires real values";
                    return {};
                }
                if (is_zero(right)) {
                    error_ = "modulus by zero";
                    return {};
                }
                left = Complex(boost::multiprecision::fmod(
                    real_part(left), real_part(right)));
            } else if (starts_implicit_factor()) {
                left *= parse_unary();
            } else {
                break;
            }
        }
        return left;
    }

    Complex parse_unary() {
        if (recursion_depth_ >= kMaxParseDepth) {
            error_ = "expression nesting too deep";
            return {};
        }

        ++recursion_depth_;
        Complex value;
        if (consume('+')) value = parse_unary();
        else if (consume('-')) value = -parse_unary();
        else value = parse_power();
        --recursion_depth_;
        return value;
    }

    Complex parse_power() {
        Complex left = parse_postfix();
        if (error_.empty() &&
            (consume('^') || consume_text("**"))) {
            const Complex right = parse_unary();
            bool valid = true;
            left = principal_power(left, right, valid);
            if (!valid) {
                error_ = "invalid power result";
                return {};
            }
        }
        return left;
    }

    bool consume_superscript_digit(int& digit) {
        static constexpr std::array<
            std::pair<std::string_view, int>, 10> digits{{
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

    bool parse_superscript_exponent(long long& exponent) {
        const std::size_t saved = position_;
        const bool negative = consume_text("⁻");
        int digit = 0;
        bool any = false;
        long long value = 0;
        while (consume_superscript_digit(digit)) {
            any = true;
            if (value > 100000000LL) {
                position_ = saved;
                return false;
            }
            value = value * 10LL + digit;
        }
        if (!any) {
            position_ = saved;
            return false;
        }
        exponent = negative ? -value : value;
        return true;
    }

    Complex factorial(Complex value) {
        if (!is_real(value)) {
            error_ = "factorial requires a real non-negative integer";
            return {};
        }
        const Real input = real_part(value);
        if (input < 0 || floor(input) != input) {
            error_ = "factorial requires a non-negative integer";
            return {};
        }
        if (input > 100000) {
            error_ = "factorial input too large";
            return {};
        }

        const unsigned long count =
            input.convert_to<unsigned long>();
        Real result = 1;
        for (unsigned long i = 2; i <= count; ++i) {
            result *= i;
        }
        return Complex(result);
    }

    Complex parse_postfix() {
        Complex value = parse_primary();
        while (error_.empty()) {
            if (consume('%')) {
                value /= 100;
            } else if (consume('!')) {
                value = factorial(value);
            } else {
                long long exponent = 0;
                if (!parse_superscript_exponent(exponent)) break;
                bool valid = true;
                value = integer_power(value, exponent, valid);
                if (!valid) {
                    error_ = "invalid power result";
                    return {};
                }
            }
        }
        return value;
    }

    std::string parse_identifier() {
        skip_space();
        const std::size_t start = position_;
        while (position_ < input_.size() &&
               (infiltratr_ascii_is_alnum(
                    static_cast<unsigned char>(input_[position_])) ||
                input_[position_] == '_')) {
            ++position_;
        }
        return std::string(
            input_.substr(start, position_ - start));
    }

    bool positive_integer_suffix(
        std::string_view name,
        std::string_view prefix,
        unsigned& value) {
        if (name.size() <= prefix.size() ||
            name.substr(0, prefix.size()) != prefix) {
            return false;
        }

        unsigned parsed = 0U;
        for (std::size_t i = prefix.size(); i < name.size(); ++i) {
            const char character = name[i];
            if (character < '0' || character > '9' ||
                parsed > 1000000U) {
                return false;
            }
            parsed = parsed * 10U +
                static_cast<unsigned>(character - '0');
        }
        value = parsed;
        return true;
    }

    Complex apply_custom_function(
        const FunctionDefinition& definition,
        const std::vector<Complex>& arguments) {
        if (arguments.size() != definition.parameters.size()) {
            error_ = "wrong function argument count";
            return {};
        }
        if (function_depth_ >= kMaxFunctionDepth) {
            error_ = "function recursion too deep";
            return {};
        }

        ScientificVariables scoped = variables_;
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            // Function arguments are an internal expression boundary, not a
            // public precision boundary. Retain maximum maintained digits so
            // factoring an expression into a user function does not reduce
            // its numerical quality.
            scoped[definition.parameters[i]] =
                pack_value(arguments[i], kScientificMaxDigits);
        }

        Parser nested(
            definition.expression, scoped, functions_,
            function_depth_ + 1U, angle_unit_, digits_);
        Complex result;
        if (!nested.run_internal(result)) {
            error_ = nested.error_;
            return {};
        }
        return result;
    }

    Complex to_radians(const Complex& value) const {
        if (angle_unit_ == AngleUnit::Degrees) {
            return value * Complex(pi_value() / 180);
        }
        if (angle_unit_ == AngleUnit::Gradians) {
            return value * Complex(pi_value() / 200);
        }
        return value;
    }

    Complex from_radians(const Complex& value) const {
        if (angle_unit_ == AngleUnit::Degrees) {
            return value * Complex(180 / pi_value());
        }
        if (angle_unit_ == AngleUnit::Gradians) {
            return value * Complex(200 / pi_value());
        }
        return value;
    }

    Complex apply_function(
        const std::string& name, const Complex& input) {
        unsigned parameter = 0U;
        if (positive_integer_suffix(name, "log", parameter)) {
            if (parameter <= 1U) {
                error_ = "function domain error";
                return {};
            }
            return principal_log(input) /
                boost::multiprecision::log(Complex(parameter));
        }

        if (positive_integer_suffix(name, "root", parameter)) {
            if (parameter == 0U) {
                error_ = "function domain error";
                return {};
            }
            if (is_real(input) && real_part(input) < 0 &&
                (parameter % 2U) != 0U) {
                bool valid = true;
                const Complex magnitude = principal_power(
                    Complex(-real_part(input)),
                    Complex(Real(1) / parameter), valid);
                if (!valid) {
                    error_ = "function domain error";
                    return {};
                }
                return -magnitude;
            }
            bool valid = true;
            const Complex result = principal_power(
                input, Complex(Real(1) / parameter), valid);
            if (!valid) {
                error_ = "function domain error";
                return {};
            }
            return result;
        }

        if (name == "frac" || name == "int" ||
            name == "round" || name == "floor" ||
            name == "ceil") {
            if (!is_real(input)) {
                error_ = "function requires a real value";
                return {};
            }

            const Real value = real_part(input);
            Real result;
            if (name == "frac") result = value - trunc(value);
            else if (name == "int") result = trunc(value);
            else if (name == "round") {
                result = value >= 0
                    ? floor(value + Real("0.5"))
                    : ceil(value - Real("0.5"));
            } else if (name == "floor") {
                result = floor(value);
            } else {
                result = ceil(value);
            }
            return Complex(result);
        }

        if (name == "sgn") {
            if (is_zero(input)) return {};
            if (is_real(input)) {
                return Complex(real_part(input) < 0 ? -1 : 1);
            }
            return input /
                Complex(complex_magnitude(input));
        }

        try {
            if (name == "sin") {
                int quadrant = 0;
                if (exact_quadrant(input, angle_unit_, quadrant)) {
                    if (quadrant == 0 || quadrant == 2) return {};
                    return Complex(quadrant == 1 ? 1 : -1);
                }
                return boost::multiprecision::sin(
                    to_radians(input));
            }
            if (name == "cos") {
                int quadrant = 0;
                if (exact_quadrant(input, angle_unit_, quadrant)) {
                    if (quadrant == 1 || quadrant == 3) return {};
                    return Complex(quadrant == 0 ? 1 : -1);
                }
                return boost::multiprecision::cos(
                    to_radians(input));
            }
            if (name == "tan") {
                int quadrant = 0;
                if (exact_quadrant(input, angle_unit_, quadrant)) {
                    if (quadrant == 1 || quadrant == 3) {
                        error_ = "function domain error";
                        return {};
                    }
                    return {};
                }
                return boost::multiprecision::tan(
                    to_radians(input));
            }
            if (name == "asin") {
                return from_radians(
                    boost::multiprecision::asin(input));
            }
            if (name == "acos") {
                return from_radians(
                    boost::multiprecision::acos(input));
            }
            if (name == "atan") {
                return from_radians(
                    boost::multiprecision::atan(input));
            }
            if (name == "sinh") {
                return boost::multiprecision::sinh(input);
            }
            if (name == "cosh") {
                return boost::multiprecision::cosh(input);
            }
            if (name == "tanh") {
                return boost::multiprecision::tanh(input);
            }
            if (name == "asinh") {
                return boost::multiprecision::asinh(input);
            }
            if (name == "acosh") {
                Complex result = boost::multiprecision::acosh(input);
                if (is_real(input) && imag_part(result) < 0) {
                    result = Complex(
                        real_part(result), -imag_part(result));
                }
                return result;
            }
            if (name == "atanh") {
                Complex result = boost::multiprecision::atanh(input);
                if (is_real(input) && imag_part(result) < 0) {
                    result = Complex(
                        real_part(result), -imag_part(result));
                }
                return result;
            }
            if (name == "sqrt") {
                Complex result = boost::multiprecision::sqrt(input);
                if (is_real(input) && real_part(input) < 0 &&
                    imag_part(result) < 0) {
                    result = Complex(
                        real_part(result), -imag_part(result));
                }
                return result;
            }
            if (name == "cbrt") {
                bool valid = true;
                if (is_real(input) && real_part(input) < 0) {
                    const Complex magnitude = principal_power(
                        Complex(-real_part(input)),
                        Complex(Real(1) / 3), valid);
                    if (!valid) {
                        error_ = "function domain error";
                        return {};
                    }
                    return -magnitude;
                }
                const Complex result = principal_power(
                    input, Complex(Real(1) / 3), valid);
                if (!valid) {
                    error_ = "function domain error";
                    return {};
                }
                return result;
            }
            if (name == "square") return input * input;
            if (name == "cube") return input * input * input;
            if (name == "ln") {
                return principal_log(input);
            }
            if (name == "log") {
                return principal_log(input) /
                    boost::multiprecision::log(Complex(10));
            }
            if (name == "exp") {
                return boost::multiprecision::exp(input);
            }
            if (name == "exp2") {
                return boost::multiprecision::pow(
                    Complex(2), input);
            }
            if (name == "exp10") {
                return boost::multiprecision::pow(
                    Complex(10), input);
            }
            if (name == "abs") {
                return Complex(complex_magnitude(input));
            }
            if (name == "conj") {
                return boost::multiprecision::conj(input);
            }
            if (name == "real") return Complex(real_part(input));
            if (name == "imag") return Complex(imag_part(input));
        } catch (...) {
            error_ = "function domain error";
            return {};
        }

        error_ = "unknown function";
        return {};
    }

    Complex parse_number() {
        skip_space();
        const std::size_t start = position_;
        std::size_t cursor = position_;
        bool any_digits = false;

        while (cursor < input_.size() &&
               infiltratr_ascii_is_digit(
                   static_cast<unsigned char>(input_[cursor]))) {
            any_digits = true;
            ++cursor;
        }

        if (cursor < input_.size() && input_[cursor] == '.') {
            ++cursor;
            while (cursor < input_.size() &&
                   infiltratr_ascii_is_digit(
                       static_cast<unsigned char>(input_[cursor]))) {
                any_digits = true;
                ++cursor;
            }
        }

        if (!any_digits) {
            error_ = "expected a number";
            return {};
        }

        if (cursor < input_.size() &&
            (input_[cursor] == 'e' || input_[cursor] == 'E')) {
            ++cursor;
            if (cursor < input_.size() &&
                (input_[cursor] == '+' || input_[cursor] == '-')) {
                ++cursor;
            }
            const std::size_t exponent_digits = cursor;
            while (cursor < input_.size() &&
                   infiltratr_ascii_is_digit(
                       static_cast<unsigned char>(input_[cursor]))) {
                ++cursor;
            }
            if (cursor == exponent_digits) {
                error_ = "invalid number";
                return {};
            }
        }

        const std::string token(
            input_.substr(start, cursor - start));
        position_ = cursor;
        try {
            return Complex(Real(token));
        } catch (...) {
            error_ = "invalid number";
            return {};
        }
    }

    Complex random_value() {
        static thread_local std::mt19937_64 engine([] {
            std::random_device source;
            return (static_cast<std::uint64_t>(source()) << 32U) ^
                static_cast<std::uint64_t>(source());
        }());

        std::string text = "0.";
        text.reserve(static_cast<std::size_t>(digits_) + 2U);
        std::uniform_int_distribution<int> digit(0, 9);
        for (unsigned i = 0; i < digits_; ++i) {
            text.push_back(
                static_cast<char>('0' + digit(engine)));
        }
        return Complex(Real(text));
    }

    Complex parse_primary() {
        skip_space();
        if (consume('(')) {
            const Complex value = parse_expression();
            if (!consume(')') && error_.empty()) {
                error_ = "missing closing parenthesis";
            }
            return value;
        }

        if (consume('|')) {
            const Complex value = parse_expression();
            if (!consume('|') && error_.empty()) {
                error_ = "missing closing absolute-value bar";
                return {};
            }
            return Complex(complex_magnitude(value));
        }

        if (position_ < input_.size() &&
            (infiltratr_ascii_is_alpha(
                 static_cast<unsigned char>(input_[position_])) ||
             input_[position_] == '_')) {
            const std::string name = parse_identifier();

            bool found = false;
            const Complex constant = constant_value(name, found);
            if (found) return constant;
            if (name == "rand") return random_value();

            std::string function_name = name;
            if (consume_text("⁻¹")) {
                function_name = inverse_function_name(name);
                if (function_name.empty()) {
                    error_ =
                        "inverse notation requires a trigonometric function";
                    return {};
                }
            }

            const FunctionDefinition* custom = nullptr;
            if (functions_) {
                const auto found_function = functions_->find(name);
                if (found_function != functions_->end()) {
                    custom = &found_function->second;
                }
            }

            if (consume('(')) {
                if (custom) {
                    std::vector<Complex> arguments;
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
                    if (!error_.empty()) return {};
                    return apply_custom_function(
                        *custom, arguments);
                }

                const Complex argument = parse_expression();
                if (!consume(')') && error_.empty()) {
                    error_ = "missing closing parenthesis";
                }
                if (!error_.empty()) return {};
                return apply_function(function_name, argument);
            }

            if (custom && custom->parameters.size() == 1U) {
                const Complex argument = parse_unary();
                if (!error_.empty()) return {};
                return apply_custom_function(
                    *custom, std::vector<Complex>{argument});
            }

            if (is_builtin(function_name)) {
                const Complex argument = parse_unary();
                if (!error_.empty()) return {};
                return apply_function(function_name, argument);
            }

            const auto variable = variables_.find(name);
            if (variable == variables_.end()) {
                error_ = "unknown variable";
                return {};
            }
            try {
                return unpack_value(variable->second);
            } catch (...) {
                error_ = "invalid stored variable";
                return {};
            }
        }

        return parse_number();
    }
};

} // namespace

// This is the only public construction path from expression text into the
// multiprecision engine: normalize spelling, clamp the requested precision and
// execute the bounded parser. Ordinary user errors stay in ScientificResult.
ScientificResult evaluate_scientific(
    const std::string& expression,
    const ScientificVariables& variables,
    const Functions& functions,
    AngleUnit angle_unit,
    unsigned decimal_digits) {
    const std::string normalized =
        normalize_expression_spelling(expression);
    return Parser(
        normalized, variables, &functions, 0U,
        angle_unit, clamp_digits(decimal_digits)).run();
}

std::string scientific_value_expression(
    const ScientificValue& value) {
    if (value.imag == "0") return value.real;
    if (value.real == "0") {
        if (value.imag == "1") return "i";
        if (value.imag == "-1") return "-i";
        return "(" + value.imag + ")*i";
    }
    return "(" + value.real + ")+(" + value.imag + ")*i";
}

// Formatting reparses the stored decimal components into the same high-
 // precision backend; there is deliberately no intermediate conversion to
 // double, including for complex values.
std::string format_scientific_value(
    const ScientificValue& value,
    unsigned decimal_digits,
    bool scientific_notation) {
    try {
        const Complex parsed = unpack_value(value);
        const std::string real = component_string(
            real_part(parsed), decimal_digits,
            scientific_notation);
        const std::string imag = component_string(
            imag_part(parsed), decimal_digits,
            scientific_notation);

        if (imag == "0") return real;
        if (real == "0") {
            if (imag == "1") return "i";
            if (imag == "-1") return "-i";
            return imag + "i";
        }
        if (!imag.empty() && imag.front() == '-') {
            return real + " - " + imag.substr(1U) + "i";
        }
        return real + " + " + imag + "i";
    } catch (...) {
        return "0";
    }
}

std::string format_engineering_value(
    const ScientificValue& value,
    unsigned significant_digits) {
    try {
        const Complex parsed = unpack_value(value);
        const std::string real =
            engineering_component(
                real_part(parsed), significant_digits);
        const std::string imag =
            engineering_component(
                imag_part(parsed), significant_digits);

        if (imag == "0e+00") return real;
        if (real == "0e+00") return imag + "i";
        if (!imag.empty() && imag.front() == '-') {
            return real + " - " + imag.substr(1U) + "i";
        }
        return real + " + " + imag + "i";
    } catch (...) {
        return "0e+00";
    }
}

std::string format_scientific_display(
    const ScientificValue& value,
    ScientificDisplayFormat format,
    unsigned precision,
    bool trailing_zeroes,
    bool group_thousands) {
    try {
        const Complex parsed = unpack_value(value);
        const Real real_value = real_part(parsed);
        const Real imag_value = imag_part(parsed);

        const std::string real = display_component(
            real_value, format, precision,
            trailing_zeroes, group_thousands);
        const std::string imag = display_component(
            imag_value, format, precision,
            trailing_zeroes, group_thousands);

        if (is_zero(imag_value)) return real;
        if (is_zero(real_value)) {
            if (imag == "1") return "i";
            if (imag == "-1") return "-i";
            return imag + "i";
        }
        if (!imag.empty() && imag.front() == '-') {
            return real + " - " + imag.substr(1U) + "i";
        }
        return real + " + " + imag + "i";
    } catch (...) {
        return "0";
    }
}

bool scientific_value_to_double(
    const ScientificValue& value, double& output) noexcept {
    try {
        const Real imaginary(value.imag);
        if (!is_zero(imaginary)) return false;
        const Real parsed(value.real);
        output = parsed.convert_to<double>();
        return std::isfinite(output);
    } catch (...) {
        return false;
    }
}

ScientificValue scientific_value_from_double(double value) {
    if (!std::isfinite(value)) return {};

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(
        std::numeric_limits<double>::max_digits10) << value;
    return {out.str(), "0"};
}

} // namespace calculator
