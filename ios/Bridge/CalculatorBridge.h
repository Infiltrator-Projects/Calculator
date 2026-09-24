/* SPDX-License-Identifier: GPL-3.0-or-later */
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CalculatorBridge : NSObject

// Bridge dictionaries are a deliberately narrow Objective-C transport layer;
// they contain value copies only and never expose C++ object lifetimes.
// themePalette/designMetrics use the stable keys consumed by ContentView.
+ (NSDictionary<NSString *, NSNumber *> *)themePaletteForDark:(BOOL)dark
    NS_SWIFT_NAME(themePalette(dark:));
+ (NSDictionary<NSString *, NSNumber *> *)designMetrics
    NS_SWIFT_NAME(designMetrics());

// snapshot returns value copies under these stable keys: mode, expression,
 // display, status, fault, angleUnit, scientificSecond, scientificHyperbolic,
 // scientificNotation, programmerBase, programmerWidth and programmerSigned.
 // Callers should treat unknown future keys as additive and must not infer
 // calculation rules from the transport representation.
- (NSDictionary *)snapshot;
- (void)setExpression:(NSString *)expression;
- (void)selectMode:(NSInteger)mode;
- (void)pressKey:(NSString *)key;
- (BOOL)isKeyEnabled:(NSString *)key;
- (NSString *)additionalResultsText;
- (NSString *)programmerRepresentationsText;

// Shared Controller persistence/precision contract. iOS owns only the native
// UserDefaults storage location; the serialized format remains C++-owned.
- (NSString *)persistentStateText;
- (BOOL)loadPersistentStateText:(NSString *)text;
- (void)setScientificDigits:(NSInteger)digits;

// Tool catalogue rows contain index/name/prompt/example. Evaluation results
 // contain ok/output/error/points, where each point carries x/y/valid. These are
 // serialized views of the shared C++ engine; the bridge does not evaluate or
 // format tool mathematics itself.
- (NSArray<NSDictionary *> *)advancedToolCatalog;
- (NSDictionary *)evaluateAdvancedToolAtIndex:(NSInteger)index
                                      input:(NSString *)input
    NS_SWIFT_NAME(evaluateAdvancedTool(index:input:));

// History indexes are newest-first, matching Controller history APIs.
- (NSString *)historyText;
- (NSArray<NSDictionary *> *)historyEntries;
- (BOOL)recallHistoryAtIndex:(NSInteger)index
    NS_SWIFT_NAME(recallHistory(at:));
- (void)clearHistory;

@end

NS_ASSUME_NONNULL_END
