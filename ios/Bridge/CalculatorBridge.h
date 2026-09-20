/* SPDX-License-Identifier: GPL-3.0-or-later */
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CalculatorBridge : NSObject

+ (NSDictionary<NSString *, NSNumber *> *)themePaletteForDark:(BOOL)dark
    NS_SWIFT_NAME(themePalette(dark:));
+ (NSDictionary<NSString *, NSNumber *> *)designMetrics
    NS_SWIFT_NAME(designMetrics());

- (NSDictionary *)snapshot;
- (void)setExpression:(NSString *)expression;
- (void)selectMode:(NSInteger)mode;
- (void)pressKey:(NSString *)key;
- (BOOL)isKeyEnabled:(NSString *)key;
- (NSString *)additionalResultsText;
- (NSString *)programmerRepresentationsText;

- (NSArray<NSDictionary *> *)advancedToolCatalog;
- (NSDictionary *)evaluateAdvancedToolAtIndex:(NSInteger)index
                                      input:(NSString *)input
    NS_SWIFT_NAME(evaluateAdvancedTool(index:input:));

- (NSString *)historyText;
- (NSArray<NSDictionary *> *)historyEntries;
- (BOOL)recallHistoryAtIndex:(NSInteger)index
    NS_SWIFT_NAME(recallHistory(at:));
- (void)clearHistory;

@end

NS_ASSUME_NONNULL_END
