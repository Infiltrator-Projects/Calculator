/* SPDX-License-Identifier: GPL-3.0-or-later */
#import "CalculatorBridge.h"

#include "../../src/ui/calculator_ui_controller.hpp"

#include <infiltratr/design.h>

#include <array>
#include <string>
#include <string_view>

namespace {

using calculator::IntegerWidth;
using calculator::ProgrammerBase;
using calculator::ui::ButtonSpec;
using calculator::ui::Controller;
using calculator::ui::Mode;

NSString *to_ns(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()];
}

template <std::size_t N>
const ButtonSpec *find_in(const std::array<ButtonSpec, N>& specs,
                          std::string_view label) {
    for (const auto& spec : specs) {
        if (spec.label == label) return &spec;
    }
    return nullptr;
}

const ButtonSpec *find_spec(const Controller& controller,
                            std::string_view label) {
    switch (controller.state().mode) {
    case Mode::Standard:
        if (const auto *memory =
                find_in(calculator::ui::kStandardMemory, label)) {
            return memory;
        }
        return find_in(calculator::ui::kStandardKeypad, label);
    case Mode::Scientific:
        return find_in(calculator::ui::kScientificKeypad, label);
    case Mode::Programmer:
        return find_in(calculator::ui::kProgrammerKeypad, label);
    }
    return nullptr;
}

NSInteger base_value(ProgrammerBase base) {
    switch (base) {
    case ProgrammerBase::Binary: return 2;
    case ProgrammerBase::Octal: return 8;
    case ProgrammerBase::Decimal: return 10;
    case ProgrammerBase::Hexadecimal: return 16;
    }
    return 10;
}

NSInteger width_value(IntegerWidth width) {
    switch (width) {
    case IntegerWidth::Bits8: return 8;
    case IntegerWidth::Bits16: return 16;
    case IntegerWidth::Bits32: return 32;
    case IntegerWidth::Bits64: return 64;
    }
    return 64;
}

} // namespace

@interface CalculatorBridge ()
@property(nonatomic, assign) void *controllerHandle;
@end

@implementation CalculatorBridge

+ (NSDictionary<NSString *, NSNumber *> *)themePaletteForDark:(BOOL)dark {
    const InfiltratrThemePalette *palette = infiltratr_theme_resolve(
        dark ? INFILTRATR_THEME_NIGHT : INFILTRATR_THEME_DAY, dark);
    return @{
        @"background": @(palette->background_rgb),
        @"panel": @(palette->panel_rgb),
        @"card": @(palette->card_rgb),
        @"surface": @(palette->surface_rgb),
        @"input": @(palette->input_rgb),
        @"border": @(palette->border_rgb),
        @"text": @(palette->text_rgb),
        @"title": @(palette->title_rgb),
        @"muted": @(palette->muted_rgb),
        @"subtle": @(palette->subtle_rgb),
        @"buttonBackground": @(palette->button_background_rgb),
        @"buttonForeground": @(palette->button_foreground_rgb),
        @"selectionBackground": @(palette->selection_background_rgb),
        @"selectionForeground": @(palette->selection_foreground_rgb),
        @"neutralAccent": @(palette->neutral_accent_rgb),
        @"success": @(palette->success_rgb),
        @"warning": @(palette->warning_rgb),
        @"fault": @(palette->fault_rgb),
        @"info": @(palette->info_rgb),
        @"operation": @(palette->operation_rgb),
        @"cardHover": @(palette->card_hover_rgb),
        @"surfaceHover": @(palette->surface_hover_rgb),
        @"operationHover": @(palette->operation_hover_rgb),
        @"equalsHover": @(palette->equals_hover_rgb)
    };
}

- (instancetype)init {
    self = [super init];
    if (self) {
        self.controllerHandle = new Controller();
    }
    return self;
}

- (void)dealloc {
    delete static_cast<Controller *>(self.controllerHandle);
    self.controllerHandle = nullptr;
}

- (Controller *)controller {
    return static_cast<Controller *>(self.controllerHandle);
}

- (NSDictionary *)snapshot {
    const auto& state = [self controller]->state();
    return @{
        @"mode": @(static_cast<NSInteger>(state.mode)),
        @"expression": to_ns(state.expression),
        @"display": to_ns(state.result),
        @"status": to_ns(state.status),
        @"fault": @(state.fault),
        @"degrees": @(state.degrees),
        @"programmerBase": @(base_value(state.programmer_base)),
        @"programmerWidth": @(width_value(state.programmer_width)),
        @"programmerSigned": @(state.programmer_signed)
    };
}

- (void)setExpression:(NSString *)expression {
    [self controller]->set_expression(expression.UTF8String ?: "");
}

- (void)selectMode:(NSInteger)mode {
    switch (mode) {
    case 1:
        [self controller]->set_mode(Mode::Scientific);
        break;
    case 2:
        [self controller]->set_mode(Mode::Programmer);
        break;
    default:
        [self controller]->set_mode(Mode::Standard);
        break;
    }
}

- (void)pressKey:(NSString *)key {
    Controller *controller = [self controller];
    const std::string label = key.UTF8String ?: "";
    const ButtonSpec *spec = find_spec(*controller, label);
    if (spec != nullptr) {
        controller->dispatch(spec->command, Controller::kEnd);
    }
}

- (BOOL)isKeyEnabled:(NSString *)key {
    Controller *controller = [self controller];
    const std::string label = key.UTF8String ?: "";
    const ButtonSpec *spec = find_spec(*controller, label);
    return spec != nullptr && controller->command_enabled(spec->command);
}

- (NSString *)historyText {
    const auto& history = [self controller]->session().history();
    if (history.empty()) return @"No calculations yet.";

    std::string out;
    bool first = true;
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (!first) out += "\n\n";
        first = false;

        out += it->input;
        out += "\n";
        if (it->result.ok) {
            out += calculator::format_value(it->result.value);
        } else {
            out += "Error: ";
            out += it->result.error;
        }
    }

    return to_ns(out);
}

- (void)clearHistory {
    [self controller]->clear_history();
}

@end
