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

    const auto precise_value = [](const calculator::ConstantInfo& constant,
                                  std::string& text) {
        const auto evaluated = calculator::evaluate_scientific(
            std::string(constant.name), {}, {}, calculator::AngleUnit::Radians,
            kToolScientificDigits);
        if (!evaluated.ok || !evaluated.display.empty()) return false;
        text = calculator::format_scientific_value(
            evaluated.value, kToolScientificDigits);
        return true;
    };

    if(name.empty()||name=="list") {
        std::ostringstream out;
        for(std::size_t i=0;i<constants.size();++i){
            std::string value;
            if (!precise_value(constants[i], value)) {
                return failure("Constant catalogue evaluation failed.");
            }
            if(i) out<<'\n';
            out<<constants[i].name<<"  "<<value;
            if(!constants[i].unit.empty()) out<<' '<<constants[i].unit;
        }
        return success(out.str());
    }
    for(const auto& c:constants) {
        if(c.name==name || c.alias==name) {
            std::string value;
            if (!precise_value(c, value)) {
                return failure("Constant evaluation failed.");
            }
            std::string out=std::string(c.name)+"  "+value;
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
        if(!std::isfinite(sum)||!std::isfinite(mean)||!std::isfinite(m2))
            return failure("Statistics exceed the finite real range.");
    }
    const double pop=m2/static_cast<double>(v.size());
    // Unequal binary64 samples have strictly positive variance. An underflowed
    // product or rounded mean must not turn them into a constant population.
    if(v.front()!=v.back() && pop==0.0)
        return failure("Statistics require greater precision for these unequal values.");
    const double median=quantile_sorted(v,0.5);
    const double q1=quantile_sorted(v,0.25);
    const double q3=quantile_sorted(v,0.75);
    const double range=v.back()-v.front();
    if(!std::isfinite(median)||!std::isfinite(q1)||!std::isfinite(q3)||
       !std::isfinite(range)||!std::isfinite(pop)||pop<0.0)
        return failure("Statistics exceed the finite real range.");
    std::ostringstream out;
    out<<"Count  "<<v.size()
       <<"\nSum  "<<number(sum)
       <<"\nMean  "<<number(mean)
       <<"\nMedian  "<<number(median)
       <<"\nQ1  "<<number(q1)
       <<"\nQ3  "<<number(q3)
       <<"\nMin  "<<number(v.front())
       <<"\nMax  "<<number(v.back())
       <<"\nRange  "<<number(range)
       <<"\nPopulation variance  "<<number(pop)
       <<"\nPopulation stddev  "<<number(std::sqrt(pop));
    if(v.size()>1U) {
        const double sample=m2/static_cast<double>(v.size()-1U);
        out<<"\nSample variance  "<<number(sample)
           <<"\nSample stddev  "<<number(std::sqrt(sample));
    }
    return success(out.str());
}

ToolResult graph_tool(
    std::string_view input,
    EvaluationBudget& budget) {
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
    const double span=xmax-xmin;
    if(!std::isfinite(span)) return failure("X interval exceeds the finite real range.");

    ToolResult result; result.ok=true;
    result.points.reserve(static_cast<std::size_t>(samples));
    std::size_t valid=0;
    double ymin=std::numeric_limits<double>::infinity();
    double ymax=-std::numeric_limits<double>::infinity();
    calculator::ScientificVariables vars;
    for(std::uint64_t i=0;i<samples;++i) {
        const double fraction=static_cast<double>(i)/static_cast<double>(samples-1U);
        const double x=i==samples-1U ? xmax : xmin+span*fraction;
        vars["x"]=calculator::scientific_value_from_double(x);
        const auto y=calculator::evaluate_scientific(
            p[0], vars, {}, calculator::AngleUnit::Radians,
            kToolScientificDigits, &budget);
        if (!y.ok && budget.failed()) return failure(budget.error());
        double plotted=0.0;
        GraphPoint pt;
        pt.x=x;
        pt.valid=y.ok && y.display.empty() &&
                 calculator::scientific_value_to_double(y.value, plotted) &&
                 std::isfinite(plotted);
        if(pt.valid){pt.y=plotted;++valid;ymin=std::min(ymin,pt.y);ymax=std::max(ymax,pt.y);}
        result.points.push_back(pt);
    }
    if(valid==0U) return failure("Expression produced no finite graph points in the requested range.");
    result.output="Expression  "+p[0]+
                  "\nX range  "+number(xmin)+" .. "+number(xmax)+
                  "\nValid samples  "+std::to_string(valid)+"/"+std::to_string(samples)+
                  "\nY range  "+number(ymin)+" .. "+number(ymax);
    return result;
}

// Search geometry is binary64, but residuals must be scale independent. Divide
// Scientific samples by the first non-zero Scientific value before crossing
// that boundary; even 1e-4000*(x-a) then has ordinary finite search residuals.
// Exact zero is identified before normalization. A small residual alone never
// admits an arbitrary scan point as a root.
class RootFunction {
public:
    RootFunction(std::string expression, EvaluationBudget& budget)
        : expression_(std::move(expression)), budget_(budget) {}

    double evaluate(double x, bool& ok) {
        variables_["x"] = calculator::scientific_value_from_double(x);
        const auto result = calculator::evaluate_scientific(
            expression_, variables_, {}, calculator::AngleUnit::Radians,
            kToolScientificDigits, &budget_);
        ok = result.ok && result.display.empty() && result.value.imag == "0";
        if (!ok || result.value.real == "0") return 0.0;
        if (scale_.empty()) scale_ = result.value.real;
        const auto normalized = calculator::evaluate_scientific(
            "(" + result.value.real + ")/(" + scale_ + ")", {}, {},
            calculator::AngleUnit::Radians, kToolScientificDigits, &budget_);
        double value = 0.0;
        ok = normalized.ok && calculator::scientific_value_to_double(
            normalized.value, value);
        return ok ? value : 0.0;
    }

private:
    std::string expression_;
    std::string scale_;
    calculator::ScientificVariables variables_;
    EvaluationBudget& budget_;
};
void add_root(std::vector<double>& roots,double x) {
    for(double r:roots) if(std::fabs(r-x)<=1e-9*std::max({1.0,std::fabs(r),std::fabs(x)})) return;
    roots.push_back(x);
}
bool refine_stationary_root(RootFunction& function,
                            double left, double seed, double right,
                            double& root,
                            EvaluationBudget& budget) {
    double x=seed;
    const double span=std::max(right-left,1e-12);
    for(int iteration=0;iteration<128;++iteration){
        bool ok=false;
        const double fx=function.evaluate(x,ok);
        if(!ok) return false;
        if(fx==0.0){root=x;return true;}

        const double h=std::max(
            span*1e-4,
            std::sqrt(std::numeric_limits<double>::epsilon())*
                std::max(1.0,std::fabs(x)));
        bool okp=false,okm=false;
        const double xp=std::min(right,x+h);
        const double xm=std::max(left,x-h);
        const double fp=function.evaluate(xp,okp);
        if (budget.failed()) return false;
        const double fm=function.evaluate(xm,okm);
        if(!okp||!okm) return false;
        const double denominator=
            xp-xm;
        if(denominator<=0.0) return false;
        const double derivative=(fp-fm)/denominator;
        if(!std::isfinite(derivative)||derivative==0.0) return false;

        double next=x-fx/derivative;
        if(!std::isfinite(next)) return false;
        next=std::clamp(next,left,right);
        if(std::fabs(next-x)<=
           8.0*std::numeric_limits<double>::epsilon()*
               std::max(1.0,std::fabs(x))){
            // Even-multiplicity roots lack a sign-changing bracket. Require
            // residual consistent with coordinate rounding and measured local
            // curvature; an absolute epsilon would also admit positive minima.
            const double hp=xp-x;
            const double hm=x-xm;
            if(hp<=0.0||hm<=0.0) return false;
            const double curvature=2.0*((fp-fx)/hp-(fx-fm)/hm)/(hp+hm);
            const double radius=std::numeric_limits<double>::epsilon()*
                std::max(1.0,std::fabs(x));
            bool valid=false;
            const double residual=function.evaluate(next,valid);
            const double tolerance=4.0*std::fabs(curvature)*radius*radius;
            if(valid&&std::isfinite(tolerance)&&std::fabs(residual)<=tolerance){
                root=next;
                return true;
            }
            if(next==x) return false;
        }
        x=next;
    }
    return false;
}

ToolResult equation_tool(
    std::string_view input,
    EvaluationBudget& budget) {
    const auto p=split_semicolon(input);
    if(p.empty()||p[0].empty()||p.size()>3U) return failure("Usage: expression ; xmin ; xmax");
    double xmin=-100.0,xmax=100.0;
    if(p.size()>1U&&!p[1].empty()&&!parse_double(p[1],xmin)) return failure("Invalid xmin.");
    if(p.size()>2U&&!p[2].empty()&&!parse_double(p[2],xmax)) return failure("Invalid xmax.");
    if(!(xmin<xmax)) return failure("xmin must be less than xmax.");
    const double span=xmax-xmin;
    if(!std::isfinite(span)) return failure("X interval exceeds the finite real range.");

    constexpr int segments=2048;
    RootFunction function(p[0],budget);
    std::vector<double> roots;
    double x_prev2=xmin;
    bool ok_prev2=false;
    double y_prev2=function.evaluate(x_prev2,ok_prev2);
    if (budget.failed()) return failure(budget.error());
    if(ok_prev2&&y_prev2==0.0) add_root(roots,x_prev2);

    double x_prev=x_prev2;
    double y_prev=y_prev2;
    bool ok_prev=ok_prev2;

    for(int i=1;i<=segments;++i){
        const double fraction=static_cast<double>(i)/segments;
        const double x=i==segments ? xmax : xmin+span*fraction;
        bool ok=false;
        const double y=function.evaluate(x,ok);
        if (budget.failed()) return failure(budget.error());
        if(ok&&y==0.0) add_root(roots,x);

        if(ok_prev&&ok&&std::signbit(y_prev)!=std::signbit(y)){
            double lo=x_prev,hi=x,flo=y_prev,fhi=y;
            const double residual_scale=std::max(std::fabs(flo),std::fabs(fhi));
            bool bracket_valid=true;
            for(int n=0;n<80;++n){
                const double mid=lo+(hi-lo)/2.0;
                if(mid==lo||mid==hi) break;
                bool okm=false;
                const double fm=function.evaluate(mid,okm);
                if (budget.failed()) return failure(budget.error());
                if(!okm){bracket_valid=false;break;}
                if(fm==0.0){lo=hi=mid;break;}
                if(std::signbit(flo)!=std::signbit(fm)){hi=mid;fhi=fm;}
                else{lo=mid;flo=fm;}
            }
            const double candidate=lo+(hi-lo)/2.0;
            bool okc=false;
            const double fc=function.evaluate(candidate,okc);
            if (budget.failed()) return failure(budget.error());
            if(bracket_valid&&okc&&
               std::fabs(fc)<=std::sqrt(std::numeric_limits<double>::epsilon())*
                   residual_scale) add_root(roots,candidate);
        }

        // Sign-change bracketing cannot see even-multiplicity roots. A local
        // minimum of |f(x)| is therefore refined with a bounded numerical
        // Newton step and admitted only after direct residual validation.
        if(i>=2&&ok_prev2&&ok_prev&&ok&&
           std::fabs(y_prev)<std::fabs(y_prev2)&&
           std::fabs(y_prev)<std::fabs(y)){
            double candidate=0.0;
            if(refine_stationary_root(
                   function,x_prev2,x_prev,x,candidate,budget)){
                add_root(roots,candidate);
            }
            if (budget.failed()) return failure(budget.error());
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
