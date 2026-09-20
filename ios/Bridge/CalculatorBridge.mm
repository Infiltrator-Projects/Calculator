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

NSInteger angle_value(calculator::AngleUnit unit) {
    switch (unit) {
    case calculator::AngleUnit::Degrees: return 0;
    case calculator::AngleUnit::Radians: return 1;
    case calculator::AngleUnit::Gradians: return 2;
    }
    return 0;
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

+ (NSDictionary<NSString *, NSNumber *> *)designMetrics {
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    return @{
        @"smallRadius": @(metrics->small_radius),
        @"controlRadius": @(metrics->control_radius),
        @"cardRadius": @(metrics->card_radius),
        @"panelRadius": @(metrics->panel_radius),
        @"compactSpacing": @(metrics->compact_spacing),
        @"controlSpacing": @(metrics->control_spacing),
        @"sectionSpacing": @(metrics->section_spacing),
        @"contentPadding": @(metrics->content_padding),
        @"screenPadding": @(metrics->screen_padding)
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
        @"angleUnit": @(angle_value(state.angle_unit)),
        @"scientificSecond": @(state.scientific_second),
        @"scientificHyperbolic": @(state.scientific_hyperbolic),
        @"scientificNotation": @(state.scientific_notation),
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

- (NSString *)programmerRepresentationsText {
    return to_ns([self controller]->programmer_representations_text());
}

- (NSString *)historyText {
    return to_ns([self controller]->history_text());
}

- (NSArray<NSDictionary *> *)historyEntries {
    Controller *controller = [self controller];
    NSMutableArray<NSDictionary *> *items = [NSMutableArray array];
    const std::size_t count = controller->history_count();
    for (std::size_t index = 0; index < count; ++index) {
        const auto entry = controller->history_entry(index);
        if (!entry) continue;

        NSInteger mode = 1;
        if (entry->kind == calculator::HistoryKind::Standard) mode = 0;
        else if (entry->kind == calculator::HistoryKind::Programmer) mode = 2;

        [items addObject:@{
            @"index": @(static_cast<NSInteger>(index)),
            @"mode": @(mode),
            @"input": to_ns(entry->input),
            @"output": to_ns(entry->output),
            @"ok": @(entry->ok)
        }];
    }
    return items;
}

- (BOOL)recallHistoryAtIndex:(NSInteger)index {
    if (index < 0) return NO;
    return [self controller]->recall_history(
        static_cast<std::size_t>(index)) ? YES : NO;
}

- (void)clearHistory {
    [self controller]->clear_history();
}

@end
