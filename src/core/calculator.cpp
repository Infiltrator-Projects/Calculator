/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "calculator.hpp"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string_view>
namespace infiltrator::calc { namespace {
class Parser {
public: explicit Parser(std::string_view input):input_(input){}
Result run(){skip_space();if(input_.empty())return fail("empty expression");double v=parse_expression();skip_space();if(!error_.empty())return{false,0.0,error_};if(position_!=input_.size())return fail("unexpected input");if(!std::isfinite(v))return fail("non-finite result");return{true,v,{}};}
private:
std::string_view input_;std::size_t position_=0;std::string error_;
Result fail(const char*m){if(error_.empty())error_=m;return{false,0.0,error_};}
void skip_space(){while(position_<input_.size()&&std::isspace(static_cast<unsigned char>(input_[position_])))++position_;}
bool consume(char c){skip_space();if(position_<input_.size()&&input_[position_]==c){++position_;return true;}return false;}
double parse_expression(){double l=parse_term();while(error_.empty()){if(consume('+'))l+=parse_term();else if(consume('-'))l-=parse_term();else break;}return l;}
double parse_term(){double l=parse_unary();while(error_.empty()){if(consume('*'))l*=parse_unary();else if(consume('/')){double r=parse_unary();if(r==0.0){error_="division by zero";return 0.0;}l/=r;}else break;}return l;}
double parse_unary(){if(consume('+'))return parse_unary();if(consume('-'))return-parse_unary();return parse_power();}
double parse_power(){double l=parse_postfix();if(error_.empty()&&consume('^')){double r=parse_unary();l=std::pow(l,r);if(!std::isfinite(l))error_="invalid power result";}return l;}
double parse_postfix(){double v=parse_primary();while(error_.empty()&&consume('%'))v/=100.0;return v;}
double parse_primary(){skip_space();if(consume('(')){double v=parse_expression();if(!consume(')')&&error_.empty())error_="missing closing parenthesis";return v;}const char*b=input_.data()+position_;char*e=nullptr;double v=std::strtod(b,&e);if(e==b){error_="expected a number";return 0.0;}position_+=static_cast<std::size_t>(e-b);if(!std::isfinite(v))error_="invalid number";return v;}
};} Result evaluate(const std::string& expression){return Parser(expression).run();} }
