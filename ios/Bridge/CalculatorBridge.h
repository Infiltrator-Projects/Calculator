/* SPDX-License-Identifier: GPL-3.0-or-later */
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CalculatorBridge : NSObject

+ (NSDictionary<NSString *, NSNumber *> *)themePaletteForDark:(BOOL)dark
    NS_SWIFT_NAME(themePalette(dark:));

- (NSDictionary *)evaluate:(NSString *)expression;
- (NSDictionary *)applyUnary:(NSString *)operation
                  expression:(NSString *)expression;
- (NSDictionary *)applyScientific:(NSString *)operation
                       expression:(NSString *)expression
                          degrees:(BOOL)degrees;

- (NSDictionary *)evaluateProgrammer:(NSString *)expression
                                base:(NSInteger)base
                               width:(NSInteger)width
                       signedDisplay:(BOOL)signedDisplay;

- (void)memoryClear;
- (void)memoryAdd:(double)value;
- (void)memorySubtract:(double)value;
- (double)memoryRecall;
- (NSString *)formatValue:(double)value NS_SWIFT_NAME(format(value:));

- (NSString *)historyText;
- (void)clearHistory;

@end

NS_ASSUME_NONNULL_END
