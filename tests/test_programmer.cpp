/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/programmer.hpp"
#include <iostream>
#include <string>

static int failures = 0;
static void fail(const std::string& message){std::cerr << "FAIL: " << message << '\n'; ++failures;}
static void expect(std::uint64_t actual,std::uint64_t expected,const char* label){if(actual!=expected)fail(std::string(label)+" wrong value");}
static void expect_ok(const char* expression,infiltrator::calc::ProgrammerBase base,infiltrator::calc::IntegerWidth width,std::uint64_t expected){
    const auto r=infiltrator::calc::evaluate_programmer(expression,base,width);
    if(!r.ok){fail(std::string(expression)+" -> "+r.error);return;}
    expect(r.value,expected,expression);
}
static void expect_error(const char* expression,infiltrator::calc::ProgrammerBase base,infiltrator::calc::IntegerWidth width){
    const auto r=infiltrator::calc::evaluate_programmer(expression,base,width);
    if(r.ok||r.error.empty())fail(std::string(expression)+" should fail");
}

int main(){
    using infiltrator::calc::IntegerWidth;
    using infiltrator::calc::ProgrammerBase;
    expect_ok("1010",ProgrammerBase::Binary,IntegerWidth::Bits8,10);
    expect_ok("1010 | 0101",ProgrammerBase::Binary,IntegerWidth::Bits8,15);
    expect_ok("1111 & 0101",ProgrammerBase::Binary,IntegerWidth::Bits8,5);
    expect_ok("1010 ^ 0110",ProgrammerBase::Binary,IntegerWidth::Bits8,12);
    expect_ok("1 << 4",ProgrammerBase::Binary,IntegerWidth::Bits8,16);
    expect_ok("64 >> 2",ProgrammerBase::Decimal,IntegerWidth::Bits8,16);
    expect_ok("~0",ProgrammerBase::Decimal,IntegerWidth::Bits8,255);
    expect_ok("0xff + 1",ProgrammerBase::Hexadecimal,IntegerWidth::Bits8,0);
    expect_ok("ff & 0f",ProgrammerBase::Hexadecimal,IntegerWidth::Bits8,15);
    expect_ok("17 * 4 + 2",ProgrammerBase::Decimal,IntegerWidth::Bits16,70);
    expect_ok("-1",ProgrammerBase::Decimal,IntegerWidth::Bits8,255);
    expect_ok("ff + 1",ProgrammerBase::Hexadecimal,IntegerWidth::Bits16,256);
    expect_ok("377",ProgrammerBase::Octal,IntegerWidth::Bits16,255);
    expect_error("1 / 0",ProgrammerBase::Decimal,IntegerWidth::Bits32);
    expect_error("1 << 64",ProgrammerBase::Decimal,IntegerWidth::Bits64);
    expect_error("102",ProgrammerBase::Binary,IntegerWidth::Bits8);
    if(failures){std::cerr<<failures<<" programmer test(s) failed\n";return 1;}
    std::cout<<"programmer tests passed\n";
    return 0;
}
