<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to Calculator

Calculator combines a portable C++ calculation core, a shared desktop interaction contract, native GTK4 and Win32 shells, and a native SwiftUI iPhone frontend. Contributions should preserve those boundaries and keep numerical behaviour explicit and testable.

## Engineering rules

- Keep arithmetic, expression, session and Programmer semantics below platform presentation code.
- Keep GTK, Win32 and SwiftUI/Objective-C++ mechanics out of the portable calculation domain.
- Preserve the distinction between Standard immediate semantics, Scientific expression semantics and Programmer fixed-width integer semantics.
- Treat numerical edge behaviour as part of the feature contract: domains, finite-result requirements, width/radix rules, overflow policy and percentage semantics require deterministic tests.
- Reuse the pinned Infiltratr Common API when its contract is the correct generic abstraction; improve Common first if Calculator has the stronger generic implementation.
- Do not move Calculator-specific grammar, calculator interaction policy or mode semantics into Common merely to reduce local code.
- Preserve compatibility identifiers when changing them would break installed upgrades, settings or application identity.
- Add the narrowest useful regression for reproducible behavioural, numerical, platform or packaging defects.
- Do not create parallel Markdown for a subject already owned by the canonical documentation set.

## Language and dependency policy

The shared domain is C++17. Linux uses GTK4, Windows uses native Win32/MSVC, and iPhone uses SwiftUI with an Objective-C++ bridge because those are the current native platform boundaries.

A new language, runtime or dependency requires a concrete technical advantage that the existing architecture cannot reasonably provide. Convenience or novelty alone is not sufficient.

Common is pinned exactly. A shared helper is adopted only when its semantics are at least as strong as the Calculator-owned implementation it replaces.

## Build and validation

Clone recursively because Calculator pins Common:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Calculator.git
cd Calculator
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Platform-visible changes also require the relevant native build/integration evidence. A Simulator build is Simulator evidence; an unsigned iPhoneOS build is device-target compilation evidence; neither is proof of a signed physical-device installation.

## Documentation and comments

Read `docs/README.md` for document authority. Architecture belongs in `docs/ARCHITECTURE.md`; rationale in `docs/DESIGN.md`; durable choices in `docs/DECISIONS.md`; direction in `docs/ROADMAP.md`; validation evidence in `docs/VALIDATION.md`; numerical contracts and evidence in `docs/NUMERICS.md`; cross-platform representation and compatibility boundaries in `docs/PORTABILITY.md`; and shared desktop/iPhone ownership rules in `docs/UI_PARITY.md`.

Comments should preserve information expensive to reconstruct: units, precedence, representation assumptions, numerical domains, ownership, compatibility constraints and deliberate deviations. Do not narrate straightforward syntax.

## Repository discipline

`main` is the authoritative development and release branch. Keep commits focused. Published tags/releases are immutable identities, and release assets must derive from the exact qualified source revision.

## Licence

Contributions are accepted under GPL-3.0-or-later unless explicitly agreed otherwise beforehand.
