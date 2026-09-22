<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Calculator Design Rationale

Calculator is designed as a set of explicit calculation domains behind native platform interfaces. The objective is not maximal abstraction; it is to place each decision at the narrowest layer that can own it correctly.

## Separation of semantics and presentation

The GUI is an adapter. Calculation rules, validation and domain behaviour belong below it. UI controls select commands and present state; they do not become an alternate implementation of arithmetic.

This allows the core to be tested independently and prevents Linux, Windows and iPhone from silently acquiring different mathematical behaviour.

## Why Standard and Scientific are distinct

A desktop Standard calculator and a mathematical expression evaluator answer different interaction expectations.

Standard and Scientific modes give shared arithmetic operators the same conventional mathematical meaning: exponentiation, multiplication/division and addition/subtraction follow normal precedence, and parentheses are explicit grouping. Standard deliberately exposes a smaller numeric-only capability surface over binary64; Scientific adds constants, functions, variables and arbitrary-precision real/complex evaluation. Mode selection changes capability and representation, not the meaning of a written arithmetic expression.

## Why Programmer is a separate numeric domain

Programmer calculations operate on fixed-width bit patterns. Wrap-around arithmetic, complement, shifts and signed presentation are meaningful only when an integer width is part of the value model.

Those semantics would be incorrect if implemented through the real-number expression engine or through Common's checked integer arithmetic. Programmer mode therefore owns its width masking and radix rules locally.

## Current real-number representation

Standard mode deliberately uses IEEE-754 binary64 because it is the compact everyday arithmetic domain; Scientific uses arbitrary precision where its broader mathematical capability justifies it. Within Standard, the binary64 value is authoritative: display formatting may round for readability but is never allowed to feed back into later computation.

Scientific mode is a distinct arbitrary-precision real/complex domain backed by Boost.Multiprecision. The interactive controller uses 50 decimal digits; the core accepts requested precisions from 16 through a maintained ceiling of 1000 digits and is independently oracle-tested across that range. Decimal literals and intermediate Scientific arithmetic remain in that domain; they are not silently routed through binary64. The public Session/Controller boundary carries real and imaginary components as decimal text so platform shells do not acquire a third-party multiprecision ABI. Principal complex branches and exact quadrant/power special cases are Calculator-owned mathematical contracts rather than accidental backend behaviour.

Exact decimal/rational tools, arbitrary-precision integer tools and Programmer fixed-width integers remain separate numeric domains because representation is part of their correctness contract. A further representation should be introduced only when a feature has a domain requirement that the existing representations cannot satisfy cleanly and that requirement can be stated and tested.

## Shared foundation boundary

Common is used for stable, product-neutral facilities. Calculator currently relies on immutable Common 1.19.23 for locale-independent numeric parsing, deterministic ASCII classification, checked arithmetic, durable POSIX persistence helpers, the canonical Design v1 palette/metrics/typography contract and immutable MB Corpo asset provenance.

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


## Additional Results

Primary calculation stays visually dominant. Secondary representations belong in an on-demand Additional Results surface rather than being permanently inserted into the keypad or display card. This follows the project's compact-window rule and gives future unit, symbolic or other secondary representations a stable home.

The platform shell may choose native presentation mechanics, but labels and values come from the shared Controller. Programmer's existing Bases surface is the domain-specific form of the same representation model.


## Advanced Tools workbench

The fourteen extended calculation families live behind one on-demand Tools surface instead of adding permanent keypad rows. This preserves the compact primary calculator while keeping engineering, conversion, network, storage, date/time, constants, statistics, graph/equation, exact, arbitrary-precision, complex, financial and number-utility capabilities directly accessible.

The workbench is not a terminal. Each native shell provides a tool selector, an explicit prompt, an editable example/input control, a Run action, selectable output and graph rendering where applicable. The small command grammar belongs to each tool domain and is documented in TOOLS.md. Evaluation remains in shared C++; platform code owns only controls and graph drawing.

Graphing follows the same rule as Additional Results: one shared mathematical result, native presentation. The shared core samples the expression and marks discontinuities; GTK/Cairo, Win32/GDI and SwiftUI/Canvas render that point sequence without re-evaluating the expression.
