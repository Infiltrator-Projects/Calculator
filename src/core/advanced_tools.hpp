/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace calculator::tools {

enum class AdvancedTool {
    Engineering,
    UnitConversion,
    Network,
    Storage,
    DateTime,
    Constants,
    Statistics,
    Graph,
    EquationSolver,
    ExactDecimal,
    ArbitraryPrecision,
    Complex,
    Financial,
    NumberUtilities
};

struct ToolDescriptor {
    AdvancedTool tool;
    std::string_view name;
    std::string_view prompt;
    std::string_view example;
};

struct GraphPoint {
    double x = 0.0;
    double y = 0.0;
    bool valid = false;
};

struct ToolResult {
    bool ok = false;
    std::string output;
    std::string error;
    std::vector<GraphPoint> points;
};

struct ConversionUnitInfo {
    std::string_view name;
    std::string_view dimension;
};

const std::array<ToolDescriptor, 14>& catalog() noexcept;
const std::vector<ConversionUnitInfo>& conversion_units() noexcept;
const ToolDescriptor& descriptor(AdvancedTool tool) noexcept;
ToolResult evaluate(AdvancedTool tool, std::string_view input);

} // namespace calculator::tools
