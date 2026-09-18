<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Calculator Roadmap

The roadmap records capability status, not release promises. A checked item is expected to have implementation and regression coverage; incomplete portions of a broader feature are listed separately rather than hidden inside a partially true checkbox.

## Foundation

- [x] Portable C++ calculation core.
- [x] Native GTK4 Linux shell.
- [x] Native Win32 Windows shell.
- [x] Native SwiftUI iPhone shell with Objective-C++ bridge.
- [x] Exact Infiltratr Common dependency.
- [x] Cross-platform release gating for Linux, Windows and iOS.
- [x] Shared desktop UI contract/controller.
- [x] System/Day/Night appearance contract.
- [x] MB Corpo preference with platform font fallback.

## Standard calculator

- [x] Immediate left-to-right binary arithmetic.
- [x] Contextual percentage semantics.
- [x] Sign, reciprocal, square and square-root operations.
- [x] Calculator memory with explicit empty/set state.
- [x] Bounded calculation history.
- [x] Reusable variables and direct assignment.
- [x] Cursor-aware desktop entry and command enablement.
- [ ] User-configurable display precision.
- [ ] Exact-decimal numeric domain where a justified use case requires it.

## Scientific

- [x] Order-of-operations expression evaluation.
- [x] Parentheses and right-associative powers.
- [x] Trigonometric and inverse-trigonometric functions.
- [x] Hyperbolic functions in the expression engine.
- [x] Logarithmic and exponential functions.
- [x] Square root, cube root and absolute value.
- [x] Factorial.
- [x] `pi` and `e` constants.
- [x] Degree/radian UI mode for trigonometric operations.
- [ ] Gradian mode.
- [ ] Combinations and permutations.
- [ ] Broader mathematical/physical constant catalogue.

## Programmer and ICT

- [x] Binary, octal, decimal and hexadecimal integer modes.
- [x] 8/16/32/64-bit width domains.
- [x] Signed/unsigned result presentation.
- [x] Bitwise complement, AND, OR and XOR.
- [x] Shifts with radix-independent decimal shift counts.
- [ ] Engineering notation and SI prefixes.
- [ ] Unit conversion.
- [ ] Storage and filesystem calculations.
- [ ] Network/subnet calculations.
- [ ] Data-rate and transfer-time calculations.
- [ ] Date/time and epoch calculations.

## Advanced mathematics

- [ ] Statistics.
- [ ] Graphing.
- [ ] Equation solving.
- [ ] Complex numbers.
- [ ] Arbitrary-precision real/integer arithmetic.
- [ ] Symbolic mathematics.

## Completion rule

A capability is complete only when its behaviour is implemented, its relevant edge cases are covered by automated tests, and maintained documentation describes the behaviour accurately. Platform-visible features must also satisfy the applicable cross-platform ownership/parity checks.
