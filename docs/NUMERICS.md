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

Standard uses C++ double and relies on conventional IEEE-754 binary64 behaviour. Scientific uses Calculator's explicit arbitrary-precision real/complex domain backed by Boost.Multiprecision cpp_bin_float / cpp_complex. The interactive controller currently evaluates at 50 decimal digits. The core evaluation API accepts requested precisions from 16 through the maintained 1000-digit ceiling, and release-CI oracle coverage exercises that full range.

Scientific decimal literals are parsed directly into that multiprecision domain, and intermediate arithmetic/transcendental/complex results are not silently routed through binary64. The Session/Controller boundary stores real and imaginary components as decimal text so the shared public C++ state preserves the precise value without exposing a third-party multiprecision ABI.

Formatting does not create precision. Standard binary64 presentation remains distinct from Scientific multiprecision presentation, Programmer fixed-width integers and the specialist exact/arbitrary tool domains.

Standard therefore has two deliberately separate decimal-output paths. `format_value()` and display preferences are presentation and may round, group or otherwise format a value for a person. `serialize_value()` is computational state: it emits the shortest locale-independent decimal that round-trips to the exact same finite binary64 value. Interactive unary operations, memory recall and any other binary64 state boundary must use the latter and must never reconstruct computation from rounded display text.

## Shared constant authority

Constant names, aliases, binary64 presentation values, physical units and definition kind have one canonical catalogue in `calculator.*`. Scientific resolves that same catalogue at its active precision: mathematical constants are constructed mathematically, decimal physical constants are constructed from their canonical decimal spelling, and reduced Planck's constant is derived from exact SI Planck h divided by 2π. This avoids a second drifting list of Scientific constant values.

## Binary64 decimal token conversion

In the binary64 expression path, Calculator owns grammar and decides when a numeric operand is expected. Common 1.19.24 owns the product-neutral decimal-token mechanic through `infiltratr_parse_double_token()`: it advances a cursor across one finite ASCII-decimal token and performs the same exact locale-independent binary64 conversion used by Common's complete-string parser.

That path therefore no longer carries a private binary64 decimal scanner. NaN, infinity, hexadecimal floating-point syntax, malformed exponents, overflow and underflow-to-zero are rejected by the shared conversion contract while operator precedence and expression structure remain Calculator-owned. Scientific is deliberately different: its decimal literals are parsed directly into the multiprecision real/complex domain and never pass through this binary64 conversion.

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

Postfix `%` divides a value by 100 in expression mode. Scientific factorial accepts real non-negative integral inputs through the maintained 100000 safety ceiling and evaluates the product in the multiprecision real domain; invalid, complex or oversized factorial inputs fail explicitly.

Scientific elementary and complex operations execute in the shared multiprecision backend. Trigonometric and inverse-trigonometric functions support radians, degrees and gradians; hyperbolic functions are unit-independent. Roots, powers, reciprocal, logarithmic/exponential, real/imaginary/conjugate and rounding-related operations retain high precision where mathematically defined. Complex logarithms and non-integer powers use the documented principal branch with argument in (-pi, pi], so an exact negative real lies on the +pi side of the cut. Exact integer powers use exponentiation by squaring, real half-integer powers of exact negative reals preserve their exact quadrantal phase, and exact trigonometric quadrant arguments are normalised to exact 0/±1 values rather than leaking tiny finite-pi residues. Domain-invalid or non-finite results are explicit calculation failures rather than values silently propagated into the UI.

Recursive grammar descent is explicitly bounded in both the Scientific expression parser and Programmer parser. Scientific parser and user-function recursion limits are deliberately conservative for the smallest supported native stacks, including Windows, and excessive nesting fails deterministically with `expression nesting too deep` or `function recursion too deep` rather than consuming unbounded native stack. User-defined function calls remain internal full-precision expression boundaries; factoring an expression into a function does not introduce an extra requested-precision rounding step.

## Standard arithmetic semantics

Standard mode is intentionally smaller than Scientific, but it is not a second definition of arithmetic. It uses the binary64 expression grammar with named constants, variables and functions disabled. Conventional operator precedence applies: exponentiation before multiplication/division before addition/subtraction, with parentheses providing explicit grouping.

Postfix `%` has one literal meaning in Standard as it does in expression mathematics: divide the preceding value by 100. Therefore `100 + 10%` is `100.1`, `100 * 10%` is `10`, and `200 + 200 / 2` is `300`.

This rule is deliberate for the greenfield Calculator design. Historical immediate-execution behaviour is not preserved merely for compatibility with older four-function calculators, because an expression displayed as infix mathematics should evaluate according to that mathematics.

The same principle applies after an operation. Display rounding is not state rounding. If Standard computes a finite binary64 value, subsequent operations consume that exact binary64 value, serialized losslessly when it must re-enter expression state. A user preference such as fixed three-decimal display or thousands grouping may change what is shown, but cannot change later arithmetic. Standard memory likewise retains a finite binary64 value exactly; overflow is rejected without mutating the register, and a Scientific memory value outside binary64 range remains valid Scientific memory but is unavailable for Standard recall rather than being replaced with a fabricated approximation.

## Programmer domain

Programmer mode operates on bit patterns constrained to the selected 8, 16, 32 or 64-bit width.

Arithmetic intentionally wraps by masking to the selected width. This is the domain semantics, not an unchecked-overflow accident. Signed display interprets the final masked pattern using two's-complement presentation; it does not change the stored bit pattern or evaluation domain.

Numeric literals use the selected radix. Matching binary/octal/hex prefixes are accepted only when consistent with that radix. Shift counts are decimal control quantities and values outside the implementation's safe shift contract are rejected before executing a native shift. NAND and NOR are width-masked after each operation. Rotate-left and rotate-right are also width-aware and reduce the rotate count modulo the selected width, avoiding undefined native shift behaviour.

Programmer semantics should be validated with exact integer expectations rather than floating-point tolerances.

## Constants and elementary functions

`pi` and `e` are built-in constants and are reserved identifiers: variable assignment cannot replace them. Standard/supporting binary64 transforms use the C++ standard mathematical facilities over `double`; Scientific evaluates its corresponding real/complex operations in the Boost.Multiprecision backend and retains the configured precision through intermediate results.

The project does not claim bit-for-bit transcendental equality across different binary64 standard libraries or multiprecision backend implementations. Regression tests therefore use mathematically justified tolerances appropriate to the represented domain, while exact parser/Programmer contracts use exact comparisons where appropriate.

New constants or functions should document source/definition, accepted domain, representation and meaningful boundary cases before being treated as complete.

## Display formatting

Calculator keeps result formatting in the shared core/controller rather than platform shells. Standard retains its locale-independent binary64 formatter. Scientific formats the retained arbitrary-precision real/complex components directly in General, Fixed, Scientific or Engineering form using the selected precision and display preferences; formatting never requires conversion through double.

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
- Calculator-owned regression tests for Standard capability restrictions, conventional precedence and literal postfix percentage semantics.

External references establish specific rules; they do not make another calculator implementation the specification for this project.

## Adding a numerical capability

Before a new mathematical capability is accepted, establish the numeric representation and units, grammar/interaction semantics, valid and invalid domains, overflow/underflow or exactness policy, display policy, authoritative definition/reference, cross-platform expectations, and deterministic reference values/boundary tests.

If those cannot be stated precisely, the capability is not ready to be advertised as complete.


## Programmer multi-radix representation

A Programmer result is one fixed-width masked bit pattern. The representation view does not re-evaluate the expression four times: it renders that one value as binary, octal, decimal and hexadecimal. Binary, octal and hexadecimal remain unsigned bit-pattern views. The decimal row follows the current signed/unsigned presentation toggle. This keeps representation switching observational rather than computational.


## Engineering notation

Engineering notation is a presentation of the existing finite binary64 result, not a separate numerical domain. The exponent is an integer multiple of three and the mantissa is scaled accordingly. Formatting is locale-independent and deterministic across supported platforms.

For example, `12345` is presented as `12.345e+03` and `0.00123` as `1.23e-03`. The canonical decimal and scientific representations remain available alongside it in Additional Results.

The engineering formatter derives its mantissa and exponent from one rounded scientific representation instead of computing a decimal scale with `pow(10, exponent)`. This keeps the smallest finite binary64 subnormals representable instead of underflowing an intermediate scale to zero, and it lets decimal rounding carry across engineering exponent boundaries before the mantissa is rearranged.

Boundary regressions cover zero, signed finite values, the smallest positive subnormal, the largest finite binary64 value and a mantissa-rounding carry into the next engineering exponent.

Additional Results never invents precision: Standard rows describe the same binary64 value, while Scientific rows are alternative presentations of the same retained arbitrary-precision real/complex value.


## Advanced-tool precision boundaries

Unit conversion evaluates source values, exact decimal/ratio factors and affine offsets in the Scientific multiprecision domain. Graph and real-root probes also use the Scientific evaluator. Native graph coordinates and the deterministic interval search remain finite binary64 geometry, so high-precision expression semantics are retained up to the explicit geometry boundary rather than being reduced at parse/evaluation entry.

Calendar date arithmetic is integer civil-date arithmetic, not floating-point duration arithmetic. Month/year shifts clamp to the valid last day of the target month and stay within years 1..9999.

## Advanced tool numeric domains

The Tools workbench deliberately uses more than one numeric representation.

Engineering, dimensional conversion, statistics, graphing, real equation solving and complex arithmetic use finite binary64 values and reject malformed or non-finite inputs. Unit conversion is dimension-checked before applying scale/offset transformations; temperature conversions use affine transformations rather than multiplicative factors alone.

Network IPv4/CIDR operations use exact 32-bit address arithmetic and 64-bit address counts. Storage allocation calculations use checked 64-bit integer arithmetic for byte/cluster counts and long-double intermediates only for human capacity conversion.

Civil-date operations validate Gregorian dates in years 1 through 9999 and use integer day-number transforms. Unix conversion is explicitly UTC and requires the documented `YYYY-MM-DDTHH:MM:SSZ` form.

### Exact decimal/rational arithmetic

Exact Decimal parses decimal literals directly into integer numerator / power-of-ten denominator pairs. Addition, subtraction, multiplication and division operate on those integer ratios, so `0.1 + 0.2` is represented exactly as `3/10`, not as a rounded binary64 approximation. A terminating decimal projection is emitted when the resulting denominator is a power of ten. Results are bounded by the maintained 20,000-digit safety ceiling.

### Arbitrary-precision integer arithmetic

Arbitrary Precision is an integer domain implemented with base-10^9 limbs. It supports signed `+`, `-`, `*`, right-associative non-negative integer powers, parentheses and factorial through 1000. It does not pretend integer division is exact when it has not been requested as part of the contract. Results are capped at 20,000 decimal digits to bound memory and run time.

### Complex arithmetic

Complex values are explicit `real,imaginary` pairs. The current workbench supports add/subtract/multiply/divide, conjugate, magnitude, argument and polar reporting. Components are binary64; division by zero is an explicit failure.

### Graphing and equation solving

Graphing evaluates the existing Scientific grammar with a supplied `x` variable over 2..4096 evenly spaced samples. Invalid/non-finite samples are retained as discontinuity markers rather than connected through by the renderer.

Equation solving searches a caller-specified real interval with deterministic segmentation and safeguarded bisection on validated sign changes. Returned candidates are re-evaluated before admission and deduplicated. The solver is a real numerical root finder, not a symbolic algebra system; absence of a validated root in the interval is reported explicitly.
