/* SPDX-License-Identifier: GPL-3.0-or-later */
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CalculatorBridge : NSObject

+ (NSDictionary<NSString *, NSNumber *> *)themePaletteForDark:(BOOL)dark
    NS_SWIFT_NAME(themePalette(dark:));

- (NSDictionary *)snapshot;
- (void)setExpression:(NSString *)expression;
- (void)selectMode:(NSInteger)mode;
- (void)pressKey:(NSString *)key;
- (BOOL)isKeyEnabled:(NSString *)key;

- (NSString *)historyText;
- (void)clearHistory;

@end

NS_ASSUME_NONNULL_END
