/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "advanced_tools_internal.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace calculator::tools {
namespace {

constexpr std::array<ToolDescriptor, 14> kCatalog{{
    {AdvancedTool::Engineering, "Engineering",
     "ohm V I | power V I | reactance-c Hz F | reactance-l Hz H | resonance H F | parallel R1,R2,... | three-phase V I PF | db ratio | db-power ratio",
     "ohm 12 2"},
    {AdvancedTool::UnitConversion, "Unit conversion",
     "value FROM TO. Supports length, mass, temperature, area, volume, speed, pressure, energy, power, duration, frequency, digital-storage and angle units.",
     "100 km mi"},
    {AdvancedTool::Network, "ICT / network",
     "subnet IPv4/prefix | cidr usable-hosts | transfer bytes bits-per-second",
     "subnet 192.168.10.42/24"},
    {AdvancedTool::Storage, "Storage / filesystem",
     "convert value FROM TO | raid LEVEL disks size-per-disk unit | clusters file-bytes cluster-bytes",
     "raid 5 6 4 TiB"},
    {AdvancedTool::DateTime, "Date / time",
     "diff YYYY-MM-DD YYYY-MM-DD | add YYYY-MM-DD days | unix YYYY-MM-DDTHH:MM:SSZ",
     "diff 2026-09-20 2027-01-01"},
    {AdvancedTool::Constants, "Constants",
     "Enter list or a constant name/alias: pi, e, tau, phi, c0, G, h, hbar, kB, NA, qe, me, mp, g0, eps0, mu0, Rgas.",
     "c0"},
    {AdvancedTool::Statistics, "Statistics",
     "Comma or space separated sample values. Produces count, sum, mean, median, quartiles, range and population/sample variance/stddev.",
     "1, 2, 3, 4, 5"},
    {AdvancedTool::Graph, "Graphing",
     "expression ; xmin ; xmax ; samples. x is the independent variable; samples defaults to 201.",
     "sin(x) ; -6.283185307 ; 6.283185307 ; 241"},
    {AdvancedTool::EquationSolver, "Equation solving",
     "expression ; xmin ; xmax. Finds real roots of expression = 0 by deterministic scan + safeguarded bisection.",
     "x^2-2 ; 0 ; 2"},
    {AdvancedTool::ExactDecimal, "Exact decimal",
     "Exact rational arithmetic over decimal literals with + - * / and parentheses. Output is an exact fraction, not binary floating point.",
     "0.1 + 0.2"},
    {AdvancedTool::ArbitraryPrecision, "Arbitrary precision",
     "Arbitrary-size integer arithmetic with + - * ^, parentheses and postfix !. Exponents must be non-negative integers.",
     "2^256"},
    {AdvancedTool::Complex, "Complex numbers",
     "add|sub|mul|div a,b c,d | conj|abs|arg|polar a,b where a,b is real,imaginary.",
     "mul 1,2 3,-4"},
    {AdvancedTool::Financial, "Financial",
     "ctrm rate fv pv | ddb cost life period | fv payment rate periods | gpm cost margin | pmt principal rate periods | pv payment rate periods | rate fv pv periods | sln cost salvage life | syd cost salvage life period | term payment fv rate. Rates are decimals.",
     "pmt 250000 0.005 360"},
    {AdvancedTool::NumberUtilities, "Number utilities",
     "mod a b | factor n | gcd a b | lcm a b | perm n r | comb n r | root n x | char UTF8 | code U+NNNN | ones value bits | twos value bits",
     "factor 360"}
}};

} // namespace

const std::array<ToolDescriptor, 14>& catalog() noexcept {
    return kCatalog;
}

const ToolDescriptor& descriptor(AdvancedTool tool) noexcept {
    const std::size_t index = static_cast<std::size_t>(tool);
    return kCatalog[index < kCatalog.size() ? index : 0U];
}

ToolResult evaluate(AdvancedTool tool, std::string_view input) {
    switch (tool) {
    case AdvancedTool::Engineering:
        return detail::engineering_tool(input);
    case AdvancedTool::UnitConversion:
        return detail::unit_tool(input);
    case AdvancedTool::Network:
        return detail::network_tool(input);
    case AdvancedTool::Storage:
        return detail::storage_tool(input);
    case AdvancedTool::DateTime:
        return detail::datetime_tool(input);
    case AdvancedTool::Constants:
        return detail::constants_tool(input);
    case AdvancedTool::Statistics:
        return detail::statistics_tool(input);
    case AdvancedTool::Graph:
        return detail::graph_tool(input);
    case AdvancedTool::EquationSolver:
        return detail::equation_tool(input);
    case AdvancedTool::ExactDecimal:
        return detail::exact_decimal_tool(input);
    case AdvancedTool::ArbitraryPrecision:
        return detail::arbitrary_precision_tool(input);
    case AdvancedTool::Complex:
        return detail::complex_tool(input);
    case AdvancedTool::Financial:
        return detail::financial_tool(input);
    case AdvancedTool::NumberUtilities:
        return detail::number_utilities_tool(input);
    }
    return detail::failure("Unknown advanced tool.");
}

} // namespace calculator::tools
