# Roadmap

This is a direction document, not a dated promise. The released source and tests define what is actually supported.

## Current foundation

- maintain Standard, Scientific and Programmer modes over one shared calculation/session architecture
- preserve native GTK4, Win32 and SwiftUI interfaces without duplicating calculation semantics
- keep numerical edge behaviour, cross-platform parity and exact Common integration protected by regression tests

## Near-term priorities

- strengthen numerical reference evidence and cross-platform boundary testing as mathematical features evolve
- improve accessibility and native interaction quality without weakening shared calculator semantics
- continue consolidating genuinely generic mechanisms into Common only when its contract remains at least as strong as Calculator's local implementation

## Longer-term direction

- add engineering, unit, ICT/network, storage/filesystem and date/time calculators when their domains and validation rules are explicit
- add broader constants, statistics, graphing and equation solving when they can be represented and tested without weakening numerical clarity
- introduce exact-decimal, arbitrary-precision or complex domains when a supported capability has a correctness requirement that binary64 cannot satisfy cleanly

## Admission rule

A proposed capability enters the roadmap only when its numeric/domain representation, ownership, user interaction and credible validation strategy are clear. Features are not admitted merely because another calculator exposes them.

## Completion rule

An item is complete when implementation, relevant edge-case tests, user-visible behaviour and maintained documentation agree. A checkbox or release number cannot substitute for missing evidence.
