<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Numerical semantics and evidence

Calculator exposes explicit computational contracts rather than unexplained numeric output. Each domain defines its representation, grammar, limits, failure semantics and validation strategy. This document is the specialist source of truth for those numerical contracts; [ARCHITECTURE.md](ARCHITECTURE.md) owns module boundaries and [DESIGN.md](DESIGN.md) owns project-wide rationale.

## Evidence hierarchy

A numerical rule should prefer evidence in this order:

1. an applicable mathematical or computing standard with explicit semantics;
2. the documented contract of the C++/third-party mechanism actually used;
3. established mathematical definitions or independently checkable reference values;
4. a documented Calculator interaction rule where behaviour is conventional but not standardised; and
5. explicit unsupported/error when deterministic behaviour cannot be justified.

Another calculator's output is useful comparative evidence, not this project's specification.

## Numeric domains

Calculator deliberately uses several representations because no one representation has the strongest contract for every mode.

- **Standard** uses finite IEEE-754 binary64 (`double`).
- **Scientific** uses Boost.Multiprecision arbitrary-precision real/complex values (`cpp_bin_float`/`cpp_complex`), 50 decimal digits by default and selectable from 16 through 1000 digits.
- **Programmer** uses explicit 8/16/32/64-bit masked integer bit patterns.
- **Exact Decimal** uses exact integer numerator/denominator pairs backed by Boost.Multiprecision `cpp_int`.
- **Arbitrary Precision** uses signed Boost.Multiprecision `cpp_int` values.

Boost is an implementation mechanism, not a public ABI. Scientific crosses Session/Controller/platform boundaries as real/imaginary decimal text; exact/integer tools expose Calculator-owned text results.

## Standard computational state versus presentation

Within Standard, the binary64 value is authoritative. `serialize_value()` emits the shortest locale-independent decimal that round-trips to exactly the same finite binary64 value and is used whenever a result must re-enter computational expression state. `format_value()` is presentation and may round or group for a person.

Presentation is never reparsed as computation. A fixed-decimal or grouped display preference may change what is shown without changing subsequent arithmetic. Memory retains a Scientific value plus an optional finite binary64 view; complex values, overflow and non-zero underflow-to-zero make that view unavailable. Representable subnormals remain valid.

## Shared constant authority

Constant names, aliases, binary64 values, units and definition kinds have one canonical catalogue in `calculator.*`. Scientific resolves the same catalogue at its active precision: mathematical constants are constructed mathematically, canonical decimal physical constants are constructed from their decimal definition, and reduced Planck's constant is derived from Planck `h / (2*pi)`.

A new constant must not introduce a second Scientific-only catalogue merely because the representation differs.

## Binary64 token conversion

Calculator owns binary64 expression grammar and decides when a number is expected. Common 1.19.57 owns product-neutral locale-independent decimal token conversion through `infiltratr_parse_double_token()` and the related complete numeric parsers.

This rejects NaN, infinity, hexadecimal floating-point spelling, malformed exponents, overflow and underflow-to-zero according to the shared conversion contract while keeping precedence and expression structure Calculator-owned. Scientific literals do not pass through binary64; they enter the multiprecision domain directly.

## Expression grammar and precedence

Standard and Scientific use the same operator meaning for their shared arithmetic grammar:

```text
expression  := term (("+" | "-") term)*
term        := unary (("*" | "/") unary)*
unary       := ("+" | "-") unary | power
power       := postfix ["^" unary]
postfix     := primary ("%" | "!")*
primary     := number | identifier | identifier "(" expression ")" | "(" expression ")"
```

Exponentiation is right-associative and binds more tightly than a leading sign: `-2^2` is `-(2^2)`, while `2^-2` is valid. Postfix `%` means division by 100; it is not contextual pending-operator percentage. Therefore Standard evaluates `100 + 10%` as `100.1`, `100 * 10%` as `10`, and `200 + 200 / 2` as `300`.

Standard disables named constants, variables and functions. Scientific enables them and performs the same shared operator semantics in its wider numeric domain.

## Scientific semantics

Scientific decimal literals and intermediates remain multiprecision. Trigonometric and inverse-trigonometric functions support radians, degrees and gradians; hyperbolic functions are unit-independent. Reciprocal trig/hyperbolic families execute in the same backend.

Complex logarithms and non-integer powers use the documented principal branch with argument in `(-pi, pi]`. Exact integer powers use exponentiation by squaring. Real half-integer powers of exact negative reals preserve exact quadrantal phase, and exact trigonometric quadrant arguments are normalised to exact `0`/`±1` values rather than exposing finite-pi residue as mathematical signal.

Scientific factorial accepts real, non-negative integral input through the maintained 100000 operation ceiling. Invalid, complex or oversized inputs fail explicitly.

User-defined functions are evaluated in the same active multiprecision domain. Entering a function boundary does not create an additional requested-precision rounding boundary.

Session reserves the Scientific evaluator's complete built-in catalogue, including dynamic `logN`/`rootN` forms, constants, `i`, `rand` and `_`. Reusable variable components reload at the supported maximum precision, preserving values produced at any requested precision through 1000 digits rather than rounding them to the 50-digit default.

After Equals, Controller retains the value returned by that evaluation. Additional Results, memory and subsequent unary operations use it directly; synchronizing unchanged input does not evaluate `_`, assignments or `rand` again. A changed expression, angle unit or calculation precision creates a fresh preview context.

## Evaluation resource semantics

Multiprecision Scientific and iterative/arbitrary-precision Advanced Tools share `EvaluationBudget`. Its defaults provide:

- 8192 UTF-8 input bytes;
- a cumulative five-million-step work budget;
- Scientific/Advanced Tool recursive parse depth of 16;
- user-function recursion depth of 16;
- Scientific factorial ceiling 100000;
- generic exponent ceiling 100000000;
- a non-relaxable product maximum of 20,000 materialised arbitrary-precision decimal digits; and
- optional monotonic timeout plus cooperative cancellation.

Callers may tighten limits but cannot use a custom budget to relax the 20,000-digit product safety ceiling. Graph/root probes share one budget across the complete user operation instead of resetting it for each sample. Exact/arbitrary parsers account for recursive unary/parenthesised/power descent against the same parse-depth authority, and iterative big-integer work checks the budget cooperatively.

Standard's binary64 parser has a separate deliberate depth policy because it is a different domain and workload; sharing one numeric grammar does not require pretending all resource contracts are identical.

## Programmer domain

Programmer values are masked to the selected 8, 16, 32 or 64-bit width after operations. Wraparound is deliberate domain semantics, not native signed-overflow behaviour. Signed decimal display interprets the same final bit pattern using two's-complement presentation; hexadecimal, octal and binary always render that pattern in the selected radix. Signedness does not change the stored value.

Numeric literals use the selected radix and optional prefixes must agree with that radix. Shift counts are decimal control quantities and unsafe counts are rejected before native shifting. `>>` is logical right shift; `ashr` is explicit arithmetic-right shift with sign extension from the selected width. NAND, NOR and XNOR are width-masked. Rotates reduce counts modulo width. Byte swap reverses bytes only inside the selected width, so 8-bit values are unchanged. `%` is integer remainder with zero-divisor failure.

Programmer expectations are exact integer/bit-pattern comparisons, never floating tolerances.

## Exact Decimal

Exact Decimal parses decimal literals directly into exact numerator/power-of-ten denominator values. `+`, `-`, `*`, `/`, unary signs and parentheses operate on those rational values; normalization uses exact integer GCD. `0.1 + 0.2` is therefore exactly `3/10`.

Decimal exponent spelling is supported within the maintained scale limit. A terminating decimal projection is emitted when the normalized denominator contains only factors of 2 and 5 and the projection remains within product safety bounds. The exact fraction remains authoritative even when an optional terminating-decimal rendering would be too large.

## Arbitrary Precision

Arbitrary Precision supports signed integer `+`, `-`, `*`, right-associative non-negative integer `^`, parentheses and postfix `!`. Exponents are limited to 100000 and factorial to 1000 by this tool's domain contract, which is intentionally stricter than the generic Scientific ceilings.

The generic big-integer mechanism is Boost.Multiprecision `cpp_int`; Calculator does not maintain its own limb arithmetic, long multiplication, division or GCD engine. Product-specific grammar, limits, output semantics and failure wording remain local.

## Advanced Tools precision boundaries

Unit conversion evaluates source values, exact decimal/ratio factors and affine offsets in the Scientific multiprecision domain. Definitions such as `5/9` and `pi/180` are therefore not first rounded into binary64.

Graph and real-root expressions use Scientific evaluation with supplied `x`. Samples are evaluated under one cumulative budget. Finite results cross explicitly into binary64 only where native graph geometry and deterministic real-interval search require it. Invalid/non-finite samples and non-zero values that underflow to zero remain discontinuity markers rather than fabricated zero points. Intervals whose span is not finite binary64 are rejected; sampling multiplies by a bounded fraction to avoid intermediate coordinate overflow.

Equation solving is a deterministic numerical real-root finder: interval segmentation identifies candidates, sign-changing intervals are refined by safeguarded bisection, stationary candidates are separately refined, results are re-evaluated before admission and duplicates are coalesced. Scientific residuals are normalized against a non-zero Scientific sample before binary64 search, so multiplying a function by a tiny constant does not turn every scan point into a root. Scan points require exact evaluated zero; bracket admission uses relative residual evidence and stationary admission uses local curvature and coordinate uncertainty. Invalid probes reject a bracket, including sampled poles. This produces numerical estimates within finite precision and sampling limits; it cannot certify all roots or distinguish every near-zero minimum from a root.

Engineering and transfer tools accept finite binary64 operands and require a finite, non-underflowed binary64 result. Derived formulae use Scientific arithmetic before that checked boundary where binary64 intermediate products would overflow or underflow. Statistics reject non-finite accumulators, variances, quantiles or ranges instead of formatting infinity/NaN as success.

Complex Advanced Tools are adapters over the Scientific multiprecision engine rather than a second `std::complex<double>` calculation path. Add/subtract/multiply/divide, conjugate, magnitude, argument and polar results therefore inherit Scientific complex-domain semantics.

Network IPv4/CIDR operations use exact 32-bit address arithmetic with 64-bit counts. Storage cluster/allocation counts use checked integer arithmetic; human-capacity conversions reuse the canonical multiprecision digital-storage definitions. Civil-date operations use Gregorian integer day transforms for years 1..9999; Unix conversion is explicitly UTC and requires the documented `Z` form.

## Engineering notation

Engineering notation is presentation of an existing value, not another numeric domain. Standard's binary64 formatter derives one rounded scientific representation and rearranges it so the exponent is a multiple of three. This avoids computing a `pow(10, exponent)` scale that could underflow for the smallest finite subnormals and allows rounding carry to cross an engineering exponent boundary correctly.

Scientific engineering presentation operates on the retained multiprecision value. Additional Results never invents precision: every row is another representation of the same domain value.

## Display formatting and structured history

Formatting is shared core/controller policy, not a platform-shell calculation. Scientific formats retained multiprecision components directly in General, Fixed, Scientific or Engineering forms without conversion through `double`.

Fixed output is bounded before streaming: if a component could exceed the 20,000-digit materialisation allowance, including requested fractional places and rounding headroom, it transitions to bounded General/exponent form at retained precision. Decimal-place requests are clamped to the supported maximum. This presentation transition leaves the numeric value unchanged.

History is structured semantic state. Numeric Scientific entries retain their result components, angle unit and calculation precision; recall uses those components so a previous-result reference, assignment or random expression cannot silently produce a different historical value. Programmer entries retain radix, width and signed-display context. Recall restores recorded context rather than reconstructing semantics from display text. History and memory remain process-local rather than part of the durable reusable-state document.

## Failure model

Malformed syntax, excessive recursion, division by zero, invalid mathematical domains, resource-budget exhaustion, factorial/exponent violations, unknown identifiers/functions, reserved-constant assignment, non-finite real results, invalid Programmer digits and unsafe shifts are explicit failures.

A plausible substitute result is not an acceptable fallback. If a numeric domain cannot justify a value under its contract, error/unsupported is preferable to invented precision.

## Validation evidence

Exact contracts—parser structure, integer bit patterns, radix rules, state transitions and exact rational results—normally compare exactly. Real/transcendental results use tolerances justified by operation and magnitude.

Scientific has independent MPFR/MPC oracle coverage, including principal-branch and reciprocal-family cases, so the production Boost implementation is not its own sole numerical oracle. Boundary tests cover binary64 round trips, subnormals/extremes, precision selection, parser/function recursion, memory overflow/range handling, Advanced Tools budgets and large exact/integer operations.

Native platform shells are expected to present the shared result; they are not separate numerical oracles. Simulator/device-target compilation is portability evidence, not proof of physical-device behaviour.

## References

The maintained basis includes IEEE Std 754-2019 / ISO/IEC 60559:2020 for floating-point concepts, ISO/IEC 14882:2017 (C++17), documented Boost.Multiprecision contracts, conventional mathematical definitions for operator precedence and elementary functions, and Calculator-owned regression/oracle tests for the specific interaction and branch conventions described above.

## Adding a numerical capability

Before acceptance, establish the representation and units, grammar/interaction semantics, valid/invalid domains, exactness/overflow policy, resource limits, display policy, authoritative definition/reference, cross-platform expectations, failure/recovery semantics and deterministic boundary tests. If those cannot be stated precisely, the capability is not complete.
