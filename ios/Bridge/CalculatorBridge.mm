/* SPDX-License-Identifier: GPL-3.0-or-later */
#import "CalculatorBridge.h"

#include "../../src/ui/calculator_ui_controller.hpp"
#include "../../src/core/advanced_tools.hpp"

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
        @"equalsHover": @(palette->equals_hover_rgb),
        @"titlebar": @(palette->titlebar_rgb),
        @"connection": @(palette->connection_rgb),
        @"connectionBorder": @(palette->connection_border_rgb),
        @"heading": @(palette->heading_rgb),
        @"summary": @(palette->summary_rgb),
        @"kicker": @(palette->kicker_rgb),
        @"detailLabel": @(palette->detail_label_rgb),
        @"note": @(palette->note_rgb),
        @"statusBorder": @(palette->status_border_rgb),
        @"accentForeground": @(palette->accent_foreground_rgb),
        @"accentHover": @(palette->accent_hover_rgb),
        @"selectedSummary": @(palette->selected_summary_rgb),
        @"warningMuted": @(palette->warning_muted_rgb),
        @"warningBorder": @(palette->warning_border_rgb),
        @"successBorder": @(palette->success_border_rgb)
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
        @"scientificDigits": @([self controller]->scientific_digits()),
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

- (NSString *)additionalResultsText {
    return to_ns([self controller]->additional_results_text());
}

- (NSString *)programmerRepresentationsText {
    return to_ns([self controller]->programmer_representations_text());
}

- (NSString *)persistentStateText {
    return to_ns([self controller]->persistent_state_text());
}

- (BOOL)loadPersistentStateText:(NSString *)text {
    return [self controller]->load_persistent_state_text(
        text.UTF8String ?: "") ? YES : NO;
}

- (void)setScientificDigits:(NSInteger)digits {
    const NSInteger bounded = digits < 0 ? 0 : digits;
    [self controller]->set_scientific_digits(
        static_cast<unsigned>(bounded));
}

- (NSArray<NSDictionary *> *)advancedToolCatalog {
    NSMutableArray<NSDictionary *> *items = [NSMutableArray array];
    const auto& catalog = calculator::tools::catalog();
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        const auto& item = catalog[index];
        [items addObject:@{
            @"index": @(static_cast<NSInteger>(index)),
            @"name": to_ns(std::string(item.name)),
            @"prompt": to_ns(std::string(item.prompt)),
            @"example": to_ns(std::string(item.example))
        }];
    }
    return items;
}

- (NSDictionary *)evaluateAdvancedToolAtIndex:(NSInteger)index
                                      input:(NSString *)input {
    const auto& catalog = calculator::tools::catalog();
    if (index < 0 || static_cast<std::size_t>(index) >= catalog.size()) {
        return @{
            @"ok": @NO,
            @"output": @"",
            @"error": @"Unknown advanced tool.",
            @"points": @[]
        };
    }

    const auto result = calculator::tools::evaluate(
        catalog[static_cast<std::size_t>(index)].tool,
        input.UTF8String ?: "");
    NSMutableArray<NSDictionary *> *points = [NSMutableArray array];
    for (const auto& point : result.points) {
        [points addObject:@{
            @"x": @(point.x),
            @"y": @(point.y),
            @"valid": @(point.valid)
        }];
    }
    return @{
        @"ok": @(result.ok),
        @"output": to_ns(result.output),
        @"error": to_ns(result.error),
        @"points": points
    };
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
