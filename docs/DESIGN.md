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

Common is used for stable, product-neutral facilities. Calculator currently relies on it for deterministic decimal conversion and the canonical Design v1 theme contract.

Calculator-specific semantics stay local even when they could technically be generalized. Shared code is valuable only when the abstraction is clearer than the duplication it replaces.

## Platform-native presentation

Linux uses GTK4, Windows uses native Win32 and iPhone uses SwiftUI. Native shells are retained because windowing, DPI, accessibility, system appearance and input behaviour are platform concerns.

The desktop shells share a platform-neutral UI contract/controller where the interaction model is genuinely common. iPhone shares the calculation core but retains a touch-native view model rather than inheriting desktop layout assumptions.

## Visual design

System, Day and Night are the supported appearance modes. Common owns the Day/Night semantic palette; each operating system owns detection of the current System appearance and the mechanics of rendering it.

MB Corpo fonts are a preferred local presentation resource, not a runtime dependency. The application must remain fully usable with the platform fallback font stack, and proprietary font binaries are not redistributed.

## Evolution criteria

A design change should have an identifiable improvement in at least one of correctness, numerical behaviour, usability, accessibility, performance, resilience or maintainability. Changes that merely relocate complexity or increase abstraction without improving one of those properties are not architectural improvements.

Where behaviour is important enough to document as a contract, it should also be represented by automated tests where practical.
