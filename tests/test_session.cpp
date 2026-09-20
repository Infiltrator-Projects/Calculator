/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/session.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

static int failures = 0;
static void fail(const std::string& message){std::cerr << "FAIL: " << message << '\n';++failures;}
static void expect_value(const calculator::Result& r,double expected,const char* label){
    if(!r.ok){fail(std::string(label)+" -> "+r.error);return;}
    if(std::abs(r.value-expected)>1e-12*std::max(1.0,std::abs(expected)))fail(std::string(label)+" wrong value");
}

int main(){
    calculator::Variables variables{{"width",1920.0},{"height",1080.0}};
    expect_value(calculator::evaluate("width * height",variables),2073600.0,"variables");
    expect_value(calculator::evaluate("sqrt(width^2 + height^2)",variables),std::sqrt(1920.0*1920.0+1080.0*1080.0),"variable expression");

    calculator::Session reserved;
    const auto pi_assignment = reserved.evaluate("pi=3");
    if(pi_assignment.ok || pi_assignment.error!="cannot assign reserved constant") fail("pi assignment should be rejected");
    const auto e_assignment = reserved.evaluate("e=4");
    if(e_assignment.ok || e_assignment.error!="cannot assign reserved constant") fail("e assignment should be rejected");
    reserved.set_variable("pi", 99.0);
    if(reserved.variable("pi").has_value()) fail("set_variable should reject pi");

    calculator::Session session(3);
    expect_value(session.evaluate("x=10"),10.0,"assignment");
    expect_value(session.evaluate("x * 2"),20.0,"stored variable");

    session.memory_clear();
    if(!session.memory_empty()) fail("cleared memory should be empty");
    session.memory_store(12.5);
    if(session.memory_empty()) fail("memory store should make memory available");
    if(std::abs(session.memory_recall()-12.5)>1e-12) fail("memory store wrong");
    session.memory_add(2.5);
    session.memory_subtract(5.0);
    if(std::abs(session.memory_recall()-10.0)>1e-12) fail("memory state wrong");
    session.memory_clear();
    if(!session.memory_empty()) fail("second memory clear should be empty");

    expect_value(session.evaluate("x + 5"),15.0,"history result");
    if(session.history().size()!=3) fail("history length wrong");
    if(session.history().back().input!="x + 5") fail("history input wrong");
    if(session.history().back().output!="15") fail("history output wrong");
    if(session.history().back().kind!=calculator::HistoryKind::Scientific)
        fail("history kind wrong");

    session.evaluate("1+1");
    session.evaluate("2+2");
    if(session.history().size()!=3) fail("history limit wrong");

    const std::string history_text = session.history_text(2, "\n");
    if(history_text.find("2+2\n  = 4\n\n") != 0)
        fail("history text newest entry wrong");
    if(history_text.find("1+1\n  = 2\n\n") == std::string::npos)
        fail("history text second entry wrong");
    if(history_text.find("x + 5") != std::string::npos)
        fail("history text limit ignored");

    calculator::HistoryContext programmer_context{};
    programmer_context.programmer_base = 16;
    programmer_context.programmer_width = 8;
    programmer_context.programmer_signed = true;
    session.record_history_text(
        "FF", "-1", true, calculator::HistoryKind::Programmer,
        programmer_context);
    const auto newest = session.history_from_newest(0);
    if(!newest || newest->output!="-1" ||
       newest->kind!=calculator::HistoryKind::Programmer ||
       newest->context.programmer_base!=16 ||
       newest->context.programmer_width!=8 ||
       !newest->context.programmer_signed)
        fail("structured programmer history context wrong");
    if(session.history_count()!=3) fail("history_count wrong");

    session.clear_history();
    if(!session.history().empty()) fail("history clear wrong");

    if(failures){std::cerr<<failures<<" session test(s) failed\n";return 1;}
    std::cout<<"calculator session tests passed\n";
    return 0;
}
