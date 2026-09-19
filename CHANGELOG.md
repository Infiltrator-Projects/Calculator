# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for Calculator. Detailed commit-by-commit history remains in Git.

## Unreleased

No unreleased changes.

## 0.1.32 — 2026-09-19

- Bound Programmer-mode recursive parsing so excessively nested parentheses or unary operators fail deterministically with `expression nesting too deep` instead of risking native stack exhaustion.
- Added permanent Programmer regression coverage for deep parenthesis and unary chains.

## Recording policy

Record additions, removals, behavioural fixes, compatibility changes, dependency changes that affect consumers, and material validation/release changes. Pure refactoring needs an entry only when it changes maintenance, numerical, portability or compatibility expectations.

## Historical releases

Existing Git tags and GitHub Releases remain the authoritative identity for exact historical source and release assets. Do not reconstruct detailed historical claims here without evidence from those immutable records.
