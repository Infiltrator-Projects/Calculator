/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/advanced_tools.hpp"
#include "../src/core/calculator.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {
int failures=0;
void check(bool ok,const char* name){if(!ok){std::cerr<<"FAIL: "<<name<<'\n';++failures;}}
bool contains(const calculator::tools::ToolResult& r,const char* text){return r.ok&&r.output.find(text)!=std::string::npos;}
}

int main(){
    using calculator::tools::AdvancedTool;
    using calculator::tools::evaluate;

    check(contains(evaluate(AdvancedTool::Engineering,"ohm 12 2"),"6e+00 ohm"),"engineering ohm");
    check(contains(evaluate(AdvancedTool::UnitConversion,"100 km mi"),"62.137"),"unit conversion");
    check(contains(evaluate(AdvancedTool::UnitConversion,"32 F C"),"0 C"),"temperature conversion");
    check(contains(evaluate(AdvancedTool::UnitConversion,"212 F C"),"100 C"),"exact Fahrenheit ratio");
    check(contains(evaluate(AdvancedTool::UnitConversion,"180 deg rad"),"3.141592653589"),"multiprecision angle conversion");
    check(contains(evaluate(AdvancedTool::UnitConversion,"8 bit byte"),"1 byte"),"Mint digital storage bits to bytes");
    check(contains(evaluate(AdvancedTool::UnitConversion,"1024 KiB MiB"),"1 MiB"),"Mint IEC storage conversion");
    check(contains(evaluate(AdvancedTool::UnitConversion,"1000 kB MB"),"1 MB"),"Mint decimal storage conversion");
    check(contains(evaluate(AdvancedTool::UnitConversion,"1 uL mL"),"0.001 mL"),"Mint microlitre conversion");
    check(contains(evaluate(AdvancedTool::UnitConversion,"1 cupMetric mL"),"250 mL"),"Mint metric cup conversion");
    const auto& units = calculator::tools::conversion_units();
    check(units.size()==138U,"conversion catalogue size");
    bool km=false,mi=false,deg=false;
    for(const auto& unit:units){
        km = km || (unit.name=="km" && unit.dimension=="length");
        mi = mi || (unit.name=="mi" && unit.dimension=="length");
        deg = deg || (unit.name=="deg" && unit.dimension=="angle");
    }
    check(km&&mi&&deg,"conversion catalogue metadata");
    check(contains(evaluate(AdvancedTool::UnitConversion,"1 pc ly"),"3.261"),"parsec light-year conversion");
    check(contains(evaluate(AdvancedTool::UnitConversion,"1 st lb"),"14"),"stone pounds conversion");
    check(contains(evaluate(AdvancedTool::UnitConversion,"491.67 R K"),"273.15 K"),"rankine conversion");
    check(contains(evaluate(AdvancedTool::UnitConversion,"1 GHz MHz"),"1000 MHz"),"frequency conversion");
    check(contains(evaluate(AdvancedTool::UnitConversion,"1 week day"),"7 day"),"duration conversion");
    check(contains(evaluate(AdvancedTool::Network,"subnet 192.168.10.42/24"),"Network  192.168.10.0"),"subnet network");
    check(contains(evaluate(AdvancedTool::Network,"cidr 254"),"/24"),"cidr sizing");
    check(!evaluate(AdvancedTool::Network,"subnet 192.168.1.1/33").ok,
          "subnet prefix range");
    check(!evaluate(AdvancedTool::Network,"cidr 0").ok,
          "cidr host lower bound");
    check(contains(evaluate(AdvancedTool::Storage,"raid 5 6 4 TiB"),"20"),"raid capacity");
    check(contains(evaluate(AdvancedTool::Storage,"clusters 4097 4096"),"Slack bytes  4095"),"cluster slack");
    check(contains(evaluate(AdvancedTool::Storage,"convert 1 YiB ZiB"),"1024 ZiB"),"yobibyte storage conversion");
    check(contains(evaluate(AdvancedTool::DateTime,"diff 2026-09-20 2026-09-21"),"Days  1"),"date diff");
    check(contains(evaluate(AdvancedTool::DateTime,"add 2024-02-28 1"),"2024-02-29"),"date leap add");
    check(contains(evaluate(AdvancedTool::DateTime,"add 2024-01-31 1 month"),"2024-02-29"),"calendar month clamp");
    check(contains(evaluate(AdvancedTool::DateTime,"add 2024-02-29 1 year"),"2025-02-28"),"calendar year clamp");
    check(contains(evaluate(AdvancedTool::DateTime,"sub 2025-03-01 1 day"),"2025-02-28"),"calendar subtract");
    check(contains(evaluate(AdvancedTool::DateTime,"diff 2024-01-31 2025-03-02"),"Calendar absolute  1 years, 1 months"),"calendar structured difference");
    check(!evaluate(
              AdvancedTool::DateTime,
              "add 9999-12-31 9223372036854775807").ok,
          "date offset checked overflow");
    check(!evaluate(
              AdvancedTool::DateTime,
              "unix 2026-09-20T24:00:00Z").ok,
          "UTC hour range");
    check(contains(evaluate(AdvancedTool::Constants,"c0"),"2.99792458"),"constant c");
    check(contains(evaluate(AdvancedTool::Statistics,"1,2,3,4,5"),"Mean  3"),"statistics mean");

    const auto graph=evaluate(AdvancedTool::Graph,"sin(x);-3.141592653589793;3.141592653589793;9");
    const auto precise_graph=evaluate(AdvancedTool::Graph,"real(i*x);-1;1;3");
    check(graph.ok&&graph.points.size()==9U,"graph points");
    check(!evaluate(AdvancedTool::Graph,"x;0;1;1").ok,
          "graph sample lower bound");
    check(!evaluate(AdvancedTool::Graph,"x;0;1;4097").ok,
          "graph sample upper bound");
    check(graph.ok&&std::fabs(graph.points[4].y)<1e-12,"graph center");
    check(precise_graph.ok&&precise_graph.points.size()==3U&&
          std::fabs(precise_graph.points[0].y)<1e-18&&
          std::fabs(precise_graph.points[1].y)<1e-18&&
          std::fabs(precise_graph.points[2].y)<1e-18,
          "graph Scientific-only evaluator");

    const auto roots=evaluate(AdvancedTool::EquationSolver,"x^2-2;0;2");
    check(roots.ok&&roots.output.find("1.414213")!=std::string::npos,"equation root");
    const auto tangent=evaluate(AdvancedTool::EquationSolver,"x^2;-1;2");
    check(tangent.ok&&tangent.output.find("Roots (1)")!=std::string::npos,"equation even root");

    check(contains(evaluate(AdvancedTool::ExactDecimal,"0.1+0.2"),"3/10"),"exact decimal fraction");
    check(contains(evaluate(AdvancedTool::ExactDecimal,"0.1+0.2"),"0.3"),"exact decimal presentation");
    check(contains(evaluate(AdvancedTool::ExactDecimal,"1/3"),"1/3"),"exact rational division");
    check(contains(evaluate(AdvancedTool::ExactDecimal,"1/3+1/3"),"Exact fraction  2/3"),"exact rational canonical reduction");
    check(contains(evaluate(AdvancedTool::ExactDecimal,"2/4"),"Exact fraction  1/2"),"exact rational common divisor");
    check(contains(evaluate(AdvancedTool::ExactDecimal,"1/2"),"Exact decimal  0.5"),"terminating rational decimal");
    check(contains(evaluate(AdvancedTool::ExactDecimal,"10/4"),"Exact decimal  2.5"),"reduced terminating decimal");

    check(contains(evaluate(AdvancedTool::ArbitraryPrecision,"2^128"),"340282366920938463463374607431768211456"),"big integer power");
    check(contains(evaluate(AdvancedTool::ArbitraryPrecision,"100!"),"933262154439"),"big factorial");
    check(contains(evaluate(AdvancedTool::ArbitraryPrecision,"-2^2"),"-4"),"big integer unary precedence");
    check(contains(evaluate(AdvancedTool::ArbitraryPrecision,"(-2)^2"),"4"),"big integer parenthesized power");
    check(contains(evaluate(AdvancedTool::ArbitraryPrecision,"2^3^2"),"512"),"big integer right associative power");

    check(contains(evaluate(AdvancedTool::Complex,"mul 1,2 3,-4"),"11 + 2i"),"complex multiply");
    check(!evaluate(AdvancedTool::Complex,"div 1,2 0,0").ok,"complex divide zero");

    check(contains(evaluate(AdvancedTool::Financial,"pmt 250000 0.005 360"),"1498.87"),"financial payment");
    check(contains(evaluate(AdvancedTool::Financial,"sln 10000 1000 9"),"1000"),"financial straight-line depreciation");
    check(contains(evaluate(AdvancedTool::Financial,"gpm 80 0.2"),"100"),"financial gross margin");
    check(contains(evaluate(AdvancedTool::NumberUtilities,"mod 9 5"),"4"),"utility modulus");
    check(contains(evaluate(AdvancedTool::NumberUtilities,"factor 360"),"2 x 2 x 2 x 3 x 3 x 5"),"utility factorization");
    check(!evaluate(AdvancedTool::NumberUtilities,"factor 1").ok,
          "factor range lower bound");
    check(contains(evaluate(AdvancedTool::NumberUtilities,"gcd 84 30"),"6"),"utility gcd");
    check(contains(evaluate(AdvancedTool::NumberUtilities,"comb 10 3"),"120"),"utility combinations");
    check(contains(evaluate(AdvancedTool::NumberUtilities,"root 3 -8"),"-2"),"utility nth root");
    check(contains(evaluate(AdvancedTool::NumberUtilities,"char A"),"U+0041"),"utility character code");
    check(contains(evaluate(AdvancedTool::NumberUtilities,"code U+20AC"),"€"),"utility unicode code");
    check(contains(evaluate(AdvancedTool::NumberUtilities,"twos 1 8"),"255"),"utility twos complement");

    check(calculator::tools::catalog().size()==14U,"expanded advanced tool catalogue");
    check(calculator::constant_catalog().size()>=16U,"constant catalogue");
    check(calculator::evaluate("tau").ok,"tau in scientific parser");
    check(calculator::evaluate("c0").ok,"c0 in scientific parser");

    if(failures){std::cerr<<failures<<" advanced-tool test(s) failed\n";return 1;}
    std::cout<<"advanced tool tests passed\n";
    return 0;
}
