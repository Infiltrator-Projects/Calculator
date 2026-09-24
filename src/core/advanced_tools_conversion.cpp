/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "advanced_tools_internal.hpp"

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace calculator::tools::detail {

// Engineering, conversion, network, storage and civil-time tools live together because they share bounded numeric parsing and unit/capacity mechanics.
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
        if (!parse_double_range(fields[3], 0.0, 1.0, pf)) {
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
    // Scientific expressions preserve exact decimal definitions and ratios
    // until the conversion is actually evaluated.
    std::string_view factor;
    std::string_view offset;
};

constexpr std::array<Unit, 135> kUnits{{
    {"m","length","1.0","0.0"},{"km","length","1000.0","0.0"},{"cm","length","0.01","0.0"},{"mm","length","0.001","0.0"},
    {"um","length","1e-6","0.0"},{"nm","length","1e-9","0.0"},{"in","length","0.0254","0.0"},{"ft","length","0.3048","0.0"},
    {"yd","length","0.9144","0.0"},{"mi","length","1609.344","0.0"},{"nmi","length","1852.0","0.0"},
    {"pc","length","3.0856775814913673e16","0.0"},{"ly","length","9.4607304725808e15","0.0"},
    {"au","length","149597870700.0","0.0"},{"U","length","0.04445","0.0"},{"cable","length","219.456","0.0"},
    {"fathom","length","1.8288","0.0"},{"pt","length","0.0003527777777777778","0.0"},

    {"kg","mass","1.0","0.0"},{"g","mass","0.001","0.0"},{"mg","mass","1e-6","0.0"},{"lb","mass","0.45359237","0.0"},
    {"oz","mass","0.028349523125","0.0"},{"t","mass","1000.0","0.0"},{"ozt","mass","0.0311034768","0.0"},
    {"st","mass","6.35029318","0.0"},

    {"C","temperature","1.0","273.15"},{"F","temperature","5.0/9.0","273.15-32.0*5.0/9.0"},{"K","temperature","1.0","0.0"},
    {"R","temperature","5.0/9.0","0.0"},

    {"m2","area","1.0","0.0"},{"km2","area","1e6","0.0"},{"cm2","area","1e-4","0.0"},{"ft2","area","0.09290304","0.0"},
    {"in2","area","0.00064516","0.0"},{"yd2","area","0.83612736","0.0"},{"mi2","area","2589988.110336","0.0"},
    {"acre","area","4046.8564224","0.0"},{"ha","area","10000.0","0.0"},

    {"m3","volume","1.0","0.0"},{"L","volume","0.001","0.0"},{"mL","volume","1e-6","0.0"},{"uL","volume","1e-9","0.0"},
    {"cupMetric","volume","0.00025","0.0"},{"galUS","volume","0.003785411784","0.0"},
    {"galUK","volume","0.00454609","0.0"},{"ft3","volume","0.028316846592","0.0"},{"in3","volume","0.000016387064","0.0"},
    {"cupUS","volume","0.0002365882365","0.0"},{"pintUS","volume","0.000473176473","0.0"},
    {"quartUS","volume","0.000946352946","0.0"},{"flozUS","volume","0.0000295735295625","0.0"},
    {"tbspUS","volume","0.00001478676478125","0.0"},{"tspUS","volume","0.00000492892159375","0.0"},
    {"pintUK","volume","0.00056826125","0.0"},{"quartUK","volume","0.0011365225","0.0"},

    {"mps","speed","1.0","0.0"},{"kph","speed","1.0/3.6","0.0"},{"mph","speed","0.44704","0.0"},
    {"knot","speed","0.5144444444444445","0.0"},{"fps","speed","0.3048","0.0"},

    {"Pa","pressure","1.0","0.0"},{"kPa","pressure","1000.0","0.0"},{"MPa","pressure","1e6","0.0"},{"bar","pressure","100000.0","0.0"},
    {"psi","pressure","6894.757293168","0.0"},{"atm","pressure","101325.0","0.0"},
    {"mmHg","pressure","133.322387415","0.0"},{"Torr","pressure","133.32236842105263","0.0"},

    {"J","energy","1.0","0.0"},{"kJ","energy","1000.0","0.0"},{"Wh","energy","3600.0","0.0"},{"kWh","energy","3.6e6","0.0"},
    {"cal","energy","4.184","0.0"},{"kcal","energy","4184.0","0.0"},{"BTU","energy","1055.05585262","0.0"},
    {"eV","energy","1.602176634e-19","0.0"},{"erg","energy","1e-7","0.0"},{"ftlb","energy","1.3558179483314004","0.0"},

    {"W","power","1.0","0.0"},{"kW","power","1000.0","0.0"},{"hp","power","745.6998715822702","0.0"},
    {"BTUmin","power","17.584264210333333","0.0"},

    {"century","duration","3155760000.0","0.0"},{"decade","duration","315576000.0","0.0"},
    {"yr","duration","31557600.0","0.0"},{"month","duration","2629800.0","0.0"},{"week","duration","604800.0","0.0"},
    {"day","duration","86400.0","0.0"},{"h","duration","3600.0","0.0"},{"min","duration","60.0","0.0"},
    {"s","duration","1.0","0.0"},{"ms","duration","1e-3","0.0"},{"us","duration","1e-6","0.0"},{"ns","duration","1e-9","0.0"},

    {"Hz","frequency","1.0","0.0"},{"kHz","frequency","1e3","0.0"},{"MHz","frequency","1e6","0.0"},
    {"GHz","frequency","1e9","0.0"},{"THz","frequency","1e12","0.0"},

    // GNOME Calculator 41.1 / Linux Mint digital-storage conversion family.
    // Byte is the canonical internal unit; decimal and IEC prefixes remain
    // distinct so the GUI never silently conflates kB with KiB.
    {"bit","digital-storage","0.125","0.0"},{"byte","digital-storage","1.0","0.0"},
    {"nibble","digital-storage","0.5","0.0"},
    {"kb","digital-storage","125.0","0.0"},{"kB","digital-storage","1000.0","0.0"},
    {"Kib","digital-storage","128.0","0.0"},{"KiB","digital-storage","1024.0","0.0"},
    {"Mb","digital-storage","125000.0","0.0"},{"MB","digital-storage","1000000.0","0.0"},
    {"Mib","digital-storage","131072.0","0.0"},{"MiB","digital-storage","1048576.0","0.0"},
    {"Gb","digital-storage","125000000.0","0.0"},{"GB","digital-storage","1000000000.0","0.0"},
    {"Gib","digital-storage","134217728.0","0.0"},{"GiB","digital-storage","1073741824.0","0.0"},
    {"Tb","digital-storage","125000000000.0","0.0"},{"TB","digital-storage","1000000000000.0","0.0"},
    {"Tib","digital-storage","137438953472.0","0.0"},{"TiB","digital-storage","1099511627776.0","0.0"},
    {"Pb","digital-storage","125000000000000.0","0.0"},{"PB","digital-storage","1000000000000000.0","0.0"},
    {"Pib","digital-storage","140737488355328.0","0.0"},{"PiB","digital-storage","1125899906842624.0","0.0"},
    {"Eb","digital-storage","1.25e17","0.0"},{"EB","digital-storage","1.0e18","0.0"},
    {"Eib","digital-storage","1.44115188075855872e17","0.0"},{"EiB","digital-storage","1.152921504606846976e18","0.0"},
    {"Zb","digital-storage","1.25e20","0.0"},{"ZB","digital-storage","1.0e21","0.0"},
    {"Zib","digital-storage","1.47573952589676412928e20","0.0"},{"ZiB","digital-storage","1.180591620717411303424e21","0.0"},
    {"Yb","digital-storage","1.25e23","0.0"},{"YB","digital-storage","1.0e24","0.0"},
    {"Yib","digital-storage","1.51115727451828646838272e23","0.0"},{"YiB","digital-storage","1.208925819614629174706176e24","0.0"}
}};

constexpr std::array<Unit, 3> kAngleUnits{{
    {"deg","angle","pi/180.0","0.0"},
    {"rad","angle","1.0","0.0"},
    {"grad","angle","pi/200.0","0.0"}
}};

std::string_view canonical_unit_name(std::string_view name) {
    // Storage historically exposed IEC/SI bit spellings such as Kibit while
    // Unit Conversion uses compact Kib/kb identifiers. Keep those spellings as
    // aliases, but resolve them to one canonical unit definition.
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 17>
        aliases{{
            {"B","byte"},
            {"kbit","kb"},{"Kibit","Kib"},
            {"Mbit","Mb"},{"Mibit","Mib"},
            {"Gbit","Gb"},{"Gibit","Gib"},
            {"Tbit","Tb"},{"Tibit","Tib"},
            {"Pbit","Pb"},{"Pibit","Pib"},
            {"Ebit","Eb"},{"Eibit","Eib"},
            {"Zbit","Zb"},{"Zibit","Zib"},
            {"Ybit","Yb"},{"Yibit","Yib"}
        }};
    for (const auto& [alias, canonical] : aliases) {
        if (name == alias) return canonical;
    }
    return name;
}

const Unit* find_unit(std::string_view name) {
    name = canonical_unit_name(name);
    for (const auto& unit : kUnits) if (unit.name == name) return &unit;
    for (const auto& unit : kAngleUnits) if (unit.name == name) return &unit;
    return nullptr;
}

ToolResult convert_units_precise(
    std::string_view value_text,
    std::string_view from_name,
    std::string_view to_name,
    std::string_view display_from = {},
    std::string_view display_to = {}) {
    const Unit* from = find_unit(from_name);
    const Unit* to = find_unit(to_name);
    if (!from || !to) return failure("Unknown unit. See the Unit conversion prompt.");
    if (from->dimension != to->dimension) return failure("Units belong to different dimensions.");

    const ScientificResult source = calculator::evaluate_scientific(
        std::string(value_text), {}, {}, AngleUnit::Radians,
        kToolScientificDigits);
    if (!source.ok || !source.display.empty() || source.value.imag != "0") {
        return failure("Invalid real numeric value.");
    }

    const std::string value_expression =
        calculator::scientific_value_expression(source.value);
    const std::string expression =
        "((" + value_expression + ")*(" + std::string(from->factor) +
        ")+(" + std::string(from->offset) + ")-(" +
        std::string(to->offset) + "))/(" + std::string(to->factor) + ")";

    const ScientificResult converted = calculator::evaluate_scientific(
        expression, {}, {}, AngleUnit::Radians, kToolScientificDigits);
    if (!converted.ok || !converted.display.empty() ||
        converted.value.imag != "0") {
        return failure(
            converted.error.empty()
                ? "Unit conversion produced an invalid result."
                : converted.error);
    }

    if (display_from.empty()) display_from = from->name;
    if (display_to.empty()) display_to = to->name;
    return success(
        calculator::format_scientific_value(source.value, 25U) + " " +
        std::string(display_from) + " = " +
        calculator::format_scientific_value(converted.value, 25U) + " " +
        std::string(display_to));
}

ToolResult unit_tool(std::string_view input) {
    const auto fields = split_ws(input);
    if (fields.size() != 3U) return failure("Usage: value FROM TO");
    return convert_units_precise(fields[0], fields[1], fields[2]);
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
        std::uint64_t value = 0;
        if (!parse_u64_range(token, 0U, 255U, value)) return false;
        out = (out << 8U) | static_cast<std::uint32_t>(value);
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
        std::uint64_t parsed_prefix = 0;
        const std::string_view p(fields[1].data() + slash + 1U, fields[1].size() - slash - 1U);
        if (!parse_u64_range(p, 0U, 32U, parsed_prefix))
            return failure("IPv4 prefix must be 0..32.");
        const unsigned prefix = static_cast<unsigned>(parsed_prefix);
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
        if (fields.size()!=2U ||
            !parse_u64_range(fields[1], 1U, 4294967294ULL, hosts))
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

ToolResult storage_tool(std::string_view input) {
    const auto fields = split_ws(input);
    if (fields.empty()) return failure("Enter convert, raid or clusters.");
    if (fields[0] == "convert") {
        if (fields.size()!=4U) return failure("Usage: convert value FROM TO");
        const ScientificResult source = calculator::evaluate_scientific(
            fields[1], {}, {}, AngleUnit::Radians, kToolScientificDigits);
        if (!source.ok || !source.display.empty() || source.value.imag != "0" ||
            (!source.value.real.empty() && source.value.real.front() == '-')) {
            return failure("Invalid storage value.");
        }
        const Unit* from = find_unit(fields[2]);
        const Unit* to = find_unit(fields[3]);
        if (!from || !to ||
            from->dimension != "digital-storage" ||
            to->dimension != "digital-storage") {
            return failure("Unknown storage unit.");
        }
        return convert_units_precise(
            fields[1], fields[2], fields[3], fields[2], fields[3]);
    }
    if (fields[0] == "raid") {
        if (fields.size()!=5U) return failure("Usage: raid LEVEL disks size-per-disk unit");
        std::uint64_t disks=0;
        if (!parse_u64(fields[2],disks)) {
            return failure("Invalid disk count or size.");
        }
        const ScientificResult size = calculator::evaluate_scientific(
            fields[3], {}, {}, AngleUnit::Radians, kToolScientificDigits);
        if (!size.ok || !size.display.empty() || size.value.imag != "0" ||
            (!size.value.real.empty() && size.value.real.front() == '-')) {
            return failure("Invalid disk count or size.");
        }
        const Unit* unit=find_unit(fields[4]);
        const Unit* tib=find_unit("TiB");
        if (!unit || unit->dimension != "digital-storage" || !tib) {
            return failure("Unknown storage unit.");
        }

        std::uint64_t usable_disks=0U;
        const std::string level=fields[1];
        if(level=="0" && disks>=2) usable_disks=disks;
        else if(level=="1" && disks>=2) usable_disks=1U;
        else if(level=="5" && disks>=3) usable_disks=disks-1U;
        else if(level=="6" && disks>=4) usable_disks=disks-2U;
        else if(level=="10" && disks>=4 && disks%2U==0U) usable_disks=disks/2U;
        else return failure("Unsupported/invalid RAID geometry. Levels: 0,1,5,6,10.");

        const std::string expression =
            "((" + calculator::scientific_value_expression(size.value) +
            ")*(" + std::to_string(usable_disks) + ")*(" +
            std::string(unit->factor) + "))/(" +
            std::string(tib->factor) + ")";
        const ScientificResult capacity = calculator::evaluate_scientific(
            expression, {}, {}, AngleUnit::Radians, kToolScientificDigits);
        if (!capacity.ok || !capacity.display.empty() ||
            capacity.value.imag != "0") {
            return failure("RAID capacity calculation failed.");
        }
        return success(
            "Usable capacity  " +
            calculator::format_scientific_value(capacity.value, 25U) +
            " TiB\nRaw disks  " + std::to_string(disks));
    }
    if (fields[0] == "clusters") {
        std::uint64_t file=0, cluster=0;
        if(fields.size()!=3U || !parse_u64(fields[1],file) || !parse_u64(fields[2],cluster) || cluster==0U)
            return failure("Usage: clusters file-bytes cluster-bytes");
        const std::uint64_t clusters =
            file / cluster + (file % cluster == 0U ? 0U : 1U);
        std::uint64_t allocated = 0U;
        if (!infiltratr_u64_multiply_checked(clusters, cluster, &allocated)) {
            return failure("Allocated size exceeds the 64-bit storage domain.");
        }
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
    auto part=[&](std::size_t pos,std::size_t n,
                   std::uint64_t minimum,std::uint64_t maximum,int& v){
        std::uint64_t parsed = 0;
        if (!parse_u64_range(
                s.substr(pos, n), minimum, maximum, parsed)) {
            return false;
        }
        v = static_cast<int>(parsed);
        return true;
    };
    if(!part(0,4,1U,9999U,y)||
       !part(5,2,1U,12U,m)||
       !part(8,2,1U,31U,d)) return false;
    return d<=month_days(y,m);
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

bool shift_years_clamped(int& y, int& m, int& d, std::int64_t years) {
    std::int64_t shifted = 0;
    if (!infiltratr_i64_add_checked(
            static_cast<std::int64_t>(y), years, &shifted) ||
        shifted < 1 || shifted > 9999) {
        return false;
    }
    y = static_cast<int>(shifted);
    d = std::min(d, month_days(y, m));
    return true;
}

bool shift_months_clamped(int& y, int& m, int& d, std::int64_t months) {
    const std::int64_t current =
        (static_cast<std::int64_t>(y) - 1) * 12 + (m - 1);
    std::int64_t shifted = 0;
    if (!infiltratr_i64_add_checked(current, months, &shifted) ||
        shifted < 0 || shifted >= 9999LL * 12LL) {
        return false;
    }
    y = static_cast<int>(shifted / 12) + 1;
    m = static_cast<int>(shifted % 12) + 1;
    d = std::min(d, month_days(y, m));
    return true;
}

bool shift_days_checked(int& y, int& m, int& d, std::int64_t days) {
    const std::int64_t current = days_from_civil(
        y, static_cast<unsigned>(m), static_cast<unsigned>(d));
    std::int64_t shifted = 0;
    if (!infiltratr_i64_add_checked(current, days, &shifted)) return false;
    int oy = 0;
    unsigned om = 0;
    unsigned od = 0;
    civil_from_days(shifted, oy, om, od);
    if (oy < 1 || oy > 9999) return false;
    y = oy;
    m = static_cast<int>(om);
    d = static_cast<int>(od);
    return true;
}

bool checked_negate(std::int64_t value, std::int64_t& output) {
    if (value == std::numeric_limits<std::int64_t>::min()) return false;
    output = -value;
    return true;
}

bool checked_week_days(std::int64_t weeks, std::int64_t& days) {
    if (weeks > std::numeric_limits<std::int64_t>::max() / 7 ||
        weeks < std::numeric_limits<std::int64_t>::min() / 7) {
        return false;
    }
    days = weeks * 7;
    return true;
}

bool parse_calendar_offsets(
    const std::vector<std::string>& fields,
    std::int64_t& years, std::int64_t& months, std::int64_t& days) {
    if (fields.size() < 4U || ((fields.size() - 2U) % 2U) != 0U) {
        return false;
    }
    for (std::size_t i = 2U; i < fields.size(); i += 2U) {
        std::int64_t value = 0;
        if (!parse_i64(fields[i], value)) return false;
        const std::string& unit = fields[i + 1U];

        std::int64_t* target = nullptr;
        std::int64_t scaled = value;
        if (unit == "year" || unit == "years" || unit == "yr") {
            target = &years;
        } else if (unit == "month" || unit == "months") {
            target = &months;
        } else if (unit == "week" || unit == "weeks") {
            if (!checked_week_days(value, scaled)) return false;
            target = &days;
        } else if (unit == "day" || unit == "days") {
            target = &days;
        } else {
            return false;
        }

        std::int64_t combined = 0;
        if (!infiltratr_i64_add_checked(*target, scaled, &combined)) {
            return false;
        }
        *target = combined;
    }
    return true;
}

bool apply_calendar_offsets(
    int& y, int& m, int& d,
    std::int64_t years, std::int64_t months, std::int64_t days,
    bool subtract) {
    if (!subtract) {
        return shift_years_clamped(y, m, d, years) &&
               shift_months_clamped(y, m, d, months) &&
               shift_days_checked(y, m, d, days);
    }

    std::int64_t ny = 0;
    std::int64_t nm = 0;
    std::int64_t nd = 0;
    if (!checked_negate(years, ny) ||
        !checked_negate(months, nm) ||
        !checked_negate(days, nd)) {
        return false;
    }
    return shift_days_checked(y, m, d, nd) &&
           shift_months_clamped(y, m, d, nm) &&
           shift_years_clamped(y, m, d, ny);
}

struct CalendarDifference {
    std::int64_t years = 0;
    std::int64_t months = 0;
    std::int64_t weeks = 0;
    std::int64_t days = 0;
};

CalendarDifference calendar_difference(
    int y1, int m1, int d1, int y2, int m2, int d2) {
    const std::int64_t first_serial = days_from_civil(
        y1, static_cast<unsigned>(m1), static_cast<unsigned>(d1));
    const std::int64_t second_serial = days_from_civil(
        y2, static_cast<unsigned>(m2), static_cast<unsigned>(d2));
    if (first_serial > second_serial) {
        std::swap(y1, y2);
        std::swap(m1, m2);
        std::swap(d1, d2);
    }

    const std::int64_t end_serial = days_from_civil(
        y2, static_cast<unsigned>(m2), static_cast<unsigned>(d2));

    CalendarDifference result;
    result.years = static_cast<std::int64_t>(y2) - y1;

    int cy = y1;
    int cm = m1;
    int cd = d1;
    if (!shift_years_clamped(cy, cm, cd, result.years) ||
        days_from_civil(cy, static_cast<unsigned>(cm), static_cast<unsigned>(cd)) >
            end_serial) {
        --result.years;
        cy = y1;
        cm = m1;
        cd = d1;
        (void)shift_years_clamped(cy, cm, cd, result.years);
    }

    result.months =
        (static_cast<std::int64_t>(y2) - cy) * 12 + (m2 - cm);
    int my = cy;
    int mm = cm;
    int md = cd;
    if (!shift_months_clamped(my, mm, md, result.months) ||
        days_from_civil(my, static_cast<unsigned>(mm), static_cast<unsigned>(md)) >
            end_serial) {
        --result.months;
        my = cy;
        mm = cm;
        md = cd;
        (void)shift_months_clamped(my, mm, md, result.months);
    }

    const std::int64_t remainder =
        end_serial -
        days_from_civil(my, static_cast<unsigned>(mm), static_cast<unsigned>(md));
    result.weeks = remainder / 7;
    result.days = remainder % 7;
    return result;
}

ToolResult datetime_tool(std::string_view input) {
    const auto f = split_ws(input);
    if (f.empty()) return failure("Enter diff, add, sub or unix.");

    if (f[0] == "diff") {
        int y1,m1,d1,y2,m2,d2;
        if (f.size()!=3U||!parse_date(f[1],y1,m1,d1)||!parse_date(f[2],y2,m2,d2))
            return failure("Usage: diff YYYY-MM-DD YYYY-MM-DD");
        const auto delta =
            days_from_civil(
                y2, static_cast<unsigned>(m2), static_cast<unsigned>(d2)) -
            days_from_civil(
                y1, static_cast<unsigned>(m1), static_cast<unsigned>(d1));
        const auto absdays = delta < 0 ? -delta : delta;
        const CalendarDifference calendar =
            calendar_difference(y1,m1,d1,y2,m2,d2);
        std::ostringstream out;
        out << "Days  " << delta
            << "\nAbsolute  " << absdays
            << "\nWeeks + days  " << absdays/7 << " + " << absdays%7
            << "\nCalendar absolute  "
            << calendar.years << " years, "
            << calendar.months << " months, "
            << calendar.weeks << " weeks, "
            << calendar.days << " days";
        return success(out.str());
    }

    if (f[0] == "add" || f[0] == "sub") {
        int y=0,m=0,d=0;
        if (f.size() < 3U || !parse_date(f[1],y,m,d)) {
            return failure(
                "Usage: add|sub YYYY-MM-DD days OR add|sub YYYY-MM-DD N years N months N weeks N days");
        }

        std::int64_t years = 0;
        std::int64_t months = 0;
        std::int64_t days = 0;
        if (f.size() == 3U) {
            if (!parse_i64(f[2], days)) return failure("Invalid day offset.");
        } else if (!parse_calendar_offsets(f, years, months, days)) {
            return failure(
                "Use offset pairs such as 1 year 2 months 3 weeks 4 days.");
        }

        if (!apply_calendar_offsets(
                y, m, d, years, months, days, f[0] == "sub")) {
            return failure("Result is outside the supported civil date domain.");
        }
        return success("Date  " + date_string(
            y, static_cast<unsigned>(m), static_cast<unsigned>(d)));
    }

    if (f[0]=="unix") {
        if(f.size()!=2U) return failure("Usage: unix YYYY-MM-DDTHH:MM:SSZ");
        const std::string& t=f[1];
        if(t.size()!=20||t[10]!='T'||t[13]!=':'||t[16]!=':'||t[19]!='Z')
            return failure("Use UTC form YYYY-MM-DDTHH:MM:SSZ.");
        int y,m,d; if(!parse_date(std::string_view(t).substr(0,10),y,m,d)) return failure("Invalid date.");
        int hh=0,mm=0,ss=0;
        auto p=[&](std::size_t pos,std::uint64_t maximum,int& v){
            std::uint64_t parsed = 0;
            if (!parse_u64_range(
                    std::string_view(t).substr(pos, 2U),
                    0U, maximum, parsed)) {
                return false;
            }
            v = static_cast<int>(parsed);
            return true;
        };
        if(!p(11,23U,hh)||!p(14,59U,mm)||!p(17,59U,ss))
            return failure("Invalid UTC time.");
        const std::int64_t seconds =
            days_from_civil(
                y, static_cast<unsigned>(m), static_cast<unsigned>(d)) *
                86400 +
            static_cast<std::int64_t>(hh) * 3600 +
            static_cast<std::int64_t>(mm) * 60 + ss;
        return success("Unix time  "+std::to_string(seconds)+" s");
    }
    return failure("Unknown date/time operation.");
}

 
} // namespace calculator::tools::detail

namespace calculator::tools {

const std::vector<ConversionUnitInfo>& conversion_units() noexcept {
    static const std::vector<ConversionUnitInfo> units = [] {
        std::vector<ConversionUnitInfo> out;
        out.reserve(detail::kUnits.size() + detail::kAngleUnits.size());
        for (const auto& unit : detail::kUnits) {
            out.push_back({unit.name, unit.dimension});
        }
        for (const auto& unit : detail::kAngleUnits) {
            out.push_back({unit.name, unit.dimension});
        }
        return out;
    }();
    return units;
}

} // namespace calculator::tools
