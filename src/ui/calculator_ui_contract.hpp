/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace infiltrator::calc::ui {

enum class Mode { Standard = 0, Scientific = 1, Programmer = 2 };

enum class ButtonRole {
    Number,
    Operation,
    Utility,
    Clear,
    Equals
};

enum class Command {
    MemoryClear,
    MemoryRecall,
    MemoryAdd,
    MemorySubtract,
    Clear,
    AllClear,
    Backspace,
    Percent,
    Divide,
    Multiply,
    Subtract,
    Add,
    Power,
    Reciprocal,
    Square,
    SquareRoot,
    CubeRoot,
    Negate,
    Equals,
    DecimalPoint,
    OpenParen,
    CloseParen,
    Pi,
    Euler,
    Factorial,
    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Ln,
    Log10,
    Exp,
    Abs,
    ToggleDegrees,
    BaseBin,
    BaseOct,
    BaseDec,
    BaseHex,
    Width8,
    Width16,
    Width32,
    Width64,
    ToggleSigned,
    BitNot,
    BitAnd,
    BitOr,
    BitXor,
    ShiftLeft,
    ShiftRight,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    HexA,
    HexB,
    HexC,
    HexD,
    HexE,
    HexF
};

struct ButtonSpec {
    std::string_view label;
    ButtonRole role;
    Command command;
};

struct DesktopMetrics {
    int default_width;
    int default_height;
    int minimum_width;
    int minimum_height;
    int shell_padding;
    int section_gap;
    int grid_gap_x;
    int grid_gap_y;
    int key_min_height;
    int memory_height;
    int mode_height;
    int display_height;
};

inline constexpr DesktopMetrics kDesktopMetrics{
    360, 610,
    320, 520,
    10, 6,
    6, 5,
    32, 24,
    38, 104
};

inline constexpr std::array<ButtonSpec, 4> kStandardMemory{{
    {"MC", ButtonRole::Utility, Command::MemoryClear},
    {"MR", ButtonRole::Utility, Command::MemoryRecall},
    {"M+", ButtonRole::Utility, Command::MemoryAdd},
    {"M−", ButtonRole::Utility, Command::MemorySubtract},
}};

inline constexpr std::array<ButtonSpec, 24> kStandardKeypad{{
    {"%", ButtonRole::Operation, Command::Percent},
    {"C", ButtonRole::Clear, Command::Clear},
    {"⌫", ButtonRole::Clear, Command::Backspace},
    {"÷", ButtonRole::Operation, Command::Divide},

    {"1/x", ButtonRole::Operation, Command::Reciprocal},
    {"x²", ButtonRole::Operation, Command::Square},
    {"√", ButtonRole::Operation, Command::SquareRoot},
    {"^", ButtonRole::Operation, Command::Power},

    {"7", ButtonRole::Number, Command::Digit7},
    {"8", ButtonRole::Number, Command::Digit8},
    {"9", ButtonRole::Number, Command::Digit9},
    {"×", ButtonRole::Operation, Command::Multiply},

    {"4", ButtonRole::Number, Command::Digit4},
    {"5", ButtonRole::Number, Command::Digit5},
    {"6", ButtonRole::Number, Command::Digit6},
    {"−", ButtonRole::Operation, Command::Subtract},

    {"1", ButtonRole::Number, Command::Digit1},
    {"2", ButtonRole::Number, Command::Digit2},
    {"3", ButtonRole::Number, Command::Digit3},
    {"+", ButtonRole::Operation, Command::Add},

    {"±", ButtonRole::Operation, Command::Negate},
    {"0", ButtonRole::Number, Command::Digit0},
    {".", ButtonRole::Number, Command::DecimalPoint},
    {"=", ButtonRole::Equals, Command::Equals},
}};

inline constexpr std::array<ButtonSpec, 40> kScientificKeypad{{
    {"DEG", ButtonRole::Utility, Command::ToggleDegrees},
    {"π", ButtonRole::Operation, Command::Pi},
    {"e", ButtonRole::Operation, Command::Euler},
    {"C", ButtonRole::Clear, Command::Clear},

    {"sin", ButtonRole::Operation, Command::Sin},
    {"cos", ButtonRole::Operation, Command::Cos},
    {"tan", ButtonRole::Operation, Command::Tan},
    {"⌫", ButtonRole::Clear, Command::Backspace},

    {"asin", ButtonRole::Operation, Command::Asin},
    {"acos", ButtonRole::Operation, Command::Acos},
    {"atan", ButtonRole::Operation, Command::Atan},
    {"^", ButtonRole::Operation, Command::Power},

    {"ln", ButtonRole::Operation, Command::Ln},
    {"log", ButtonRole::Operation, Command::Log10},
    {"exp", ButtonRole::Operation, Command::Exp},
    {"x!", ButtonRole::Operation, Command::Factorial},

    {"√", ButtonRole::Operation, Command::SquareRoot},
    {"∛", ButtonRole::Operation, Command::CubeRoot},
    {"abs", ButtonRole::Operation, Command::Abs},
    {"%", ButtonRole::Operation, Command::Percent},

    {"7", ButtonRole::Number, Command::Digit7},
    {"8", ButtonRole::Number, Command::Digit8},
    {"9", ButtonRole::Number, Command::Digit9},
    {"÷", ButtonRole::Operation, Command::Divide},

    {"4", ButtonRole::Number, Command::Digit4},
    {"5", ButtonRole::Number, Command::Digit5},
    {"6", ButtonRole::Number, Command::Digit6},
    {"×", ButtonRole::Operation, Command::Multiply},

    {"1", ButtonRole::Number, Command::Digit1},
    {"2", ButtonRole::Number, Command::Digit2},
    {"3", ButtonRole::Number, Command::Digit3},
    {"−", ButtonRole::Operation, Command::Subtract},

    {"(", ButtonRole::Operation, Command::OpenParen},
    {"0", ButtonRole::Number, Command::Digit0},
    {")", ButtonRole::Operation, Command::CloseParen},
    {"+", ButtonRole::Operation, Command::Add},

    {"±", ButtonRole::Operation, Command::Negate},
    {".", ButtonRole::Number, Command::DecimalPoint},
    {"1/x", ButtonRole::Operation, Command::Reciprocal},
    {"=", ButtonRole::Equals, Command::Equals},
}};

inline constexpr std::array<ButtonSpec, 40> kProgrammerKeypad{{
    {"BIN", ButtonRole::Utility, Command::BaseBin},
    {"OCT", ButtonRole::Utility, Command::BaseOct},
    {"DEC", ButtonRole::Utility, Command::BaseDec},
    {"HEX", ButtonRole::Utility, Command::BaseHex},

    {"W8", ButtonRole::Utility, Command::Width8},
    {"W16", ButtonRole::Utility, Command::Width16},
    {"W32", ButtonRole::Utility, Command::Width32},
    {"W64", ButtonRole::Utility, Command::Width64},

    {"U/S", ButtonRole::Utility, Command::ToggleSigned},
    {"~", ButtonRole::Operation, Command::BitNot},
    {"&", ButtonRole::Operation, Command::BitAnd},
    {"|", ButtonRole::Operation, Command::BitOr},

    {"^", ButtonRole::Operation, Command::BitXor},
    {"<<", ButtonRole::Operation, Command::ShiftLeft},
    {">>", ButtonRole::Operation, Command::ShiftRight},
    {"AC", ButtonRole::Clear, Command::AllClear},

    {"(", ButtonRole::Operation, Command::OpenParen},
    {")", ButtonRole::Operation, Command::CloseParen},
    {"÷", ButtonRole::Operation, Command::Divide},
    {"×", ButtonRole::Operation, Command::Multiply},

    {"7", ButtonRole::Number, Command::Digit7},
    {"8", ButtonRole::Number, Command::Digit8},
    {"9", ButtonRole::Number, Command::Digit9},
    {"−", ButtonRole::Operation, Command::Subtract},

    {"4", ButtonRole::Number, Command::Digit4},
    {"5", ButtonRole::Number, Command::Digit5},
    {"6", ButtonRole::Number, Command::Digit6},
    {"+", ButtonRole::Operation, Command::Add},

    {"1", ButtonRole::Number, Command::Digit1},
    {"2", ButtonRole::Number, Command::Digit2},
    {"3", ButtonRole::Number, Command::Digit3},
    {"=", ButtonRole::Equals, Command::Equals},

    {"0", ButtonRole::Number, Command::Digit0},
    {"A", ButtonRole::Number, Command::HexA},
    {"B", ButtonRole::Number, Command::HexB},
    {"⌫", ButtonRole::Clear, Command::Backspace},

    {"C", ButtonRole::Number, Command::HexC},
    {"D", ButtonRole::Number, Command::HexD},
    {"E", ButtonRole::Number, Command::HexE},
    {"F", ButtonRole::Number, Command::HexF},
}};

inline constexpr std::string_view mode_name(Mode mode) {
    switch (mode) {
    case Mode::Standard: return "Standard";
    case Mode::Scientific: return "Scientific";
    case Mode::Programmer: return "Programmer";
    }
    return "Standard";
}

inline constexpr bool is_programmer_selector(Command command) {
    switch (command) {
    case Command::BaseBin:
    case Command::BaseOct:
    case Command::BaseDec:
    case Command::BaseHex:
    case Command::Width8:
    case Command::Width16:
    case Command::Width32:
    case Command::Width64:
    case Command::ToggleSigned:
        return true;
    default:
        return false;
    }
}

inline constexpr std::string_view insertion_text(Command command) {
    switch (command) {
    case Command::Digit0: return "0";
    case Command::Digit1: return "1";
    case Command::Digit2: return "2";
    case Command::Digit3: return "3";
    case Command::Digit4: return "4";
    case Command::Digit5: return "5";
    case Command::Digit6: return "6";
    case Command::Digit7: return "7";
    case Command::Digit8: return "8";
    case Command::Digit9: return "9";
    case Command::HexA: return "A";
    case Command::HexB: return "B";
    case Command::HexC: return "C";
    case Command::HexD: return "D";
    case Command::HexE: return "E";
    case Command::HexF: return "F";
    case Command::DecimalPoint: return ".";
    case Command::OpenParen: return "(";
    case Command::CloseParen: return ")";
    case Command::Divide: return "/";
    case Command::Multiply: return "*";
    case Command::Subtract: return "-";
    case Command::Add: return "+";
    case Command::Power: return "^";
    case Command::Percent: return "%";
    case Command::BitNot: return "~";
    case Command::BitAnd: return "&";
    case Command::BitOr: return "|";
    case Command::BitXor: return "^";
    case Command::ShiftLeft: return "<<";
    case Command::ShiftRight: return ">>";
    default: return "";
    }
}

} // namespace infiltrator::calc::ui
