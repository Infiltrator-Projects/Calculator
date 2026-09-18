<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Implementation Notes

This document records implementation choices that are not obvious from the public interface alone. It describes the current behaviour; historical release details belong in the changelog and release notes.

## Decimal token conversion

Calculator owns expression tokenisation because a parser must know where a numeric literal ends and an operator begins. Common's `infiltratr_parse_double()` deliberately validates a complete decimal string.

`parse_decimal_token()` therefore performs only boundary recognition for the accepted decimal grammar: optional sign when requested, decimal digits, optional fractional part and optional exponent. The extracted token is then passed to Common for deterministic locale-independent binary64 conversion.

This division avoids `strtod()` locale dependence without moving Calculator's expression grammar into Common. NaN, infinity, hexadecimal floating-point syntax, malformed exponents, overflow and underflow-to-zero are rejected by the conversion contract.

## Scientific expression grammar

The expression evaluator implements the following precedence model:

```text
expression  := term (("+" | "-") term)*
term        := unary (("*" | "/") unary)*
unary       := ("+" | "-") unary | power
power       := postfix ["^" unary]
postfix     := primary ("%" | "!")*
primary     := number
             | identifier
             | identifier "(" expression ")"
             | "(" expression ")"
```

Exponentiation is right-associative. It binds more tightly than unary sign, so `-2^2` is `-(2^2)`, while `2^-2` is valid and evaluates with a negative exponent.

Postfix `%` divides a value by 100 in expression mode. Factorial accepts non-negative integral binary64 values up to 170; larger values would overflow the current representation.

The built-in constants are `pi` and `e`. The parser currently recognises trigonometric, inverse-trigonometric, hyperbolic, root, logarithmic, exponential and absolute-value functions implemented by the C++ standard math library.

All successful expression results must be finite.

## Standard immediate semantics

`evaluate_immediate()` is intentionally simpler than the expression grammar. It accepts signed decimal operands and the binary operators `+`, `-`, `*`, `/` and `^`, applying them strictly from left to right.

Percentage is contextual:

- for addition/subtraction, the right-hand percentage is interpreted relative to the current accumulated value;
- for multiplication/division, it is interpreted as a fraction of 100.

For example, `100 + 10%` produces 110, while `100 * 10%` produces 10. This behaviour models conventional desktop-calculator interaction and is kept separate from Scientific expression precedence.

## Programmer evaluation

Programmer mode evaluates unsigned bit patterns within the selected 8, 16, 32 or 64-bit width.

The precedence order is bitwise OR, XOR, AND, shifts, additive operations, multiplicative operations, unary operations and primary values. Results are masked to the selected width throughout evaluation, so overflow and underflow intentionally wrap in two's-complement bit-pattern space.

Numeric literals use the selected radix. Matching `0x`, `0b` and `0o` prefixes are accepted only when they agree with the selected radix.

Shift counts are decimal quantities independent of the selected input radix. They are parsed at full 64-bit width and values of 64 or greater are rejected before shifting.

Signed display does not alter evaluation. It interprets the final masked bit pattern as two's-complement only when formatting the result.

## Session semantics

`Session` owns variables, memory and bounded history.

Direct assignment uses `name=expression`. The identifier must begin with an alphabetic character or underscore and continue with alphanumeric characters or underscores; the assignment operator immediately follows the identifier. A successful assignment updates the variable after evaluation.

Every session evaluation, including an error, is recorded in history. When the configured history limit is exceeded, the oldest entry is discarded. A zero history limit is normalised to one.

Memory tracks both a numeric value and whether it has been explicitly set. This distinction allows the UI to disable recall/clear before memory has ever been populated even though the stored numeric value is initially zero.

Session state is process-local and is not currently persisted between application launches.

## Desktop UI controller

The desktop controller is the authoritative interaction state machine for GTK and Win32. It owns mode, expression, result/status, DEG/RAD state, Programmer radix/width/signedness, command enablement and dispatch.

The platform shells render `ViewState`, provide the current cursor position and dispatch `Command` values. `DispatchResult` returns the cursor position the shell should restore and whether the expression text changed.

This keeps calculator behaviour testable without constructing GTK or Win32 controls.

## iPhone bridge

SwiftUI calls the shared C++ engines through `CalculatorBridge`. The bridge converts Objective-C/Swift values to the C++ domain and returns result dictionaries suitable for the existing Swift model.

The bridge also exposes Common's canonical Day/Night semantic palette. Swift resolves System appearance using `ColorScheme` but does not maintain a private copy of Common colour values.

## Display formatting

Calculator owns one result-display formatter for its real-number domain. It uses locale-independent general notation with 15 significant digits. GTK, Win32 and iPhone consume that formatter rather than maintaining platform-specific numeric formatting rules.

This is intentionally a Calculator presentation contract rather than Common's scalar formatter, whose fixed-decimal semantics serve a different purpose.

## Error propagation

Core evaluators return structured success/error results. UI layers display those errors but do not convert failed results into substitute values.

Domain checks include division by zero, malformed input, missing parentheses, unknown identifiers/functions, invalid function domains, factorial constraints, non-finite real results, invalid Programmer digits and out-of-range shift counts.
