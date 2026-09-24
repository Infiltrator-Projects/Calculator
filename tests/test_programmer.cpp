/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/core/programmer.hpp"
#include <iostream>
#include <string>

static int failures = 0;
static void fail(const std::string& message){std::cerr << "FAIL: " << message << '\n'; ++failures;}
static void expect(std::uint64_t actual,std::uint64_t expected,const char* label){if(actual!=expected)fail(std::string(label)+" wrong value");}
static void expect_ok(const char* expression,calculator::ProgrammerBase base,calculator::IntegerWidth width,std::uint64_t expected){
    const auto r=calculator::evaluate_programmer(expression,base,width);
    if(!r.ok){fail(std::string(expression)+" -> "+r.error);return;}
    expect(r.value,expected,expression);
}
static void expect_error(const char* expression,calculator::ProgrammerBase base,calculator::IntegerWidth width){
    const auto r=calculator::evaluate_programmer(expression,base,width);
    if(r.ok||r.error.empty())fail(std::string(expression)+" should fail");
}
static void expect_error_message(const std::string& expression,calculator::ProgrammerBase base,calculator::IntegerWidth width,const char* expected){
    const auto r=calculator::evaluate_programmer(expression,base,width);
    if(r.ok||r.error!=expected)fail("unexpected error for bounded Programmer expression: "+r.error);
}

int main(){
    using calculator::IntegerWidth;
    using calculator::ProgrammerBase;
    expect_ok("1010",ProgrammerBase::Binary,IntegerWidth::Bits8,10);
    expect_ok("1010 | 0101",ProgrammerBase::Binary,IntegerWidth::Bits8,15);
    expect_ok("1111 & 0101",ProgrammerBase::Binary,IntegerWidth::Bits8,5);
    expect_ok("1010 ^ 0110",ProgrammerBase::Binary,IntegerWidth::Bits8,12);
    expect_ok("1 << 4",ProgrammerBase::Binary,IntegerWidth::Bits8,16);
    expect_ok("1 << 10",ProgrammerBase::Hexadecimal,IntegerWidth::Bits16,1024);
    expect_ok("64 >> 2",ProgrammerBase::Decimal,IntegerWidth::Bits8,16);
    expect_ok("01011001 rol 3",ProgrammerBase::Binary,IntegerWidth::Bits8,0b11001010);
    expect_ok("01011001 ror 3",ProgrammerBase::Binary,IntegerWidth::Bits8,0b00101011);
    expect_ok("0f nand 03",ProgrammerBase::Hexadecimal,IntegerWidth::Bits8,0xfc);
    expect_ok("0f nor f0",ProgrammerBase::Hexadecimal,IntegerWidth::Bits8,0x00);
    expect_ok("1 rol 9",ProgrammerBase::Binary,IntegerWidth::Bits8,2);
    expect_ok("bswap 12",ProgrammerBase::Hexadecimal,IntegerWidth::Bits8,0x12);
    expect_ok("bswap 1234",ProgrammerBase::Hexadecimal,IntegerWidth::Bits16,0x3412);
    expect_ok("bswap 12345678",ProgrammerBase::Hexadecimal,IntegerWidth::Bits32,0x78563412);
    expect_ok("bswap 0123456789abcdef",ProgrammerBase::Hexadecimal,IntegerWidth::Bits64,0xefcdab8967452301ULL);
    expect_ok("~0",ProgrammerBase::Decimal,IntegerWidth::Bits8,255);
    expect_ok("0xff + 1",ProgrammerBase::Hexadecimal,IntegerWidth::Bits8,0);
    expect_ok("ff & 0f",ProgrammerBase::Hexadecimal,IntegerWidth::Bits8,15);
    expect_ok("17 * 4 + 2",ProgrammerBase::Decimal,IntegerWidth::Bits16,70);
    expect_ok("-1",ProgrammerBase::Decimal,IntegerWidth::Bits8,255);
    expect_ok("ff + 1",ProgrammerBase::Hexadecimal,IntegerWidth::Bits16,256);
    expect_ok("377",ProgrammerBase::Octal,IntegerWidth::Bits16,255);
    expect_error("1 / 0",ProgrammerBase::Decimal,IntegerWidth::Bits32);
    expect_error("1 << 64",ProgrammerBase::Decimal,IntegerWidth::Bits64);
    expect_error("1 << 256",ProgrammerBase::Decimal,IntegerWidth::Bits8);
    expect_error("102",ProgrammerBase::Binary,IntegerWidth::Bits8);

    std::string deeply_nested(300,'(');
    deeply_nested += "1";
    deeply_nested.append(300,')');
    expect_error_message(
        deeply_nested,ProgrammerBase::Decimal,IntegerWidth::Bits64,
        "expression nesting too deep");

    std::string deeply_unary(300,'~');
    deeply_unary += "0";
    expect_error_message(
        deeply_unary,ProgrammerBase::Decimal,IntegerWidth::Bits64,
        "expression nesting too deep");

    const auto reps = calculator::programmer_representations(
        0xff, calculator::IntegerWidth::Bits8, true);
    if(reps.binary!="11111111") fail("representation binary wrong");
    if(reps.octal!="377") fail("representation octal wrong");
    if(reps.decimal!="-1") fail("representation signed decimal wrong");
    if(reps.hexadecimal!="FF") fail("representation hexadecimal wrong");
    if(calculator::group_programmer_digits(
           "1111000011110000", ProgrammerBase::Binary) !=
       "1111 0000 1111 0000") {
        fail("binary digit grouping wrong");
    }
    if(calculator::group_programmer_digits(
           "1234567", ProgrammerBase::Octal) != "1 234 567") {
        fail("octal digit grouping wrong");
    }
    if(calculator::group_programmer_digits(
           "DEADBEEF", ProgrammerBase::Hexadecimal) != "DEAD BEEF") {
        fail("hexadecimal digit grouping wrong");
    }

    if(failures){std::cerr<<failures<<" programmer test(s) failed\n";return 1;}
    std::cout<<"programmer tests passed\n";
    return 0;
}
