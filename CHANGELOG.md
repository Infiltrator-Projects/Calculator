# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for Calculator. Detailed commit-by-commit history remains in Git.

## Unreleased

No unreleased changes.

## 0.1.40 — 2026-09-20

- Add one shared Programmer representation contract that renders the current fixed-width bit pattern simultaneously as BIN, OCT, DEC and HEX.
- Preserve signed interpretation for the decimal representation while keeping binary/octal/hexadecimal as exact masked bit-pattern views.
- Add an on-demand Programmer Representations surface on Linux, Windows and iPhone without increasing Standard or Programmer main-window geometry.
- Group binary output by nibbles for readability while retaining the exact full-width representation underneath.
- Make Linux and iPhone result text directly selectable for copy workflows and expose selectable representations on iPhone.
- Add regression coverage for representation semantics and cross-platform presentation wiring.

## 0.1.39 — 2026-09-20

- Replace presentation-only history with structured history entries carrying mode, exact rendered result and Programmer radix/width/signed context.
- Record Programmer calculations and errors in the same bounded history model without converting 64-bit results through binary64.
- Add history recall through the shared Controller so replay restores the originating calculator mode and exact Programmer context.
- Make Linux history rows directly recallable, add native Win32 list selection/double-click recall, and expose structured recall in the iPhone history sheet.
- Keep wide desktop history docking as a read-only summary while the dedicated history surface owns explicit recall interaction.
- Add regression coverage for history ordering, bounds, exact Programmer context and cross-platform replay wiring.

## 0.1.38 — 2026-09-20

- Expand Scientific mode without increasing its keypad row count: add 2nd and HYP layers, DEG/RAD/GRAD cycling and F-E scientific-notation display switching while preserving the existing direct constants/operators.
- Add inverse and hyperbolic trig command routing through the shared C++ core, plus square/cube, floor/ceiling and base-2/base-10 exponential transforms exposed through the 2nd layer.
- Extend the expression engine with asinh/acosh/atanh, square/cube, exp2/exp10, floor and ceil functions and add gradian-aware trigonometric conversion.
- Extend Programmer mode with fixed-width ROL/ROR and NAND/NOR operators while preserving wrapping, radix and width semantics.
- Add an explicit MS memory-store command alongside MC/MR/M+/M− on every platform.
- Keep Standard at its compact 480-unit geometry; extended controls remain shared through the controller/UI contract rather than platform-specific implementations.
- Release the previously unpublished copyright-normalisation commits together with the functional changes so main and the immutable release are aligned.

## 0.1.37 — 2026-09-19

- Advance the exact shared foundation from Common 1.19.7 to immutable Common 1.19.8.
- Retain the existing public Common API boundary because 1.19.8 is an ABI-compatible internal-consolidation release rather than an API expansion.
- Re-run the Calculator/Common ownership audit and confirm no private Common implementation headers or Calculator-specific semantics should be pulled across the boundary.
- Strengthen the Common integration regression so Calculator is pinned to the exact 1.19.8 release commit and consumes only published Common interfaces.
- Preserve all Standard, Scientific, Programmer, history, typography, platform and packaging behaviour while rebuilding every release target against Common 1.19.8.

## 0.1.36 — 2026-09-19

- Advance Calculator to immutable Common 1.19.7 and replace its private decimal-token scanner with Common's exact locale-independent token parser.
- Consume Common's native typography identity, structural design metrics and MB Corpo asset provenance instead of maintaining desktop copies.
- Centralise real unary/scientific transforms in the Calculator core so parser and controller no longer implement the same mathematics separately.
- Centralise bounded history text rendering in Session/Controller and remove platform-shell reach-through into mutable session state.
- Preserve the deliberate C/C++ split: Common remains C11 portable infrastructure, Calculator domain/state remains C++17, and native shells retain only platform adaptation.

## 0.1.35 — 2026-09-19

- Corrected the 0.1.34 Standard-mode sizing regression: Standard is compact again instead of stretching its keypad to fill an oversized window.
- Moved the 480-unit Standard preferred/minimum height into the shared desktop contract, with Scientific and Programmer retaining the 610-unit preferred / 520-unit minimum extended geometry.
- Made GTK and Win32 consume the same mode-aware height contract and added regression coverage preventing a return to platform-local or stretched Standard geometry.

## 0.1.34 — 2026-09-19

- Removed the dead vertical strip below the Linux Standard keypad by allowing the Standard panel, grid and keypad rows to consume the window's shared 520px allocation.
- Kept the shared desktop geometry contract intact rather than reintroducing a Linux-only undersized window exception.
- Added a regression guard for the Standard vertical-fill contract.

## 0.1.33 — 2026-09-19

- Replaced the Linux Calculator artwork with the shared non-automotive Infiltrator icon language: dark graphite field, #72dcff cyan linework and a simplified calculator glyph.
- Kept the same project-owned SVG wired through the desktop launcher and Linux Mint Software Manager package alias.

## 0.1.32 — 2026-09-19

- Bound Programmer-mode recursive parsing so excessively nested parentheses or unary operators fail deterministically with `expression nesting too deep` instead of risking native stack exhaustion.
- Added permanent Programmer regression coverage for deep parenthesis and unary chains.

## Recording policy

Record additions, removals, behavioural fixes, compatibility changes, dependency changes that affect consumers, and material validation/release changes. Pure refactoring needs an entry only when it changes maintenance, numerical, portability or compatibility expectations.

## Historical releases

Existing Git tags and GitHub Releases remain the authoritative identity for exact historical source and release assets. Do not reconstruct detailed historical claims here without evidence from those immutable records.
