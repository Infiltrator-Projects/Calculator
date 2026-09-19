# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for Calculator. Detailed commit-by-commit history remains in Git.

## Unreleased

- Harden Common 1.19.3 integration by checking the exact immutable dependency commit in repository builds.
- Run Linux, Windows and iPhone verification on every main-branch change instead of only release commits.
- Add a Clang ASan/UBSan Calculator lane and require it before release publication.
- Keep the iPhone Common source list aligned with Common's full Portable source set.
- Correct the Debian install manifest after the package-identity rename and verify desktop/icon payloads.
- Keep Common as a build/link dependency only so Calculator packages do not leak Common libraries, headers or CMake metadata.
- Remove obsolete repository-rename automation.

- Documentation governance aligned with the other maintained applications.
- Numerical semantics, portability and evidence boundaries are now documented explicitly.
- Historical release detail remains in immutable tags and GitHub Releases instead of being duplicated in the current tree.

## Recording policy

Record additions, removals, behavioural fixes, compatibility changes, dependency changes that affect consumers, and material validation/release changes. Pure refactoring needs an entry only when it changes maintenance, numerical, portability or compatibility expectations.

## Historical releases

Existing Git tags and GitHub Releases remain the authoritative identity for exact historical source and release assets. Do not reconstruct detailed historical claims here without evidence from those immutable records.
