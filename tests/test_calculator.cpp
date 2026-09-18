/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/calculator.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

static int failures = 0;
static void fail(const std::string& message){std::cerr << "FAIL: " << message << '\n';++failures;}
static void expect_value(const char* expression,double expected){const auto r=infiltrator::calc::evaluate(expression);if(!r.ok){fail(std::string(expression)+" -> "+r.error);return;}if(std::abs(r.value-expected)>1e-12*std::max(1.0,std::abs(expected)))fail(std::string(expression)+" wrong value");}
static void expect_error(const char* expression){const auto r=infiltrator::calc::evaluate(expression);if(r.ok||r.error.empty())fail(std::string(expression)+" should fail");}
static void expect_immediate(const char* expression,double expected){const auto r=infiltrator::calc::evaluate_immediate(expression);if(!r.ok){fail(std::string("immediate ")+expression+" -> "+r.error);return;}if(std::abs(r.value-expected)>1e-12*std::max(1.0,std::abs(expected)))fail(std::string("immediate ")+expression+" wrong value");}
int main(){
expect_value("2 + 3 * 4",14);expect_value("(2 + 3) * 4",20);expect_value("2^3^2",512);expect_value("-4 + 10",6);expect_value("2.5 * 4",10);
expect_value("-2^2",-4);expect_value("(-2)^2",4);expect_value("2^-2",0.25);expect_value("50%",0.5);expect_value("200 * 10%",20);expect_value("  6 / 3  ",2);expect_value("1e3 + 2",1002);expect_value(".5 + .25",0.75);expect_value("--5",5);
expect_value("pi",3.14159265358979323846);expect_value("e",2.71828182845904523536);expect_value("sin(pi/2)",1);expect_value("cos(0)",1);expect_value("tan(0)",0);expect_value("asin(1)",3.14159265358979323846/2);expect_value("acos(1)",0);expect_value("atan(1)",3.14159265358979323846/4);expect_value("sinh(0)",0);expect_value("cosh(0)",1);expect_value("tanh(0)",0);expect_value("sqrt(81)",9);expect_value("cbrt(27)",3);expect_value("ln(e)",1);expect_value("log(1000)",3);expect_value("exp(0)",1);expect_value("abs(-12.5)",12.5);expect_value("5!",120);expect_value("0!",1);expect_value("3!^2",36);expect_value("sqrt(16)+log(100)",6);
expect_immediate("2+3*4",20);expect_immediate("100+10%",110);expect_immediate("100-10%",90);expect_immediate("100*10%",10);expect_immediate("100/10%",1000);expect_immediate("2^3+1",9);
expect_error("");expect_error("1 / 0");expect_error("(1 + 2");expect_error("1 + foo");expect_error("2 ** 3");expect_error("1 +");expect_error("nan");expect_error("0x1p2");expect_error("1e9999");expect_error("1e-9999");expect_error("sqrt(-1)");expect_error("ln(0)");expect_error("acos(2)");expect_error("(-1)!");expect_error("2.5!");expect_error("171!");expect_error("madeup(1)");
if(infiltrator::calc::format_value(1.0/3.0)!="0.333333333333333")fail("shared value formatter wrong output");
if(failures){std::cerr<<failures<<" calculator test(s) failed\n";return 1;}std::cout<<"calculator core tests passed\n";return 0;}
