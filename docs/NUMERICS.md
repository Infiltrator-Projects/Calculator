<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Numerical semantics and evidence

Calculator exposes explicit computational contracts rather than unexplained numeric output. Each mode must define its representation, grammar or interaction rule, domain limits, error behaviour and validation strategy.

This document is the specialist source of truth for numerical behaviour and evidence. [ARCHITECTURE.md](ARCHITECTURE.md) owns module boundaries, [DESIGN.md](DESIGN.md) owns project-wide rationale, and code/tests remain authoritative for executable behaviour.

## Evidence hierarchy

A numerical rule should prefer evidence in this order:

1. an applicable mathematical or computing standard whose semantics are explicit;
2. the documented C++/platform library contract used by the implementation;
3. established mathematical definitions or independently checkable reference values;
4. a documented Calculator interaction rule when calculator-style behaviour is conventional but not standardised;
5. unsupported/error when the project cannot justify deterministic behaviour.

A familiar result, another calculator's output or a plausible display string is not enough by itself to establish correctness.

## Real-number representation

Standard and Scientific currently use C++ `double` and rely on the supported release platforms providing the conventional IEEE-754 binary64 representation.

Binary64 provides finite precision and cannot represent every decimal rational exactly. Calculator therefore distinguishes the internal binary value, the mathematical/domain validity of an operation, and the user-facing display string.

Formatting does not make an inexact binary value exact. A feature requiring exact decimal, arbitrary precision, complex values or another domain should introduce that representation explicitly rather than hiding the requirement behind display rounding.

The primary representation authority is IEEE 754 / ISO/IEC 60559. The project currently targets the binary64 behaviour provided by the supported C++ toolchains and platforms rather than claiming portability to an arbitrary non-IEC-60559 `double` implementation.

## Decimal token conversion

Calculator owns expression grammar and decides when a numeric operand is expected. Common 1.19.8 owns the product-neutral decimal-token mechanic through `infiltratr_parse_double_token()`: it advances a cursor across one finite ASCII-decimal token and performs the same exact locale-independent binary64 conversion used by Common's complete-string parser.

Calculator therefore no longer carries a private decimal scanner. NaN, infinity, hexadecimal floating-point syntax, malformed exponents, overflow and underflow-to-zero are rejected by the shared conversion contract while operator precedence and expression structure remain Calculator-owned.

## Scientific expression grammar

The expression evaluator implements:

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

Exponentiation is right-associative and binds more tightly than a leading sign: `-2^2` means `-(2^2)`, while `2^-2` is valid.

Postfix `%` divides a value by 100 in expression mode. Factorial accepts non-negative integral real values through 170; larger factorials exceed the finite range of the current binary64 domain.

The current function set delegates elementary transcendental operations to the C++ standard math library through one Calculator-owned `apply_real_function()` contract. Both the expression parser and interactive unary/scientific controls use that same implementation. Trigonometric and inverse-trigonometric functions support radians, degrees and gradians; hyperbolic functions are unit-independent. The same core owns square/cube, square/cube root, reciprocal, natural/base-10 logarithms, e/2/10 exponentials, absolute value, floor and ceiling. Domain-invalid or non-finite results are calculation failures rather than values silently propagated into the UI.

Recursive grammar descent is explicitly bounded in both the Scientific expression parser and Programmer parser. Inputs whose nested parentheses or unary operators exceed the maintained parser limit fail with `expression nesting too deep` rather than consuming unbounded native stack.

## Standard immediate semantics

Standard mode is intentionally not the Scientific grammar. Binary operations are committed left-to-right as entered, matching conventional immediate desktop-calculator interaction.

Percentage is contextual: addition/subtraction interpret the right-hand percentage relative to the current accumulated value, while multiplication/division interpret it as a fraction of 100.

Thus `100 + 10%` produces 110 while `100 * 10%` produces 10. This is a Calculator interaction contract protected by regression tests; it is not presented as a universal mathematical grammar.

## Programmer domain

Programmer mode operates on bit patterns constrained to the selected 8, 16, 32 or 64-bit width.

Arithmetic intentionally wraps by masking to the selected width. This is the domain semantics, not an unchecked-overflow accident. Signed display interprets the final masked pattern using two's-complement presentation; it does not change the stored bit pattern or evaluation domain.

Numeric literals use the selected radix. Matching binary/octal/hex prefixes are accepted only when consistent with that radix. Shift counts are decimal control quantities and values outside the implementation's safe shift contract are rejected before executing a native shift. NAND and NOR are width-masked after each operation. Rotate-left and rotate-right are also width-aware and reduce the rotate count modulo the selected width, avoiding undefined native shift behaviour.

Programmer semantics should be validated with exact integer expectations rather than floating-point tolerances.

## Constants and elementary functions

`pi` and `e` are current built-in constants and are reserved identifiers: variable assignment cannot replace them. Trigonometric, inverse-trigonometric, hyperbolic/inverse-hyperbolic, logarithmic, exponential, root, rounding and absolute-value functions use the C++ standard math implementation over the binary64 domain.

The project does not claim bit-for-bit transcendental equality across different standard libraries. Regression tests should use mathematically justified tolerances for real-valued functions while exact parser/Programmer contracts use exact comparisons where appropriate.

New constants or functions should document source/definition, accepted domain, representation and meaningful boundary cases before being treated as complete.

## Display formatting

Calculator uses one locale-independent real-result display formatter across GTK, Win32 and iPhone. Normal display uses general notation with 15 significant digits; Scientific F-E mode uses a second shared formatter with explicit scientific notation so platform shells do not invent their own exponent formatting.

Fifteen significant digits are a presentation choice, not a round-trip guarantee for every binary64 value. Internal evaluation continues to use the underlying binary64 value until a new numeric domain explicitly replaces it.

Platform shells must not introduce independent numeric formatting rules that change the represented Calculator result.

## Errors and unavailable results

Malformed syntax, excessive parser nesting, division by zero, invalid function domains, factorial violations, unknown identifiers/functions, attempts to assign reserved constants, non-finite real results, invalid Programmer digits and unsafe shift counts are explicit failures.

A plausible substitute result is not an acceptable fallback. If a requested future numeric domain cannot establish a justified value under its contract, unsupported/error is preferable to invented precision.

## Cross-platform numerical validation

Portable core tests are the primary numerical oracle for platform parity. Native shells should present the same core result rather than independently re-evaluating expressions.

Validation should distinguish exact contracts from approximate real-number contracts:

- parser structure, integer bit patterns, radix rules and state transitions should normally compare exactly;
- transcendental/real arithmetic should use a tolerance justified by the operation and expected magnitude;
- UI string tests should test the documented display contract rather than infer internal binary equality from text.

A Simulator or different host standard library may provide useful cross-platform evidence without proving bit-identical results on every physical platform.

## References

The maintained numerical basis includes:

- IEEE Std 754-2019 / ISO/IEC 60559:2020 for floating-point arithmetic concepts and binary formats;
- ISO/IEC 14882:2017 (C++17) and the supported standard-library implementations for C++ `double`, `<cmath>` and `std::to_chars` interfaces used by the project;
- conventional mathematical definitions for elementary functions and operator precedence; and
- Calculator-owned regression tests for interaction semantics such as contextual percentage and immediate left-to-right evaluation where no single mathematical standard defines desktop-calculator behaviour.

External references establish specific rules; they do not make another calculator implementation the specification for this project.

## Adding a numerical capability

Before a new mathematical capability is accepted, establish the numeric representation and units, grammar/interaction semantics, valid and invalid domains, overflow/underflow or exactness policy, display policy, authoritative definition/reference, cross-platform expectations, and deterministic reference values/boundary tests.

If those cannot be stated precisely, the capability is not ready to be advertised as complete.


## Programmer multi-radix representation

A Programmer result is one fixed-width masked bit pattern. The representation view does not re-evaluate the expression four times: it renders that one value as binary, octal, decimal and hexadecimal. Binary, octal and hexadecimal remain unsigned bit-pattern views. The decimal row follows the current signed/unsigned presentation toggle. This keeps representation switching observational rather than computational.
