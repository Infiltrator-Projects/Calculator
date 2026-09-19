<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Calculator Design Rationale

Calculator is designed as a set of explicit calculation domains behind native platform interfaces. The objective is not maximal abstraction; it is to place each decision at the narrowest layer that can own it correctly.

## Separation of semantics and presentation

The GUI is an adapter. Calculation rules, validation and domain behaviour belong below it. UI controls select commands and present state; they do not become an alternate implementation of arithmetic.

This allows the core to be tested independently and prevents Linux, Windows and iPhone from silently acquiring different mathematical behaviour.

## Why Standard and Scientific are distinct

A desktop Standard calculator and a mathematical expression evaluator answer different interaction expectations.

Standard mode applies binary operations in entry order and uses contextual percentage semantics. Scientific mode evaluates a grammar with mathematical precedence, right-associative exponentiation, functions, parentheses and variables. Preserving these as separate evaluators makes the distinction explicit and testable instead of hiding mode-dependent exceptions inside one parser.

## Why Programmer is a separate numeric domain

Programmer calculations operate on fixed-width bit patterns. Wrap-around arithmetic, complement, shifts and signed presentation are meaningful only when an integer width is part of the value model.

Those semantics would be incorrect if implemented through the real-number expression engine or through Common's checked integer arithmetic. Programmer mode therefore owns its width masking and radix rules locally.

## Current real-number representation

The current Standard/Scientific domain uses IEEE-754 binary64. This is a deliberate current constraint, not an assertion that binary64 is universally sufficient.

A new numeric representation should be introduced when a feature has a correctness requirement that binary64 cannot satisfy cleanly—for example exact decimal finance, arbitrary precision, complex values or symbolic manipulation. The trigger is a domain requirement with tests, not a desire to make the type system more elaborate.

## Shared foundation boundary

Common is used for stable, product-neutral facilities. Calculator currently relies on Common 1.19.7 for exact decimal-token conversion, the canonical Design v1 palette/metrics/typography contract and immutable MB Corpo asset provenance.

Calculator-specific semantics stay local even when they could technically be generalized. Real unary/scientific transforms are shared inside Calculator because they are calculator-domain semantics; the controller and expression parser consume one core implementation rather than moving those rules into Common. Shared code is valuable only when the abstraction is clearer than the duplication it replaces.

## Platform-native presentation

Linux uses GTK4, Windows uses native Win32 and iPhone uses SwiftUI. Native shells are retained because windowing, DPI, accessibility, system appearance and input behaviour are platform concerns.

All three shells share the platform-neutral Calculator controller where command/state behaviour is genuinely common. Linux and Windows also share desktop layout metrics and button definitions; iPhone retains touch-native SwiftUI composition rather than inheriting desktop geometry.

## Visual design

System, Day and Night are the supported appearance modes. Common owns the Day/Night semantic palette; each operating system owns detection of the current System appearance and the mechanics of rendering it.

Calculator-owned text uses exactly three canonical MB Corpo faces: S Regular, S Bold and A Condensed Regular. Release builds package or embed the exact hash-verified resources on Linux, Windows and iPhone. A missing or mismatched required face is an explicit build/startup failure rather than permission to introduce a fourth fallback family.

## Evolution criteria

A design change should have an identifiable improvement in at least one of correctness, numerical behaviour, usability, accessibility, performance, resilience or maintainability. Changes that merely relocate complexity or increase abstraction without improving one of those properties are not architectural improvements.

Where behaviour is important enough to document as a contract, it should also be represented by automated tests where practical.
