# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for Calculator. Detailed commit-by-commit history remains in Git.

## Unreleased

No unreleased changes.

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
