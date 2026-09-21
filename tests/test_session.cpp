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
    const auto c0_assignment = reserved.evaluate("c0=1");
    if(c0_assignment.ok || c0_assignment.error!="cannot assign reserved constant")
        fail("broader constant assignment should be rejected");
    reserved.set_variable("c", 99.0);
    if(reserved.variable("c").has_value()) fail("constant alias should be reserved");
    if(reserved.evaluate("_=1").ok) fail("last-result variable assignment should be rejected");
    if(reserved.evaluate("rand=1").ok) fail("random variable assignment should be rejected");
    if(reserved.evaluate("sin=1").ok) fail("built-in function assignment should be rejected");
    if(reserved.evaluate("sqrt=1").ok) fail("root function assignment should be rejected");

    calculator::Session unlimited;
    for(int i=0;i<150;++i) unlimited.evaluate("1+1");
    if(unlimited.history_count()!=150U)
        fail("default history should be unbounded");

    calculator::Session functions;
    const auto define_double =
        functions.evaluate("double(x)=x*2 @ Double a value");
    if(!define_double.ok || define_double.display!="Function defined: double")
        fail("single-argument function definition failed");
    expect_value(functions.evaluate("double(21)"),42.0,"custom function call");
    expect_value(functions.evaluate("double 5"),10.0,"custom prefix function call");
    const auto double_def=functions.function("double");
    if(!double_def || double_def->parameters.size()!=1U ||
       double_def->description!="Double a value")
        fail("custom function metadata wrong");

    const auto define_hyp =
        functions.evaluate("hyp2(a;b)=a*a+b*b");
    if(!define_hyp.ok) fail("multi-argument function definition failed");
    expect_value(functions.evaluate("hyp2(3;4)"),25.0,"multi-argument function");
    if(functions.evaluate("hyp2(3)").ok)
        fail("custom function wrong arity should fail");

    if(functions.evaluate("sin(x)=x").ok)
        fail("reserved built-in function definition should fail");
    if(functions.evaluate("bad(x;x)=x").ok)
        fail("duplicate custom parameters should fail");

    functions.evaluate("quad(x)=double(double(x))");
    expect_value(functions.evaluate("quad(3)"),12.0,"nested custom function");
    functions.evaluate("loop(x)=loop(x)");
    const auto recursive=functions.evaluate("loop(1)");
    if(recursive.ok || recursive.error!="function recursion too deep")
        fail("recursive custom function must be bounded");
    if(!functions.remove_function("double") ||
       functions.function("double").has_value())
        fail("custom function removal failed");

    const std::string persisted_functions =
        functions.function_definitions_text();
    calculator::Session restored_functions;
    if(!restored_functions.load_function_definitions_text(
           persisted_functions))
        fail("custom function persistence load failed");
    expect_value(restored_functions.evaluate("hyp2(6;8)"),100.0,
                 "persisted multi-argument function");
    const std::string before_bad_load =
        restored_functions.function_definitions_text();
    if(restored_functions.load_function_definitions_text("bad(x;x)=x\n"))
        fail("malformed persisted functions should be rejected");
    if(restored_functions.function_definitions_text()!=before_bad_load)
        fail("failed function load should be transactional");

    calculator::Session persisted_variables;
    expect_value(persisted_variables.evaluate("alpha=0.1"),0.1,
                 "persist variable alpha");
    expect_value(persisted_variables.evaluate("zeta=1e100"),1e100,
                 "persist variable zeta");
    const std::string variable_text = persisted_variables.variables_text();
    if(variable_text.find("alpha=")==std::string::npos ||
       variable_text.find("zeta=")==std::string::npos ||
       variable_text.find("_=")!=std::string::npos)
        fail("variable persistence text wrong");
    calculator::Session variable_restore;
    if(!variable_restore.load_variables_text(variable_text))
        fail("variable persistence load failed");
    expect_value(variable_restore.evaluate("alpha*10"),1.0,
                 "restored variable value");
    const std::string before_bad_variables = variable_restore.variables_text();
    if(variable_restore.load_variables_text("pi=3\n"))
        fail("reserved persisted variable should be rejected");
    if(variable_restore.variables_text()!=before_bad_variables)
        fail("failed variable load should be transactional");

    calculator::Session session(3);
    expect_value(session.evaluate("x=10"),10.0,"assignment");
    expect_value(session.evaluate("x * 2"),20.0,"stored variable");
    expect_value(session.evaluate("_ + 1"),21.0,"last result variable");

    const auto precise_seventh = session.evaluate_scientific(
        "1/7", calculator::AngleUnit::Radians, 80);
    if(!precise_seventh.ok ||
       calculator::format_scientific_value(
           precise_seventh.value, 80).size() < 60U)
        fail("scientific Session should retain high precision");

    const auto complex_assignment = session.evaluate_scientific(
        "z=sqrt(-1)", calculator::AngleUnit::Radians, 80);
    if(!complex_assignment.ok ||
       calculator::format_scientific_value(
           complex_assignment.value, 80)!="i")
        fail("scientific Session complex assignment failed");
    const auto complex_use = session.evaluate_scientific(
        "z*z", calculator::AngleUnit::Radians, 80);
    if(!complex_use.ok ||
       calculator::format_scientific_value(
           complex_use.value, 80)!="-1")
        fail("scientific Session complex variable lost precision");

    const std::string precise_variables = session.variables_text();
    calculator::Session precise_restore;
    if(!precise_restore.load_variables_text(precise_variables))
        fail("precise variable persistence load failed");
    const auto restored_complex = precise_restore.evaluate_scientific(
        "z", calculator::AngleUnit::Radians, 80);
    if(!restored_complex.ok ||
       calculator::format_scientific_value(
           restored_complex.value, 80)!="i")
        fail("complex variable persistence failed");

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
