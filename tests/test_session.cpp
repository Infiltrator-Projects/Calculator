/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/session.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

static int failures = 0;
static void fail(const std::string& message){std::cerr << "FAIL: " << message << '\n';++failures;}
static void expect_value(const infiltrator::calc::Result& r,double expected,const char* label){
    if(!r.ok){fail(std::string(label)+" -> "+r.error);return;}
    if(std::abs(r.value-expected)>1e-12*std::max(1.0,std::abs(expected)))fail(std::string(label)+" wrong value");
}

int main(){
    infiltrator::calc::Variables variables{{"width",1920.0},{"height",1080.0}};
    expect_value(infiltrator::calc::evaluate("width * height",variables),2073600.0,"variables");
    expect_value(infiltrator::calc::evaluate("sqrt(width^2 + height^2)",variables),std::sqrt(1920.0*1920.0+1080.0*1080.0),"variable expression");

    infiltrator::calc::Session session(3);
    expect_value(session.evaluate("x=10"),10.0,"assignment");
    expect_value(session.evaluate("x * 2"),20.0,"stored variable");

    session.memory_clear();
    if(!session.memory_empty()) fail("cleared memory should be empty");
    session.memory_add(12.5);
    if(session.memory_empty()) fail("memory add should make memory available");
    session.memory_subtract(2.5);
    if(std::abs(session.memory_recall()-10.0)>1e-12) fail("memory state wrong");
    session.memory_clear();
    if(!session.memory_empty()) fail("second memory clear should be empty");

    expect_value(session.evaluate("x + 5"),15.0,"history result");
    if(session.history().size()!=3) fail("history length wrong");
    if(session.history().back().input!="x + 5") fail("history input wrong");

    session.evaluate("1+1");
    session.evaluate("2+2");
    if(session.history().size()!=3) fail("history limit wrong");

    session.clear_history();
    if(!session.history().empty()) fail("history clear wrong");

    if(failures){std::cerr<<failures<<" session test(s) failed\n";return 1;}
    std::cout<<"calculator session tests passed\n";
    return 0;
}
