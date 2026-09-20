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
    check(contains(evaluate(AdvancedTool::Network,"subnet 192.168.10.42/24"),"Network  192.168.10.0"),"subnet network");
    check(contains(evaluate(AdvancedTool::Network,"cidr 254"),"/24"),"cidr sizing");
    check(contains(evaluate(AdvancedTool::Storage,"raid 5 6 4 TiB"),"20"),"raid capacity");
    check(contains(evaluate(AdvancedTool::Storage,"clusters 4097 4096"),"Slack bytes  4095"),"cluster slack");
    check(contains(evaluate(AdvancedTool::DateTime,"diff 2026-09-20 2026-09-21"),"Days  1"),"date diff");
    check(contains(evaluate(AdvancedTool::DateTime,"add 2024-02-28 1"),"2024-02-29"),"date leap add");
    check(contains(evaluate(AdvancedTool::Constants,"c0"),"2.99792458"),"constant c");
    check(contains(evaluate(AdvancedTool::Statistics,"1,2,3,4,5"),"Mean  3"),"statistics mean");

    const auto graph=evaluate(AdvancedTool::Graph,"sin(x);-3.141592653589793;3.141592653589793;9");
    check(graph.ok&&graph.points.size()==9U,"graph points");
    check(graph.ok&&std::fabs(graph.points[4].y)<1e-12,"graph center");

    const auto roots=evaluate(AdvancedTool::EquationSolver,"x^2-2;0;2");
    check(roots.ok&&roots.output.find("1.414213")!=std::string::npos,"equation root");

    check(contains(evaluate(AdvancedTool::ExactDecimal,"0.1+0.2"),"30/100"),"exact decimal fraction");
    check(contains(evaluate(AdvancedTool::ExactDecimal,"0.1+0.2"),"0.3"),"exact decimal presentation");
    check(contains(evaluate(AdvancedTool::ExactDecimal,"1/3"),"1/3"),"exact rational division");

    check(contains(evaluate(AdvancedTool::ArbitraryPrecision,"2^128"),"340282366920938463463374607431768211456"),"big integer power");
    check(contains(evaluate(AdvancedTool::ArbitraryPrecision,"100!"),"933262154439"),"big factorial");

    check(contains(evaluate(AdvancedTool::Complex,"mul 1,2 3,-4"),"11 + 2i"),"complex multiply");
    check(!evaluate(AdvancedTool::Complex,"div 1,2 0,0").ok,"complex divide zero");

    check(calculator::constant_catalog().size()>=16U,"constant catalogue");
    check(calculator::evaluate("tau").ok,"tau in scientific parser");
    check(calculator::evaluate("c0").ok,"c0 in scientific parser");

    if(failures){std::cerr<<failures<<" advanced-tool test(s) failed\n";return 1;}
    std::cout<<"advanced tool tests passed\n";
    return 0;
}
