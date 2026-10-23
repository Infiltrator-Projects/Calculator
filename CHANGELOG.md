# Changelog

## 0.2.57 - 2026-10-23

- Evaluate Financial formulae through the shared Scientific engine before checked binary64 conversion, preserving tiny-rate annuities and finite results across large intermediate values.
- Replace per-period DDB iteration and unsafe integer conversion with bounded closed-form evaluation that shares the caller's work and cancellation budget.
- Preserve committed results on unchanged namespace/state imports; refresh Scientific display, cache and memory authority together when bindings change, and show invalidated expressions explicitly.
- Preserve exact binary64 values when widening formula operands, graph coordinates and memory into Scientific arithmetic.
- Add 70 independent Financial reference cases, range/domain/budget regressions and Controller import tests across the existing platform qualification gates.

## 0.2.56 - 2026-10-23

- Retain one committed Scientific result for display, Additional Results, memory, unary actions and history; refresh angle changes consistently.
- Preserve saved real/complex variables through 1000 digits and share GUI/CLI durable state with locked merge and explicit conflict failure. Reserve every Scientific built-in name.
- Reject scaled non-roots, discontinuity candidates, numerical overflow and non-zero underflow at real-tool boundaries; keep signed Programmer output in the selected radix.
- Bound huge Fixed Scientific displays with exponent-form fallback without changing retained values.
- Remove native installer profile/test bypasses; require CPU-native LTO, tests and trained two-pass PGO, with isolated GTK training that exits cleanly.
- Derive iPhone keys and dynamic labels from the shared C++ contract and align current dependency, typography and numerical documentation.

## 0.2.55 - 2026-10-18

- Fix hardware-native installation when APT classifies a locally rebuilt package as a downgrade despite matching the installed version.
- Explicitly reinstall equal versions after checking with dpkg; refuse genuinely older source versions before building and check again before installation.
- Test fresh installs, upgrades, same-version rebuilds and version ordering with epochs and local revisions.

## 0.2.54 - 2026-10-18

- Improve GTK compact Calculator legibility after reviewing running Linux screenshots: larger MB Corpo Text on controls and clearer mode, memory and status labels.
- Use existing Common summary/text semantic roles for readable Night/Day contrast rather than private colours; make inactive keys distinguishable without becoming invisible.
- Slightly enlarge A Screen2 numerical results and A Title Cond branding while preserving layout geometry, arithmetic and verified 23-font packaging.

## 0.2.53 - 2026-10-18

- Align Calculator's Windows and iPhone font resources with five verified 23-face MB Corpo roles: S Text Regular/Bold, S Text Cond Bold, A Title Cond and A Screen2 Cond.
- Verify the exact pinned InfiltratorOS inventory and each selected TTF before embedding in the Windows EXE or iPhone bundle; reject missing registered font families rather than substituting generic fonts.
- Apply Screen2 to Windows/iPhone results, reserve A Title Cond for product identity and Cond Bold for short controls; preserve calculation, native geometry and Common palette semantics.

## 0.2.52 - 2026-10-18

- Pin Libraries/Common 1.19.57 with its verified extended 23-face native font installer so a standalone Linux Calculator has the same MB Corpo Text, Condensed and Screen2 choices as an InfiltratorOS installation.
- Use the extended font helper without invoking the retired three-face bootstrap during Debian package configuration and retain fonts as persistent shared machine resources independent of Calculator uninstall.
- Keep existing Windows/iPhone privately embedded three-face asset sets and numerical behaviour unchanged.

## 0.2.51 - 2026-10-18

- Optimise Linux Calculator font hierarchy using the InfiltratorOS 23-face shared cache: MB Corpo S Text for reading and keypad, S Text Cond for compact toolbar/mode/status labels, A Title Cond for identity and A Screen2 Cond for the large numerical result.
- Keep the original pinned Common three-face font bundle as a safe fallback for standalone installations without the newer machine cache; Windows/iPhone native font registration remains unchanged in this release.
- Preserve controller, precision, layout/geometry, existing colour/interaction contract and numerical semantics.

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
