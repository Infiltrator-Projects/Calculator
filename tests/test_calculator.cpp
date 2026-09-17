/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/calculator.hpp"
#include <cmath>
#include <iostream>
#include <string>

static int failures = 0;
static void fail(const std::string& message){std::cerr << "FAIL: " << message << '\n';++failures;}
static void expect_value(const char* expression,double expected){const auto r=infiltrator::calc::evaluate(expression);if(!r.ok){fail(std::string(expression)+" -> "+r.error);return;}if(std::abs(r.value-expected)>1e-12*std::max(1.0,std::abs(expected)))fail(std::string(expression)+" wrong value");}
static void expect_error(const char* expression){const auto r=infiltrator::calc::evaluate(expression);if(r.ok||r.error.empty())fail(std::string(expression)+" should fail");}
int main(){
expect_value("2 + 3 * 4",14);expect_value("(2 + 3) * 4",20);expect_value("2^3^2",512);expect_value("-4 + 10",6);expect_value("2.5 * 4",10);
expect_value("-2^2",-4);expect_value("(-2)^2",4);expect_value("2^-2",0.25);expect_value("50%",0.5);expect_value("200 * 10%",20);expect_value("  6 / 3  ",2);expect_value("1e3 + 2",1002);expect_value("--5",5);
expect_error("");expect_error("1 / 0");expect_error("(1 + 2");expect_error("1 + foo");expect_error("2 ** 3");expect_error("1 +");expect_error("nan");expect_error("1e9999");
if(failures){std::cerr<<failures<<" calculator test(s) failed\n";return 1;}std::cout<<"calculator core tests passed\n";return 0;}
