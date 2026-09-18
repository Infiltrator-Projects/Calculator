/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/ui/calculator_ui_contract.hpp"

#include <cassert>
#include <string_view>

using namespace infiltrator::calc::ui;

int main() {
    static_assert(kStandardMemory.size() == 4);
    static_assert(kStandardKeypad.size() == 24);
    static_assert(kScientificKeypad.size() == 40);
    static_assert(kProgrammerKeypad.size() == 40);

    static_assert(kDesktopMetrics.default_width == 360);
    static_assert(kDesktopMetrics.default_height == 610);
    static_assert(kDesktopMetrics.minimum_width == 320);
    static_assert(kDesktopMetrics.minimum_height == 520);
    static_assert(kDesktopMetrics.grid_gap_x == 6);
    static_assert(kDesktopMetrics.grid_gap_y == 5);
    static_assert(kDesktopMetrics.key_min_height == 32);
    static_assert(kDesktopMetrics.memory_height == 24);

    assert(mode_name(Mode::Standard) == "Standard");
    assert(mode_name(Mode::Scientific) == "Scientific");
    assert(mode_name(Mode::Programmer) == "Programmer");

    assert(kStandardMemory[0].command == Command::MemoryClear);
    assert(kStandardMemory[1].command == Command::MemoryRecall);
    assert(kStandardMemory[2].command == Command::MemoryAdd);
    assert(kStandardMemory[3].command == Command::MemorySubtract);

    assert(kStandardKeypad.front().label == "%");
    assert(kStandardKeypad.back().command == Command::Equals);
    assert(kScientificKeypad.front().command == Command::ToggleDegrees);
    assert(kScientificKeypad.back().command == Command::Equals);
    assert(kProgrammerKeypad.front().command == Command::BaseBin);
    assert(kProgrammerKeypad.back().command == Command::HexF);

    assert(insertion_text(Command::Divide) == "/");
    assert(insertion_text(Command::Multiply) == "*");
    assert(insertion_text(Command::Subtract) == "-");
    assert(insertion_text(Command::Add) == "+");
    assert(insertion_text(Command::ShiftLeft) == "<<");
    assert(insertion_text(Command::ShiftRight) == ">>");

    assert(is_programmer_selector(Command::BaseHex));
    assert(is_programmer_selector(Command::Width64));
    assert(is_programmer_selector(Command::ToggleSigned));
    assert(!is_programmer_selector(Command::Equals));

    return 0;
}
