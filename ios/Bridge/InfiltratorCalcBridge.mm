/* SPDX-License-Identifier: GPL-3.0-or-later */
#import "InfiltratorCalcBridge.h"

#include "../../src/core/programmer.hpp"
#include "../../src/core/session.hpp"

#include <infiltratr/design.h>

#include <cmath>
#include <cstdint>
#include <string>

namespace {

using infiltrator::calc::IntegerWidth;
using infiltrator::calc::ProgrammerBase;
using infiltrator::calc::Session;

NSString *to_ns(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()];
}

NSDictionary *numeric_result(bool ok, double value, const std::string& error) {
    return @{
        @"ok": @(ok),
        @"value": @(value),
        @"display": ok ? to_ns(infiltrator::calc::format_value(value)) : @"0",
        @"error": to_ns(error)
    };
}

ProgrammerBase programmer_base(NSInteger base) {
    switch (base) {
    case 2: return ProgrammerBase::Binary;
    case 8: return ProgrammerBase::Octal;
    case 16: return ProgrammerBase::Hexadecimal;
    default: return ProgrammerBase::Decimal;
    }
}

IntegerWidth integer_width(NSInteger width) {
    switch (width) {
    case 8: return IntegerWidth::Bits8;
    case 16: return IntegerWidth::Bits16;
    case 32: return IntegerWidth::Bits32;
    default: return IntegerWidth::Bits64;
    }
}

} // namespace

@interface ICCalculatorBridge ()
@property(nonatomic, assign) void *sessionHandle;
@end

@implementation ICCalculatorBridge

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
        self.sessionHandle = new Session();
    }
    return self;
}

- (void)dealloc {
    delete static_cast<Session *>(self.sessionHandle);
    self.sessionHandle = nullptr;
}

- (Session *)session {
    return static_cast<Session *>(self.sessionHandle);
}

- (NSDictionary *)evaluate:(NSString *)expression {
    const auto result = [self session]->evaluate(expression.UTF8String ?: "");
    return numeric_result(result.ok, result.value, result.error);
}

- (NSDictionary *)applyUnary:(NSString *)operation
                  expression:(NSString *)expression {
    const auto source = [self session]->evaluate(expression.UTF8String ?: "");
    if (!source.ok) return numeric_result(false, 0.0, source.error);

    double value = source.value;
    const std::string op = operation.UTF8String ?: "";

    if (op == "±") {
        value = -value;
    } else if (op == "x²") {
        value *= value;
    } else if (op == "√") {
        if (value < 0.0) return numeric_result(false, 0.0, "domain error");
        value = std::sqrt(value);
    } else if (op == "1/x") {
        if (value == 0.0) return numeric_result(false, 0.0, "division by zero");
        value = 1.0 / value;
    } else {
        return numeric_result(false, 0.0, "unknown unary operation");
    }

    return numeric_result(std::isfinite(value), value,
                          std::isfinite(value) ? "" : "non-finite result");
}

- (NSDictionary *)applyScientific:(NSString *)operation
                       expression:(NSString *)expression
                          degrees:(BOOL)degrees {
    const auto source = [self session]->evaluate(expression.UTF8String ?: "");
    if (!source.ok) return numeric_result(false, 0.0, source.error);

    const std::string op = operation.UTF8String ?: "";
    constexpr double pi = 3.14159265358979323846;
    double value = source.value;
    double argument = value;

    if (degrees && (op == "sin" || op == "cos" || op == "tan")) {
        argument = value * pi / 180.0;
    }

    if (op == "sin") value = std::sin(argument);
    else if (op == "cos") value = std::cos(argument);
    else if (op == "tan") value = std::tan(argument);
    else if (op == "asin") {
        value = std::asin(value);
        if (degrees) value = value * 180.0 / pi;
    } else if (op == "acos") {
        value = std::acos(value);
        if (degrees) value = value * 180.0 / pi;
    } else if (op == "atan") {
        value = std::atan(value);
        if (degrees) value = value * 180.0 / pi;
    } else if (op == "ln") value = std::log(value);
    else if (op == "log") value = std::log10(value);
    else if (op == "exp") value = std::exp(value);
    else if (op == "abs") value = std::fabs(value);
    else return numeric_result(false, 0.0, "unknown scientific operation");

    if (!std::isfinite(value)) return numeric_result(false, 0.0, "domain error");
    return numeric_result(true, value, "");
}

- (NSDictionary *)evaluateProgrammer:(NSString *)expression
                                base:(NSInteger)base
                               width:(NSInteger)width
                       signedDisplay:(BOOL)signedDisplay {
    const auto selectedBase = programmer_base(base);
    const auto selectedWidth = integer_width(width);
    const auto result = infiltrator::calc::evaluate_programmer(
        expression.UTF8String ?: "", selectedBase, selectedWidth);

    if (!result.ok) {
        return @{
            @"ok": @NO,
            @"value": @0,
            @"display": @"0",
            @"error": to_ns(result.error)
        };
    }

    const std::string formatted = infiltrator::calc::format_programmer(
        result.value, selectedBase, selectedWidth, signedDisplay);

    return @{
        @"ok": @YES,
        @"value": @(result.value),
        @"display": to_ns(formatted),
        @"error": @""
    };
}

- (void)memoryClear {
    [self session]->memory_clear();
}

- (void)memoryAdd:(double)value {
    [self session]->memory_add(value);
}

- (void)memorySubtract:(double)value {
    [self session]->memory_subtract(value);
}

- (double)memoryRecall {
    return [self session]->memory_recall();
}

- (NSString *)formatValue:(double)value {
    return to_ns(infiltrator::calc::format_value(value));
}

- (NSString *)historyText {
    const auto& history = [self session]->history();
    if (history.empty()) return @"No calculations yet.";

    std::string out;
    bool first = true;
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (!first) out += "\n\n";
        first = false;

        out += it->input;
        out += "\n";
        if (it->result.ok) {
            out += infiltrator::calc::format_value(it->result.value);
        } else {
            out += "Error: ";
            out += it->result.error;
        }
    }

    return to_ns(out);
}

- (void)clearHistory {
    [self session]->clear_history();
}

@end
