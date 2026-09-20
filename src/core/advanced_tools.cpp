/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "advanced_tools.hpp"
#include "calculator.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace calculator::tools {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

constexpr std::array<ToolDescriptor, 12> kCatalog{{
    {AdvancedTool::Engineering, "Engineering",
     "ohm V I | power V I | reactance-c Hz F | reactance-l Hz H | resonance H F | parallel R1,R2,... | three-phase V I PF | db ratio | db-power ratio",
     "ohm 12 2"},
    {AdvancedTool::UnitConversion, "Unit conversion",
     "value FROM TO. Supports length, mass, temperature, area, volume, speed, pressure, energy, power and angle units.",
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
     "mul 1,2 3,-4"}
}};

ToolResult success(std::string text) {
    return {true, std::move(text), {}, {}};
}
ToolResult failure(std::string text) {
    return {false, {}, std::move(text), {}};
}

std::string trim(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return std::string(value.substr(first, last - first));
}

std::vector<std::string> split_ws(std::string_view value) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < value.size()) {
        while (i < value.size() &&
               std::isspace(static_cast<unsigned char>(value[i]))) ++i;
        if (i == value.size()) break;
        const std::size_t begin = i;
        while (i < value.size() &&
               !std::isspace(static_cast<unsigned char>(value[i]))) ++i;
        out.emplace_back(value.substr(begin, i - begin));
    }
    return out;
}

std::vector<std::string> split_semicolon(std::string_view value) {
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

bool parse_double(std::string_view text, double& value) {
    const std::string s = trim(text);
    if (s.empty()) return false;
    const char* begin = s.data();
    const char* end = begin + s.size();
    const auto converted = std::from_chars(
        begin, end, value, std::chars_format::general);
    return converted.ec == std::errc{} &&
           converted.ptr == end && std::isfinite(value);
}

bool parse_u64(std::string_view text, std::uint64_t& value) {
    const std::string s = trim(text);
    if (s.empty()) return false;
    const auto converted = std::from_chars(
        s.data(), s.data() + s.size(), value);
    return converted.ec == std::errc{} &&
           converted.ptr == s.data() + s.size();
}

std::string number(double value) {
    return calculator::format_value(value);
}

std::string engineering(double value, std::string_view suffix = {}) {
    std::string out = calculator::format_engineering_value(value);
    if (!suffix.empty()) {
        out.push_back(' ');
        out.append(suffix);
    }
    return out;
}

ToolResult engineering_tool(std::string_view input) {
    const auto fields = split_ws(input);
    if (fields.empty()) return failure("Enter an engineering operation.");

    auto two = [&](double& a, double& b) {
        return fields.size() == 3U &&
               parse_double(fields[1], a) &&
               parse_double(fields[2], b);
    };

    double a = 0.0;
    double b = 0.0;
    if (fields[0] == "ohm") {
        if (!two(a, b) || b == 0.0) return failure("Usage: ohm volts amps; amps must be non-zero.");
        const double r = a / b;
        const double p = a * b;
        return success("Voltage  " + engineering(a, "V") +
                       "\nCurrent  " + engineering(b, "A") +
                       "\nResistance  " + engineering(r, "ohm") +
                       "\nPower  " + engineering(p, "W"));
    }
    if (fields[0] == "power") {
        if (!two(a, b)) return failure("Usage: power volts amps");
        return success("Power  " + engineering(a * b, "W"));
    }
    if (fields[0] == "reactance-c") {
        if (!two(a, b) || a <= 0.0 || b <= 0.0) return failure("Usage: reactance-c frequency_hz capacitance_f; values must be positive.");
        return success("Capacitive reactance  " +
                       engineering(1.0 / (2.0 * kPi * a * b), "ohm"));
    }
    if (fields[0] == "reactance-l") {
        if (!two(a, b) || a < 0.0 || b < 0.0) return failure("Usage: reactance-l frequency_hz inductance_h");
        return success("Inductive reactance  " +
                       engineering(2.0 * kPi * a * b, "ohm"));
    }
    if (fields[0] == "resonance") {
        if (!two(a, b) || a <= 0.0 || b <= 0.0) return failure("Usage: resonance inductance_h capacitance_f; values must be positive.");
        return success("Resonant frequency  " +
                       engineering(1.0 / (2.0 * kPi * std::sqrt(a * b)), "Hz"));
    }
    if (fields[0] == "three-phase") {
        if (fields.size() != 4U ||
            !parse_double(fields[1], a) ||
            !parse_double(fields[2], b)) {
            return failure("Usage: three-phase line_volts line_amps power_factor");
        }
        double pf = 0.0;
        if (!parse_double(fields[3], pf) || pf < 0.0 || pf > 1.0) {
            return failure("Power factor must be between 0 and 1.");
        }
        return success("Apparent power  " +
                       engineering(std::sqrt(3.0) * a * b, "VA") +
                       "\nReal power  " +
                       engineering(std::sqrt(3.0) * a * b * pf, "W"));
    }
    if (fields[0] == "db" || fields[0] == "db-power") {
        if (fields.size() != 2U || !parse_double(fields[1], a) || a <= 0.0) {
            return failure("Usage: db ratio or db-power ratio; ratio must be positive.");
        }
        const double multiplier = fields[0] == "db" ? 20.0 : 10.0;
        return success("Level  " + number(multiplier * std::log10(a)) + " dB");
    }
    if (fields[0] == "parallel") {
        const std::size_t space = std::string(input).find_first_of(" \t");
        if (space == std::string::npos) return failure("Usage: parallel R1,R2,...");
        std::string list = trim(input.substr(space + 1U));
        std::replace(list.begin(), list.end(), ',', ' ');
        const auto values = split_ws(list);
        if (values.empty()) return failure("Provide at least one resistance.");
        double reciprocal_sum = 0.0;
        for (const auto& item : values) {
            double r = 0.0;
            if (!parse_double(item, r) || r <= 0.0) return failure("Parallel resistances must be positive numbers.");
            reciprocal_sum += 1.0 / r;
        }
        return success("Equivalent resistance  " +
                       engineering(1.0 / reciprocal_sum, "ohm"));
    }
    return failure("Unknown engineering operation. Use the prompt examples.");
}

struct Unit {
    std::string_view name;
    std::string_view dimension;
    double factor;
    double offset;
};

constexpr std::array<Unit, 54> kUnits{{
    {"m","length",1.0,0.0},{"km","length",1000.0,0.0},{"cm","length",0.01,0.0},{"mm","length",0.001,0.0},
    {"um","length",1e-6,0.0},{"nm","length",1e-9,0.0},{"in","length",0.0254,0.0},{"ft","length",0.3048,0.0},
    {"yd","length",0.9144,0.0},{"mi","length",1609.344,0.0},{"nmi","length",1852.0,0.0},
    {"kg","mass",1.0,0.0},{"g","mass",0.001,0.0},{"mg","mass",1e-6,0.0},{"lb","mass",0.45359237,0.0},
    {"oz","mass",0.028349523125,0.0},{"t","mass",1000.0,0.0},
    {"C","temperature",1.0,273.15},{"F","temperature",5.0/9.0,255.3722222222222},{"K","temperature",1.0,0.0},
    {"m2","area",1.0,0.0},{"km2","area",1e6,0.0},{"cm2","area",1e-4,0.0},{"ft2","area",0.09290304,0.0},
    {"in2","area",0.00064516,0.0},{"acre","area",4046.8564224,0.0},{"ha","area",10000.0,0.0},
    {"m3","volume",1.0,0.0},{"L","volume",0.001,0.0},{"mL","volume",1e-6,0.0},{"galUS","volume",0.003785411784,0.0},
    {"galUK","volume",0.00454609,0.0},{"ft3","volume",0.028316846592,0.0},{"in3","volume",0.000016387064,0.0},
    {"mps","speed",1.0,0.0},{"kph","speed",1.0/3.6,0.0},{"mph","speed",0.44704,0.0},{"knot","speed",0.5144444444444445,0.0},
    {"Pa","pressure",1.0,0.0},{"kPa","pressure",1000.0,0.0},{"MPa","pressure",1e6,0.0},{"bar","pressure",100000.0,0.0},
    {"psi","pressure",6894.757293168,0.0},{"atm","pressure",101325.0,0.0},
    {"J","energy",1.0,0.0},{"kJ","energy",1000.0,0.0},{"Wh","energy",3600.0,0.0},{"kWh","energy",3.6e6,0.0},
    {"cal","energy",4.184,0.0},{"kcal","energy",4184.0,0.0},{"BTU","energy",1055.05585262,0.0},
    {"W","power",1.0,0.0},{"kW","power",1000.0,0.0},{"hp","power",745.6998715822702,0.0}
}};

const Unit* find_unit(std::string_view name) {
    for (const auto& unit : kUnits) if (unit.name == name) return &unit;
    static constexpr std::array<Unit, 3> angles{{
        {"deg","angle",kPi/180.0,0.0},
        {"rad","angle",1.0,0.0},
        {"grad","angle",kPi/200.0,0.0}
    }};
    for (const auto& unit : angles) if (unit.name == name) return &unit;
    return nullptr;
}

ToolResult unit_tool(std::string_view input) {
    const auto fields = split_ws(input);
    if (fields.size() != 3U) return failure("Usage: value FROM TO");
    double value = 0.0;
    if (!parse_double(fields[0], value)) return failure("Invalid numeric value.");
    const Unit* from = find_unit(fields[1]);
    const Unit* to = find_unit(fields[2]);
    if (!from || !to) return failure("Unknown unit. See the Unit conversion prompt.");
    if (from->dimension != to->dimension) return failure("Units belong to different dimensions.");
    const double si = value * from->factor + from->offset;
    const double converted = (si - to->offset) / to->factor;
    return success(number(value) + " " + std::string(from->name) +
                   " = " + number(converted) + " " + std::string(to->name));
}

bool parse_ipv4(std::string_view text, std::uint32_t& address) {
    std::uint32_t out = 0;
    std::size_t begin = 0;
    for (int part = 0; part < 4; ++part) {
        const std::size_t end = text.find('.', begin);
        if ((part < 3 && end == std::string_view::npos) ||
            (part == 3 && end != std::string_view::npos)) return false;
        const std::string_view token = text.substr(
            begin, (end == std::string_view::npos ? text.size() : end) - begin);
        unsigned value = 0;
        const auto converted = std::from_chars(
            token.data(), token.data() + token.size(), value);
        if (token.empty() || converted.ec != std::errc{} ||
            converted.ptr != token.data() + token.size() || value > 255U) return false;
        out = (out << 8U) | value;
        begin = end == std::string_view::npos ? text.size() : end + 1U;
    }
    address = out;
    return true;
}

std::string ipv4(std::uint32_t address) {
    std::ostringstream out;
    out << ((address >> 24U) & 0xffU) << '.'
        << ((address >> 16U) & 0xffU) << '.'
        << ((address >> 8U) & 0xffU) << '.'
        << (address & 0xffU);
    return out.str();
}

ToolResult network_tool(std::string_view input) {
    const auto fields = split_ws(input);
    if (fields.empty()) return failure("Enter subnet, cidr or transfer.");
    if (fields[0] == "subnet") {
        if (fields.size() != 2U) return failure("Usage: subnet IPv4/prefix");
        const std::size_t slash = fields[1].find('/');
        if (slash == std::string::npos) return failure("CIDR prefix is required.");
        std::uint32_t address = 0;
        if (!parse_ipv4(std::string_view(fields[1]).substr(0, slash), address)) return failure("Invalid IPv4 address.");
        unsigned prefix = 0;
        const std::string_view p(fields[1].data() + slash + 1U, fields[1].size() - slash - 1U);
        const auto converted = std::from_chars(p.data(), p.data()+p.size(), prefix);
        if (p.empty() || converted.ec != std::errc{} ||
            converted.ptr != p.data()+p.size() || prefix > 32U) return failure("IPv4 prefix must be 0..32.");
        const std::uint32_t mask = prefix == 0U ? 0U : 0xffffffffU << (32U-prefix);
        const std::uint32_t network = address & mask;
        const std::uint32_t broadcast = network | ~mask;
        const std::uint64_t total = prefix == 0U ? (1ULL<<32U) : (1ULL<<(32U-prefix));
        std::uint64_t usable = total;
        std::uint32_t first = network;
        std::uint32_t last = broadcast;
        if (prefix <= 30U) {
            usable = total - 2U;
            first = network + 1U;
            last = broadcast - 1U;
        }
        std::ostringstream out;
        out << "Address  " << ipv4(address) << '/' << prefix
            << "\nNetmask  " << ipv4(mask)
            << "\nNetwork  " << ipv4(network)
            << "\nBroadcast  " << ipv4(broadcast)
            << "\nFirst usable  " << ipv4(first)
            << "\nLast usable  " << ipv4(last)
            << "\nUsable addresses  " << usable;
        return success(out.str());
    }
    if (fields[0] == "cidr") {
        std::uint64_t hosts = 0;
        if (fields.size()!=2U || !parse_u64(fields[1], hosts) || hosts == 0U || hosts > 4294967294ULL)
            return failure("Usage: cidr usable-hosts (1..4294967294)");
        std::uint64_t needed = hosts + 2U;
        unsigned host_bits = 0;
        std::uint64_t addresses = 1U;
        while (addresses < needed && host_bits < 32U) {
            addresses <<= 1U;
            ++host_bits;
        }
        const unsigned prefix = 32U - host_bits;
        return success("Smallest conventional subnet  /" + std::to_string(prefix) +
                       "\nTotal addresses  " + std::to_string(addresses) +
                       "\nUsable addresses  " + std::to_string(addresses >= 2U ? addresses - 2U : 0U));
    }
    if (fields[0] == "transfer") {
        double bytes = 0.0, bps = 0.0;
        if (fields.size()!=3U || !parse_double(fields[1], bytes) ||
            !parse_double(fields[2], bps) || bytes < 0.0 || bps <= 0.0)
            return failure("Usage: transfer bytes bits-per-second");
        const double seconds = bytes * 8.0 / bps;
        return success("Transfer time  " + number(seconds) + " s\nRate  " +
                       engineering(bps, "bit/s"));
    }
    return failure("Unknown network operation.");
}

struct StorageUnit { std::string_view name; long double bytes; };
constexpr std::array<StorageUnit, 11> kStorageUnits{{
    {"B",1.0L},{"kB",1000.0L},{"MB",1000000.0L},{"GB",1000000000.0L},
    {"TB",1000000000000.0L},{"PB",1000000000000000.0L},
    {"KiB",1024.0L},{"MiB",1048576.0L},{"GiB",1073741824.0L},
    {"TiB",1099511627776.0L},{"PiB",1125899906842624.0L}
}};
const StorageUnit* storage_unit(std::string_view name) {
    for (const auto& unit : kStorageUnits) if (unit.name == name) return &unit;
    return nullptr;
}

ToolResult storage_tool(std::string_view input) {
    const auto fields = split_ws(input);
    if (fields.empty()) return failure("Enter convert, raid or clusters.");
    if (fields[0] == "convert") {
        if (fields.size()!=4U) return failure("Usage: convert value FROM TO");
        double value = 0.0;
        if (!parse_double(fields[1], value) || value < 0.0) return failure("Invalid storage value.");
        const auto* from=storage_unit(fields[2]); const auto* to=storage_unit(fields[3]);
        if (!from||!to) return failure("Unknown storage unit.");
        const long double converted = static_cast<long double>(value)*from->bytes/to->bytes;
        std::ostringstream out; out << std::setprecision(15) << static_cast<double>(converted);
        return success(number(value)+" "+fields[2]+" = "+out.str()+" "+fields[3]);
    }
    if (fields[0] == "raid") {
        if (fields.size()!=5U) return failure("Usage: raid LEVEL disks size-per-disk unit");
        std::uint64_t disks=0; double size=0.0;
        if (!parse_u64(fields[2],disks) || !parse_double(fields[3],size) || size<0.0)
            return failure("Invalid disk count or size.");
        const auto* unit=storage_unit(fields[4]); if(!unit) return failure("Unknown storage unit.");
        long double usable_disks=0.0L;
        const std::string level=fields[1];
        if(level=="0" && disks>=2) usable_disks=static_cast<long double>(disks);
        else if(level=="1" && disks>=2) usable_disks=1.0L;
        else if(level=="5" && disks>=3) usable_disks=static_cast<long double>(disks-1U);
        else if(level=="6" && disks>=4) usable_disks=static_cast<long double>(disks-2U);
        else if(level=="10" && disks>=4 && disks%2U==0U) usable_disks=static_cast<long double>(disks/2U);
        else return failure("Unsupported/invalid RAID geometry. Levels: 0,1,5,6,10.");
        const long double bytes=usable_disks*static_cast<long double>(size)*unit->bytes;
        const long double tib=bytes/1099511627776.0L;
        std::ostringstream out; out<<std::setprecision(15)<<static_cast<double>(tib);
        return success("Usable capacity  "+out.str()+" TiB\nRaw disks  "+std::to_string(disks));
    }
    if (fields[0] == "clusters") {
        std::uint64_t file=0, cluster=0;
        if(fields.size()!=3U || !parse_u64(fields[1],file) || !parse_u64(fields[2],cluster) || cluster==0U)
            return failure("Usage: clusters file-bytes cluster-bytes");
        const std::uint64_t clusters =
            file / cluster + (file % cluster == 0U ? 0U : 1U);
        if (clusters != 0U &&
            clusters > std::numeric_limits<std::uint64_t>::max() / cluster) {
            return failure("Allocated size exceeds the 64-bit storage domain.");
        }
        const std::uint64_t allocated=clusters*cluster;
        return success("Clusters  "+std::to_string(clusters)+
                       "\nAllocated bytes  "+std::to_string(allocated)+
                       "\nSlack bytes  "+std::to_string(allocated-file));
    }
    return failure("Unknown storage operation.");
}

bool leap(int y) {
    return (y%4==0 && y%100!=0) || y%400==0;
}
int month_days(int y,int m) {
    static constexpr int days[]{31,28,31,30,31,30,31,31,30,31,30,31};
    return m==2 ? days[m-1]+(leap(y)?1:0) : days[m-1];
}
bool parse_date(std::string_view s,int& y,int& m,int& d) {
    if(s.size()!=10 || s[4]!='-' || s[7]!='-') return false;
    auto part=[&](std::size_t pos,std::size_t n,int& v){
        const auto r=std::from_chars(s.data()+pos,s.data()+pos+n,v);
        return r.ec==std::errc{} && r.ptr==s.data()+pos+n;
    };
    if(!part(0,4,y)||!part(5,2,m)||!part(8,2,d)) return false;
    return y>=1 && y<=9999 && m>=1 && m<=12 && d>=1 && d<=month_days(y,m);
}
std::int64_t days_from_civil(int y,unsigned m,unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y-399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned adjusted_month = m > 2U ? m - 3U : m + 9U;
    const unsigned doy = (153U * adjusted_month + 2U)/5U + d-1U;
    const unsigned doe = yoe*365U + yoe/4U - yoe/100U + doy;
    return static_cast<std::int64_t>(era)*146097 + static_cast<std::int64_t>(doe) - 719468;
}
void civil_from_days(std::int64_t z,int& y,unsigned& m,unsigned& d) {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe/1460U + doe/36524U - doe/146096U) / 365U;
    y = static_cast<int>(yoe) + static_cast<int>(era)*400;
    const unsigned doy = doe - (365U*yoe + yoe/4U - yoe/100U);
    const unsigned mp = (5U*doy + 2U)/153U;
    d = doy - (153U*mp+2U)/5U + 1U;
    m = mp < 10U ? mp + 3U : mp - 9U;
    y += m <= 2U;
}
std::string date_string(int y,unsigned m,unsigned d) {
    std::ostringstream out;
    out<<std::setfill('0')<<std::setw(4)<<y<<'-'<<std::setw(2)<<m<<'-'<<std::setw(2)<<d;
    return out.str();
}

ToolResult datetime_tool(std::string_view input) {
    const auto f=split_ws(input);
    if(f.empty()) return failure("Enter diff, add or unix.");
    if(f[0]=="diff") {
        int y1,m1,d1,y2,m2,d2;
        if(f.size()!=3U||!parse_date(f[1],y1,m1,d1)||!parse_date(f[2],y2,m2,d2))
            return failure("Usage: diff YYYY-MM-DD YYYY-MM-DD");
        const auto delta=days_from_civil(y2,m2,d2)-days_from_civil(y1,m1,d1);
        const auto absdays=delta<0?-delta:delta;
        return success("Days  "+std::to_string(delta)+
                       "\nAbsolute  "+std::to_string(absdays)+
                       "\nWeeks + days  "+std::to_string(absdays/7)+" + "+std::to_string(absdays%7));
    }
    if(f[0]=="add") {
        int y,m,d; std::int64_t delta=0;
        if(f.size()!=3U||!parse_date(f[1],y,m,d)) return failure("Usage: add YYYY-MM-DD days");
        const auto conv=std::from_chars(f[2].data(),f[2].data()+f[2].size(),delta);
        if(conv.ec!=std::errc{}||conv.ptr!=f[2].data()+f[2].size()) return failure("Invalid day offset.");
        int oy; unsigned om,od;
        civil_from_days(days_from_civil(y,m,d)+delta,oy,om,od);
        if(oy<1||oy>9999) return failure("Result is outside supported civil year range 1..9999.");
        return success("Date  "+date_string(oy,om,od));
    }
    if(f[0]=="unix") {
        if(f.size()!=2U) return failure("Usage: unix YYYY-MM-DDTHH:MM:SSZ");
        const std::string& t=f[1];
        if(t.size()!=20||t[10]!='T'||t[13]!=':'||t[16]!=':'||t[19]!='Z')
            return failure("Use UTC form YYYY-MM-DDTHH:MM:SSZ.");
        int y,m,d; if(!parse_date(std::string_view(t).substr(0,10),y,m,d)) return failure("Invalid date.");
        int hh=0,mm=0,ss=0;
        auto p=[&](std::size_t pos,int& v){auto r=std::from_chars(t.data()+pos,t.data()+pos+2,v);return r.ec==std::errc{}&&r.ptr==t.data()+pos+2;};
        if(!p(11,hh)||!p(14,mm)||!p(17,ss)||hh>23||mm>59||ss>59) return failure("Invalid UTC time.");
        const std::int64_t seconds=days_from_civil(y,m,d)*86400 + hh*3600 + mm*60 + ss;
        return success("Unix time  "+std::to_string(seconds)+" s");
    }
    return failure("Unknown date/time operation.");
}

ToolResult constants_tool(std::string_view input) {
    const std::string name=trim(input);
    const auto& constants=calculator::constant_catalog();
    if(name.empty()||name=="list") {
        std::ostringstream out;
        for(std::size_t i=0;i<constants.size();++i){
            if(i) out<<'\n';
            out<<constants[i].name<<"  "<<calculator::format_scientific_value(constants[i].value);
            if(!constants[i].unit.empty()) out<<' '<<constants[i].unit;
        }
        return success(out.str());
    }
    for(const auto& c:constants) {
        if(c.name==name || c.alias==name) {
            std::string out=std::string(c.name)+"  "+calculator::format_scientific_value(c.value);
            if(!c.unit.empty()) out+=" "+std::string(c.unit);
            return success(out);
        }
    }
    return failure("Unknown constant. Enter list to show the catalogue.");
}

bool parse_value_list(std::string_view input,std::vector<double>& values) {
    std::string s(input);
    std::replace(s.begin(),s.end(),',',' ');
    for(const auto& token:split_ws(s)) {
        double v=0.0; if(!parse_double(token,v)) return false;
        values.push_back(v);
    }
    return !values.empty();
}
double quantile_sorted(const std::vector<double>& v,double q) {
    if(v.size()==1U) return v[0];
    const double pos=q*static_cast<double>(v.size()-1U);
    const std::size_t lo=static_cast<std::size_t>(std::floor(pos));
    const std::size_t hi=static_cast<std::size_t>(std::ceil(pos));
    if(lo==hi) return v[lo];
    return v[lo]+(v[hi]-v[lo])*(pos-static_cast<double>(lo));
}
ToolResult statistics_tool(std::string_view input) {
    std::vector<double> v;
    if(!parse_value_list(input,v)) return failure("Enter comma or space separated finite values.");
    std::sort(v.begin(),v.end());
    const double sum=std::accumulate(v.begin(),v.end(),0.0);
    const double mean=sum/static_cast<double>(v.size());
    double sq=0.0;
    for(double x:v){const double d=x-mean;sq+=d*d;}
    const double pop=sq/static_cast<double>(v.size());
    std::ostringstream out;
    out<<"Count  "<<v.size()
       <<"\nSum  "<<number(sum)
       <<"\nMean  "<<number(mean)
       <<"\nMedian  "<<number(quantile_sorted(v,0.5))
       <<"\nQ1  "<<number(quantile_sorted(v,0.25))
       <<"\nQ3  "<<number(quantile_sorted(v,0.75))
       <<"\nMin  "<<number(v.front())
       <<"\nMax  "<<number(v.back())
       <<"\nRange  "<<number(v.back()-v.front())
       <<"\nPopulation variance  "<<number(pop)
       <<"\nPopulation stddev  "<<number(std::sqrt(pop));
    if(v.size()>1U) {
        const double sample=sq/static_cast<double>(v.size()-1U);
        out<<"\nSample variance  "<<number(sample)
           <<"\nSample stddev  "<<number(std::sqrt(sample));
    }
    return success(out.str());
}

ToolResult graph_tool(std::string_view input) {
    const auto p=split_semicolon(input);
    if(p.empty()||p[0].empty()) return failure("Enter expression ; xmin ; xmax ; samples");
    double xmin=-10.0,xmax=10.0;
    std::uint64_t samples=201;
    if(p.size()>1U&&!p[1].empty()&&!parse_double(p[1],xmin)) return failure("Invalid xmin.");
    if(p.size()>2U&&!p[2].empty()&&!parse_double(p[2],xmax)) return failure("Invalid xmax.");
    if(p.size()>3U&&!p[3].empty()&&(!parse_u64(p[3],samples)||samples<2U||samples>4096U))
        return failure("Samples must be 2..4096.");
    if(p.size()>4U) return failure("Too many graph fields.");
    if(!(xmin<xmax)) return failure("xmin must be less than xmax.");

    ToolResult result; result.ok=true;
    result.points.reserve(static_cast<std::size_t>(samples));
    std::size_t valid=0;
    double ymin=std::numeric_limits<double>::infinity();
    double ymax=-std::numeric_limits<double>::infinity();
    calculator::Variables vars;
    for(std::uint64_t i=0;i<samples;++i) {
        const double x=xmin+(xmax-xmin)*static_cast<double>(i)/static_cast<double>(samples-1U);
        vars["x"]=x;
        const auto y=calculator::evaluate(p[0],vars);
        GraphPoint pt; pt.x=x; pt.valid=y.ok&&std::isfinite(y.value);
        if(pt.valid){pt.y=y.value;++valid;ymin=std::min(ymin,pt.y);ymax=std::max(ymax,pt.y);}
        result.points.push_back(pt);
    }
    if(valid==0U) return failure("Expression produced no finite graph points in the requested range.");
    result.output="Expression  "+p[0]+
                  "\nX range  "+number(xmin)+" .. "+number(xmax)+
                  "\nValid samples  "+std::to_string(valid)+"/"+std::to_string(samples)+
                  "\nY range  "+number(ymin)+" .. "+number(ymax);
    return result;
}

double eval_x(std::string_view expr,double x,bool& ok) {
    calculator::Variables vars; vars["x"]=x;
    const auto r=calculator::evaluate(std::string(expr),vars);
    ok=r.ok&&std::isfinite(r.value); return ok?r.value:0.0;
}
void add_root(std::vector<double>& roots,double x) {
    for(double r:roots) if(std::fabs(r-x)<=1e-9*std::max({1.0,std::fabs(r),std::fabs(x)})) return;
    roots.push_back(x);
}
ToolResult equation_tool(std::string_view input) {
    const auto p=split_semicolon(input);
    if(p.empty()||p[0].empty()||p.size()>3U) return failure("Usage: expression ; xmin ; xmax");
    double xmin=-100.0,xmax=100.0;
    if(p.size()>1U&&!p[1].empty()&&!parse_double(p[1],xmin)) return failure("Invalid xmin.");
    if(p.size()>2U&&!p[2].empty()&&!parse_double(p[2],xmax)) return failure("Invalid xmax.");
    if(!(xmin<xmax)) return failure("xmin must be less than xmax.");

    constexpr int segments=2048;
    std::vector<double> roots;
    double x0=xmin; bool ok0=false; double y0=eval_x(p[0],x0,ok0);
    if(ok0&&std::fabs(y0)<1e-12) add_root(roots,x0);
    for(int i=1;i<=segments;++i){
        const double x1=xmin+(xmax-xmin)*static_cast<double>(i)/segments;
        bool ok1=false; const double y1=eval_x(p[0],x1,ok1);
        if(ok1&&std::fabs(y1)<1e-12) add_root(roots,x1);
        if(ok0&&ok1&&std::signbit(y0)!=std::signbit(y1)){
            double lo=x0,hi=x1,flo=y0,fhi=y1;
            for(int n=0;n<80;++n){
                const double mid=(lo+hi)/2.0; bool okm=false; const double fm=eval_x(p[0],mid,okm);
                if(!okm) break;
                if(std::fabs(fm)<1e-14){lo=hi=mid;flo=fhi=fm;break;}
                if(std::signbit(flo)!=std::signbit(fm)){hi=mid;fhi=fm;}
                else{lo=mid;flo=fm;}
            }
            const double root=(lo+hi)/2.0; bool okr=false; const double fr=eval_x(p[0],root,okr);
            if(okr&&std::fabs(fr)<=1e-7) add_root(roots,root);
        }
        x0=x1;y0=y1;ok0=ok1;
    }
    std::sort(roots.begin(),roots.end());
    if(roots.empty()) return failure("No validated real root was found in the requested interval.");
    std::ostringstream out; out<<"Roots ("<<roots.size()<<")";
    for(double root:roots) out<<"\n"<<number(root);
    return success(out.str());
}

class BigInt {
public:
    BigInt()=default;
    explicit BigInt(std::int64_t value){ if(value<0){negative_=true; value=-value;} while(value){limbs_.push_back(static_cast<std::uint32_t>(value%kBase));value/=kBase;} }
    static bool parse(std::string_view digits,BigInt& out) {
        if(digits.empty()) return false;
        out=BigInt();
        for(char ch:digits){
            if(ch<'0'||ch>'9') return false;
            out.mul_small(10U); out.add_small(static_cast<unsigned>(ch-'0'));
            if(out.decimal_digits()>20000U) return false;
        }
        return true;
    }
    bool zero() const{return limbs_.empty();}
    bool negative() const{return negative_&&!zero();}
    void negate(){if(!zero())negative_=!negative_;}
    BigInt absolute() const{BigInt x=*this;x.negative_=false;return x;}
    std::string str() const{
        if(zero())return"0";
        std::ostringstream out;if(negative())out<<'-';
        out<<limbs_.back();
        for(std::size_t i=limbs_.size()-1;i>0;--i)out<<std::setfill('0')<<std::setw(9)<<limbs_[i-1];
        return out.str();
    }
    std::size_t decimal_digits() const {
        if(zero())return 1U;
        std::size_t n=(limbs_.size()-1U)*9U;std::uint32_t top=limbs_.back();
        do{++n;top/=10U;}while(top); return n;
    }
    bool divisible_by_10() const {
        return !zero() && (limbs_.front() % 10U) == 0U;
    }
    void divide_small(std::uint32_t divisor) {
        if (divisor == 0U) return;
        std::uint64_t remainder = 0U;
        for (std::size_t i = limbs_.size(); i > 0; --i) {
            const std::uint64_t current =
                remainder * kBase + limbs_[i - 1U];
            limbs_[i - 1U] =
                static_cast<std::uint32_t>(current / divisor);
            remainder = current % divisor;
        }
        trim();
    }
    bool to_u32(std::uint32_t& out) const {
        if(negative()||limbs_.size()>2U)return false;
        std::uint64_t v=0;for(std::size_t i=limbs_.size();i>0;--i)v=v*kBase+limbs_[i-1];
        if(v>std::numeric_limits<std::uint32_t>::max())return false;out=static_cast<std::uint32_t>(v);return true;
    }
    friend BigInt operator+(const BigInt&a,const BigInt&b){
        if(a.negative_==b.negative_){BigInt r=add_abs(a,b);r.negative_=a.negative_;return r;}
        const int c=cmp_abs(a,b);if(c==0)return BigInt();
        if(c>0){BigInt r=sub_abs(a,b);r.negative_=a.negative_;return r;}
        BigInt r=sub_abs(b,a);r.negative_=b.negative_;return r;
    }
    friend BigInt operator-(BigInt a,const BigInt&b){BigInt n=b;n.negate();return a+n;}
    friend BigInt operator*(const BigInt&a,const BigInt&b){
        if(a.zero()||b.zero())return BigInt();
        BigInt r;r.limbs_.assign(a.limbs_.size()+b.limbs_.size(),0U);
        for(std::size_t i=0;i<a.limbs_.size();++i){
            std::uint64_t carry=0;
            for(std::size_t j=0;j<b.limbs_.size()||carry;++j){
                const std::uint64_t cur=r.limbs_[i+j]+carry+
                    (j<b.limbs_.size()?static_cast<std::uint64_t>(a.limbs_[i])*b.limbs_[j]:0U);
                r.limbs_[i+j]=static_cast<std::uint32_t>(cur%kBase);carry=cur/kBase;
            }
        }
        r.negative_=a.negative_!=b.negative_;r.trim();return r;
    }
    static BigInt pow(BigInt base,std::uint32_t exp,bool& ok){
        BigInt result(1);ok=true;
        while(exp){
            if(exp&1U){result=result*base;if(result.decimal_digits()>20000U){ok=false;return {};}}
            exp>>=1U;if(exp){base=base*base;if(base.decimal_digits()>20000U){ok=false;return {};}}
        }return result;
    }
private:
    static constexpr std::uint64_t kBase=1000000000ULL;
    std::vector<std::uint32_t> limbs_;
    bool negative_=false;
    void trim(){while(!limbs_.empty()&&limbs_.back()==0U)limbs_.pop_back();if(limbs_.empty())negative_=false;}
    void mul_small(std::uint32_t m){std::uint64_t carry=0;for(auto&x:limbs_){const std::uint64_t v=static_cast<std::uint64_t>(x)*m+carry;x=static_cast<std::uint32_t>(v%kBase);carry=v/kBase;}if(carry)limbs_.push_back(static_cast<std::uint32_t>(carry));}
    void add_small(std::uint32_t a){std::uint64_t carry=a;std::size_t i=0;while(carry){if(i==limbs_.size())limbs_.push_back(0);const std::uint64_t v=limbs_[i]+carry;limbs_[i]=static_cast<std::uint32_t>(v%kBase);carry=v/kBase;++i;}}
    static int cmp_abs(const BigInt&a,const BigInt&b){if(a.limbs_.size()!=b.limbs_.size())return a.limbs_.size()<b.limbs_.size()?-1:1;for(std::size_t i=a.limbs_.size();i>0;--i)if(a.limbs_[i-1]!=b.limbs_[i-1])return a.limbs_[i-1]<b.limbs_[i-1]?-1:1;return 0;}
    static BigInt add_abs(const BigInt&a,const BigInt&b){BigInt r;const std::size_t n=std::max(a.limbs_.size(),b.limbs_.size());r.limbs_.resize(n);std::uint64_t carry=0;for(std::size_t i=0;i<n;++i){const std::uint64_t v=carry+(i<a.limbs_.size()?a.limbs_[i]:0U)+(i<b.limbs_.size()?b.limbs_[i]:0U);r.limbs_[i]=static_cast<std::uint32_t>(v%kBase);carry=v/kBase;}if(carry)r.limbs_.push_back(static_cast<std::uint32_t>(carry));return r;}
    static BigInt sub_abs(const BigInt&a,const BigInt&b){BigInt r;r.limbs_.resize(a.limbs_.size());std::int64_t borrow=0;for(std::size_t i=0;i<a.limbs_.size();++i){std::int64_t v=static_cast<std::int64_t>(a.limbs_[i])-borrow-(i<b.limbs_.size()?b.limbs_[i]:0U);if(v<0){v+=static_cast<std::int64_t>(kBase);borrow=1;}else borrow=0;r.limbs_[i]=static_cast<std::uint32_t>(v);}r.trim();return r;}
};

BigInt power10(std::size_t n,bool& ok){
    BigInt ten(10);return BigInt::pow(ten,static_cast<std::uint32_t>(n),ok);
}

struct Exact {BigInt n;BigInt d=BigInt(1);};

class ExactParser {
public:
    explicit ExactParser(std::string_view in):in_(in){}
    ToolResult run(){
        Exact v=expr();skip();
        if(!error_.empty())return failure(error_);
        if(pos_!=in_.size())return failure("Unexpected input in exact-decimal expression.");
        if (v.n.zero()) {
            v.d = BigInt(1);
        } else {
            while (v.n.divisible_by_10() && v.d.divisible_by_10()) {
                v.n.divide_small(10U);
                v.d.divide_small(10U);
            }
        }
        std::string frac=v.n.str()+"/"+v.d.str();
        std::string decimal;
        const std::string ds=v.d.str();
        if(ds.size()>=2U&&ds[0]=='1'&&ds.find_first_not_of('0',1)==std::string::npos){
            std::string ns=v.n.str();bool neg=!ns.empty()&&ns[0]=='-';if(neg)ns.erase(ns.begin());
            const std::size_t scale=ds.size()-1U;
            if(ns.size()<=scale)ns.insert(0,scale+1U-ns.size(),'0');
            ns.insert(ns.size()-scale,1,'.');
            while(ns.size()>1U&&ns.back()=='0')ns.pop_back();
            if(!ns.empty()&&ns.back()=='.')ns.pop_back();
            if(neg&&ns!="0")ns.insert(ns.begin(),'-');
            decimal=ns;
        } else if(ds=="1") decimal=v.n.str();
        std::string out="Exact fraction  "+frac;
        if(!decimal.empty())out+="\nExact decimal  "+decimal;
        return success(out);
    }
private:
    std::string_view in_;std::size_t pos_=0;std::string error_;
    void skip(){while(pos_<in_.size()&&std::isspace(static_cast<unsigned char>(in_[pos_])))++pos_;}
    bool take(char c){skip();if(pos_<in_.size()&&in_[pos_]==c){++pos_;return true;}return false;}
    Exact expr(){Exact a=term();while(error_.empty()){if(take('+')){Exact b=term();a={a.n*b.d+b.n*a.d,a.d*b.d};}else if(take('-')){Exact b=term();a={a.n*b.d-b.n*a.d,a.d*b.d};}else break;}return a;}
    Exact term(){Exact a=unary();while(error_.empty()){if(take('*')){Exact b=unary();a={a.n*b.n,a.d*b.d};}else if(take('/')){Exact b=unary();if(b.n.zero()){error_="Division by zero.";return{};}BigInt bn=b.n.absolute();BigInt n=a.n*b.d;if(b.n.negative())n.negate();a={n,a.d*bn};}else break;if(a.n.decimal_digits()+a.d.decimal_digits()>20000U){error_="Exact result exceeds the 20,000-digit safety limit.";}}return a;}
    Exact unary(){if(take('+'))return unary();if(take('-')){Exact a=unary();a.n.negate();return a;}return primary();}
    Exact primary(){
        if(take('(')){Exact v=expr();if(!take(')')&&error_.empty())error_="Missing closing parenthesis.";return v;}
        skip();const std::size_t start=pos_;bool dot=false;std::size_t fractional=0;
        while(pos_<in_.size()){
            char c=in_[pos_];if(c>='0'&&c<='9'){if(dot)++fractional;++pos_;}
            else if(c=='.'&&!dot){dot=true;++pos_;}else break;
        }
        if(pos_==start){error_="Expected a decimal literal.";return{};}
        std::string token(in_.substr(start,pos_-start));token.erase(std::remove(token.begin(),token.end(),'.'),token.end());
        if(token.empty()){error_="Invalid decimal literal.";return{};}
        BigInt n;if(!BigInt::parse(token,n)){error_="Invalid or oversized decimal literal.";return{};}
        int exponent=0;
        if(pos_<in_.size()&&(in_[pos_]=='e'||in_[pos_]=='E')){
            ++pos_;bool neg=false;if(pos_<in_.size()&&(in_[pos_]=='+'||in_[pos_]=='-')){neg=in_[pos_]=='-';++pos_;}
            const std::size_t es=pos_;while(pos_<in_.size()&&std::isdigit(static_cast<unsigned char>(in_[pos_])))++pos_;
            if(es==pos_){error_="Malformed decimal exponent.";return{};}
            auto r=std::from_chars(in_.data()+es,in_.data()+pos_,exponent);if(r.ec!=std::errc{}||exponent>4096){error_="Decimal exponent exceeds safety limit.";return{};}if(neg)exponent=-exponent;
        }
        long long scale=static_cast<long long>(fractional)-exponent;bool ok=true;
        if(scale>=0){BigInt d=power10(static_cast<std::size_t>(scale),ok);if(!ok){error_="Decimal scale exceeds safety limit.";return{};}return{n,d};}
        BigInt mul=power10(static_cast<std::size_t>(-scale),ok);if(!ok){error_="Decimal scale exceeds safety limit.";return{};}return{n*mul,BigInt(1)};
    }
};

class IntegerParser {
public:
    explicit IntegerParser(std::string_view in):in_(in){}
    ToolResult run(){BigInt v=expr();skip();if(!err_.empty())return failure(err_);if(pos_!=in_.size())return failure("Unexpected arbitrary-precision input.");return success(v.str());}
private:
    std::string_view in_;std::size_t pos_=0;std::string err_;
    void skip(){while(pos_<in_.size()&&std::isspace(static_cast<unsigned char>(in_[pos_])))++pos_;}
    bool take(char c){skip();if(pos_<in_.size()&&in_[pos_]==c){++pos_;return true;}return false;}
    BigInt expr(){BigInt a=term();while(err_.empty()){if(take('+'))a=a+term();else if(take('-'))a=a-term();else break;}return a;}
    BigInt term(){BigInt a=power();while(err_.empty()&&take('*')){a=a*power();if(a.decimal_digits()>20000U)err_="Result exceeds the 20,000-digit safety limit.";}return a;}
    BigInt power(){BigInt a=unary();if(err_.empty()&&take('^')){BigInt e=power();std::uint32_t exp=0;if(!e.to_u32(exp)||exp>100000U){err_="Exponent must be a non-negative integer <= 100000.";return{};}bool ok=false;a=BigInt::pow(a,exp,ok);if(!ok)err_="Result exceeds the 20,000-digit safety limit.";}while(err_.empty()&&take('!')){std::uint32_t n=0;if(!a.to_u32(n)||n>1000U){err_="Factorial requires an integer from 0 to 1000.";return{};}BigInt r(1);for(std::uint32_t i=2;i<=n;++i)r=r*BigInt(i);a=r;}return a;}
    BigInt unary(){if(take('+'))return unary();if(take('-')){BigInt v=unary();v.negate();return v;}return primary();}
    BigInt primary(){if(take('(')){BigInt v=expr();if(!take(')')&&err_.empty())err_="Missing closing parenthesis.";return v;}skip();const std::size_t start=pos_;while(pos_<in_.size()&&std::isdigit(static_cast<unsigned char>(in_[pos_])))++pos_;if(start==pos_){err_="Expected an integer.";return{};}BigInt v;if(!BigInt::parse(in_.substr(start,pos_-start),v))err_="Invalid or oversized integer.";return v;}
};

bool parse_complex_pair(std::string_view s,double& re,double& im) {
    const std::size_t comma=s.find(',');if(comma==std::string_view::npos)return false;
    return parse_double(s.substr(0,comma),re)&&parse_double(s.substr(comma+1),im);
}
std::string complex_string(double re,double im){
    std::string out=number(re);out+=im<0.0?" - ":" + ";out+=number(std::fabs(im));out+="i";return out;
}
ToolResult complex_tool(std::string_view input) {
    const auto f=split_ws(input);if(f.empty())return failure("Enter a complex operation.");
    double ar=0,ai=0,br=0,bi=0;
    if(f[0]=="conj"||f[0]=="abs"||f[0]=="arg"||f[0]=="polar"){
        if(f.size()!=2U||!parse_complex_pair(f[1],ar,ai))return failure("Usage: conj|abs|arg|polar real,imag");
        if(f[0]=="conj")return success(complex_string(ar,-ai));
        const double mag=std::hypot(ar,ai);
        if(f[0]=="abs")return success("Magnitude  "+number(mag));
        const double angle=std::atan2(ai,ar);
        if(f[0]=="arg")return success("Argument  "+number(angle)+" rad");
        return success("Magnitude  "+number(mag)+"\nPhase  "+number(angle)+" rad");
    }
    if(f.size()!=3U||!parse_complex_pair(f[1],ar,ai)||!parse_complex_pair(f[2],br,bi))
        return failure("Usage: add|sub|mul|div real,imag real,imag");
    double rr=0,ri=0;
    if(f[0]=="add"){rr=ar+br;ri=ai+bi;}
    else if(f[0]=="sub"){rr=ar-br;ri=ai-bi;}
    else if(f[0]=="mul"){rr=ar*br-ai*bi;ri=ar*bi+ai*br;}
    else if(f[0]=="div"){const double d=br*br+bi*bi;if(d==0.0)return failure("Complex division by zero.");rr=(ar*br+ai*bi)/d;ri=(ai*br-ar*bi)/d;}
    else return failure("Unknown complex operation.");
    return success(complex_string(rr,ri)+"\nMagnitude  "+number(std::hypot(rr,ri))+"\nPhase  "+number(std::atan2(ri,rr))+" rad");
}

} // namespace

const std::array<ToolDescriptor, 12>& catalog() noexcept { return kCatalog; }

const ToolDescriptor& descriptor(AdvancedTool tool) noexcept {
    const std::size_t index=static_cast<std::size_t>(tool);
    return kCatalog[index<kCatalog.size()?index:0U];
}

ToolResult evaluate(AdvancedTool tool,std::string_view input) {
    switch(tool){
    case AdvancedTool::Engineering:return engineering_tool(input);
    case AdvancedTool::UnitConversion:return unit_tool(input);
    case AdvancedTool::Network:return network_tool(input);
    case AdvancedTool::Storage:return storage_tool(input);
    case AdvancedTool::DateTime:return datetime_tool(input);
    case AdvancedTool::Constants:return constants_tool(input);
    case AdvancedTool::Statistics:return statistics_tool(input);
    case AdvancedTool::Graph:return graph_tool(input);
    case AdvancedTool::EquationSolver:return equation_tool(input);
    case AdvancedTool::ExactDecimal:return ExactParser(input).run();
    case AdvancedTool::ArbitraryPrecision:return IntegerParser(input).run();
    case AdvancedTool::Complex:return complex_tool(input);
    }
    return failure("Unknown advanced tool.");
}

} // namespace calculator::tools
