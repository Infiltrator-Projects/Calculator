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

// Constants, statistics, graph sampling and equation solving form the analysis family. Graph/root evaluation reuses Calculator's existing expression engine rather than defining another grammar.
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
    // Kahan summation keeps the reported sum stable; Welford's recurrence
    // avoids catastrophic cancellation in variance for large-offset samples.
    double sum=0.0;
    double compensation=0.0;
    double mean=0.0;
    double m2=0.0;
    std::size_t seen=0;
    for(double x:v){
        const double adjusted=x-compensation;
        const double next=sum+adjusted;
        compensation=(next-sum)-adjusted;
        sum=next;

        ++seen;
        const double delta=x-mean;
        mean+=delta/static_cast<double>(seen);
        const double delta2=x-mean;
        m2+=delta*delta2;
    }
    const double pop=m2/static_cast<double>(v.size());
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
        const double sample=m2/static_cast<double>(v.size()-1U);
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
    if(p.size()>3U&&!p[3].empty()&&
       !parse_u64_range(p[3],2U,4096U,samples))
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
    calculator::ScientificVariables vars;
    vars["x"] = calculator::scientific_value_from_double(x);
    const auto result = calculator::evaluate_scientific(
        std::string(expr), vars, {}, calculator::AngleUnit::Radians,
        kToolScientificDigits);
    double value = 0.0;
    ok = result.ok && result.display.empty() &&
         calculator::scientific_value_to_double(result.value, value) &&
         std::isfinite(value);
    return ok ? value : 0.0;
}
void add_root(std::vector<double>& roots,double x) {
    for(double r:roots) if(std::fabs(r-x)<=1e-9*std::max({1.0,std::fabs(r),std::fabs(x)})) return;
    roots.push_back(x);
}
bool refine_stationary_root(std::string_view expression,
                            double left, double seed, double right,
                            double& root) {
    double x=seed;
    const double span=std::max(right-left,1e-12);
    for(int iteration=0;iteration<40;++iteration){
        bool ok=false;
        const double fx=eval_x(expression,x,ok);
        if(!ok) return false;
        if(std::fabs(fx)<=1e-24){root=x;return true;}

        const double h=std::max(
            span*1e-4,
            std::sqrt(std::numeric_limits<double>::epsilon())*
                std::max(1.0,std::fabs(x)));
        bool okp=false,okm=false;
        const double fp=eval_x(expression,std::min(right,x+h),okp);
        const double fm=eval_x(expression,std::max(left,x-h),okm);
        if(!okp||!okm) return false;
        const double denominator=
            std::min(right,x+h)-std::max(left,x-h);
        if(denominator<=0.0) return false;
        const double derivative=(fp-fm)/denominator;
        if(!std::isfinite(derivative)||std::fabs(derivative)<1e-15) return false;

        double next=x-fx/derivative;
        if(!std::isfinite(next)) return false;
        next=std::clamp(next,left,right);
        if(std::fabs(next-x)<=
           8.0*std::numeric_limits<double>::epsilon()*
               std::max(1.0,std::fabs(x))){
            x=next;
            break;
        }
        x=next;
    }
    bool ok=false;
    const double value=eval_x(expression,x,ok);
    if(ok&&std::fabs(value)<=1e-7){root=x;return true;}
    return false;
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
    double x_prev2=xmin;
    bool ok_prev2=false;
    double y_prev2=eval_x(p[0],x_prev2,ok_prev2);
    if(ok_prev2&&std::fabs(y_prev2)<1e-12) add_root(roots,x_prev2);

    double x_prev=x_prev2;
    double y_prev=y_prev2;
    bool ok_prev=ok_prev2;

    for(int i=1;i<=segments;++i){
        const double x=xmin+(xmax-xmin)*static_cast<double>(i)/segments;
        bool ok=false;
        const double y=eval_x(p[0],x,ok);
        if(ok&&std::fabs(y)<1e-12) add_root(roots,x);

        if(ok_prev&&ok&&std::signbit(y_prev)!=std::signbit(y)){
            double lo=x_prev,hi=x,flo=y_prev,fhi=y;
            for(int n=0;n<80;++n){
                const double mid=(lo+hi)/2.0;
                bool okm=false;
                const double fm=eval_x(p[0],mid,okm);
                if(!okm) break;
                if(std::fabs(fm)<1e-14){lo=hi=mid;flo=fhi=fm;break;}
                if(std::signbit(flo)!=std::signbit(fm)){hi=mid;fhi=fm;}
                else{lo=mid;flo=fm;}
            }
            const double candidate=(lo+hi)/2.0;
            bool okc=false;
            const double fc=eval_x(p[0],candidate,okc);
            if(okc&&std::fabs(fc)<=1e-7) add_root(roots,candidate);
        }

        // Sign-change bracketing cannot see even-multiplicity roots. A local
        // minimum of |f(x)| is therefore refined with a bounded numerical
        // Newton step and admitted only after direct residual validation.
        if(i>=2&&ok_prev2&&ok_prev&&ok&&
           std::fabs(y_prev)<std::fabs(y_prev2)&&
           std::fabs(y_prev)<std::fabs(y)){
            double candidate=0.0;
            if(refine_stationary_root(
                   p[0],x_prev2,x_prev,x,candidate)){
                add_root(roots,candidate);
            }
        }

        x_prev2=x_prev;
        y_prev2=y_prev;
        ok_prev2=ok_prev;
        x_prev=x;
        y_prev=y;
        ok_prev=ok;
    }
    std::sort(roots.begin(),roots.end());
    if(roots.empty()) return failure("No validated real root was found in the requested interval.");
    std::ostringstream out; out<<"Roots ("<<roots.size()<<")";
    for(double root:roots) out<<"\n"<<number(root);
    return success(out.str());
}

} // namespace calculator::tools::detail
