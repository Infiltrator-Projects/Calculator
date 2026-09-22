/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "advanced_tools.hpp"
#include "calculator.hpp"

#include <infiltratr/core.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace calculator::tools::detail {

// Product-neutral parsing remains delegated to Common. These adapters preserve
// Advanced Tools' std::string_view boundary and deterministic ASCII whitespace
// contract without exposing tool-domain semantics through Common.
inline constexpr double kPi = 3.141592653589793238462643383279502884;

inline ToolResult success(std::string text) {
    return {true, std::move(text), {}, {}};
}
inline ToolResult failure(std::string text) {
    return {false, {}, std::move(text), {}};
}

inline std::string trim(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() &&
           infiltratr_ascii_is_space(static_cast<unsigned char>(value[first]))) ++first;
    std::size_t last = value.size();
    while (last > first &&
           infiltratr_ascii_is_space(static_cast<unsigned char>(value[last - 1]))) --last;
    return std::string(value.substr(first, last - first));
}

inline std::vector<std::string> split_ws(std::string_view value) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < value.size()) {
        while (i < value.size() &&
               infiltratr_ascii_is_space(static_cast<unsigned char>(value[i]))) ++i;
        if (i == value.size()) break;
        const std::size_t begin = i;
        while (i < value.size() &&
               !infiltratr_ascii_is_space(static_cast<unsigned char>(value[i]))) ++i;
        out.emplace_back(value.substr(begin, i - begin));
    }
    return out;
}

inline std::vector<std::string> split_semicolon(std::string_view value) {
    std::vector<std::string> out;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const std::size_t end = value.find(';', begin);
        out.push_back(trim(value.substr(
            begin, end == std::string_view::npos ? value.size() - begin
                                                 : end - begin)));
        if (end == std::string_view::npos) break;
        begin = end + 1U;
    }
    return out;
}

inline bool parse_double(std::string_view text, double& value) {
    const std::string input(text);
    return infiltratr_parse_double(input.c_str(), &value);
}

inline bool parse_u64(std::string_view text, std::uint64_t& value,
               unsigned int base = 10U) {
    const std::string input(text);
    return infiltratr_parse_u64(input.c_str(), base, &value);
}

inline bool parse_i64(std::string_view text, std::int64_t& value,
               unsigned int base = 10U) {
    const std::string input(text);
    return infiltratr_parse_i64(input.c_str(), base, &value);
}

inline bool parse_u64_range(std::string_view text, std::uint64_t minimum,
                     std::uint64_t maximum, std::uint64_t& value,
                     unsigned int base = 10U) {
    const std::string input(text);
    return infiltratr_parse_u64_range(
        input.c_str(), base, minimum, maximum, &value);
}

inline bool parse_double_range(std::string_view text, double minimum,
                        double maximum, double& value) {
    const std::string input(text);
    return infiltratr_parse_double_range(
        input.c_str(), minimum, maximum, &value);
}

inline std::string number(double value) {
    return calculator::format_value(value);
}

inline std::string engineering(double value, std::string_view suffix = {}) {
    std::string out = calculator::format_engineering_value(value);
    if (!suffix.empty()) {
        out.push_back(' ');
        out.append(suffix);
    }
    return out;
}

ToolResult engineering_tool(std::string_view input);
ToolResult unit_tool(std::string_view input);
ToolResult network_tool(std::string_view input);
ToolResult storage_tool(std::string_view input);
ToolResult datetime_tool(std::string_view input);
ToolResult constants_tool(std::string_view input);
ToolResult statistics_tool(std::string_view input);
ToolResult graph_tool(std::string_view input);
ToolResult equation_tool(std::string_view input);
ToolResult exact_decimal_tool(std::string_view input);
ToolResult arbitrary_precision_tool(std::string_view input);
ToolResult financial_tool(std::string_view input);
ToolResult number_utilities_tool(std::string_view input);
ToolResult complex_tool(std::string_view input);

} // namespace calculator::tools::detail
