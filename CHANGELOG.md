# Changelog

This file records the current Calculator release line. The complete pre-0.2.50 release ledger is preserved verbatim in [CHANGELOG_HISTORY.md](CHANGELOG_HISTORY.md); immutable Git tags and GitHub Releases remain the authority for exact historical source and assets.

## 0.2.50 - 2026-10-17

- Use `/System/Resources/Fonts` as Calculator's persistent Linux font cache instead of packaging Calculator-owned MB Corpo font binaries.
- Pin exact Libraries 1.19.51 and install its verified native font bootstrap from the pinned Common source.
- Keep Windows private font embedding unchanged while Linux installers verify or repair shared native font resources, and never remove those resources during Calculator uninstall.
- Make Debian qualification reject duplicate MB Corpo TTF payloads and require the native font bootstrap lifecycle.

## 0.2.49 - 2026-10-17

- Publish the product changes made after the immutable 0.2.48 release instead of mutating that release identity.
- Keep `/Programs/calculator` as Calculator's sole Linux executable location with no `/usr/bin` compatibility entry.
- Keep CMake/source installation, Debian packaging, desktop launch, package qualification and the aggressive hardware-native installer on the same native filesystem contract.

## Recording policy

Record current user-visible, compatibility, architecture and validation changes here. Older entries move to the historical ledger rather than making current release metadata progressively harder to maintain.
