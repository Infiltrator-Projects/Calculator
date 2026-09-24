/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace calculator::tools {

// Shared resource bound for every Advanced Tools command. Native workbenches
// may reject oversized input earlier, but the core entry point is authoritative.
inline constexpr std::size_t kMaxToolInputBytes = 8192U;

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

// Graph samples preserve invalid/discontinuous points explicitly rather than
// connecting across them. Platform renderers must honour valid == false.
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

// Static process-lifetime metadata used by every native workbench. Returned
// string_views refer to Calculator-owned static storage.
const std::array<ToolDescriptor, 14>& catalog() noexcept;
const std::vector<ConversionUnitInfo>& conversion_units() noexcept;

// AdvancedTool is a closed enum; descriptor() therefore always returns a
// catalogue entry and does not expose an optional/failure path.
const ToolDescriptor& descriptor(AdvancedTool tool) noexcept;

// Evaluate one tool-domain command. User/input/domain failures are represented
// by ToolResult::ok/error; graph tools additionally return shared sample points.
// Platform shells must not reimplement the mathematical operation.
ToolResult evaluate(AdvancedTool tool, std::string_view input);

} // namespace calculator::tools
