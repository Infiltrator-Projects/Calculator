/* SPDX-License-Identifier: GPL-3.0-or-later */
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ICCalculatorBridge : NSObject

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

- (NSString *)historyText;
- (void)clearHistory;

@end

NS_ASSUME_NONNULL_END
