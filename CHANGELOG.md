# Changelog

## 0.1.22

- Completed the internal product rename to Calculator while preserving only compatibility-sensitive installed identities.
- Renamed the CMake project, internal libraries/tests and C++ namespace away from the former product name.
- Renamed the iOS project, scheme, Swift app type, Objective-C++ bridge files/classes and Swift palette/style adapters to Calculator.
- Renamed native Windows class identities and aligned regression/release workflows with the new internal names.
- Removed stale former-name references from current Debian metadata and implementation documentation.
- Documented the deliberate retention of the `infiltrator-calc` package/executable/configuration identity and existing iOS bundle identifier for upgrade continuity.

## 0.1.21

- Rewrote architecture, design and implementation documentation around the current cross-platform system rather than the initial prototype.
- Documented dependency direction, calculation-domain boundaries, Common ownership, numerical trade-offs, error propagation and release verification.
- Brought the roadmap into factual alignment with implemented Standard, Scientific and Programmer capabilities.
- Added focused source-contract comments for parser semantics, deterministic decimal conversion, fixed-width Programmer arithmetic, session state and desktop UI ownership.
- Reworked the calculator-core test source into readable behavioural groups without changing test semantics.
- Marked the 0.1.0 release note explicitly as historical and corrected remaining README structure/naming drift.

## 0.1.20

- Deepened Common 1.19.2 integration without moving Calculator-specific semantics into the shared library.
- Replaced desktop palette mirroring with the Common theme structure and resolver directly.
- Made the iPhone target compile the Common Portable source set and removed its duplicated hard-coded Day/Night palette.
- Replaced locale-sensitive numeric token conversion with Common's deterministic decimal parser.
- Centralised Calculator's 15-significant-digit display formatting across GTK, Win32 and iPhone.
- Completed the iPhone user-facing rename to Calculator and updated stale Common/version documentation.
- Updated the cross-platform parity gate to require bridge-backed Common theme ownership on iPhone.

## 0.1.19

- Renamed the user-facing application to **Calculator**.
- Preserved the existing `infiltrator-calc` package, executable and configuration identifiers for upgrade compatibility.
- Updated Linux, Windows and iPhone display names without changing calculator behaviour.


## 0.1.18

- Advanced the exact shared dependency to Infiltratr Common 1.19.2.
- Rebuilt Linux, Windows and iPhone from the same immutable Common revision.
- Republished through the repaired central APT path so the release and updater state agree.


## 0.1.17

- Added persistent **System / Day / Night** appearance modes across Linux GTK, native Windows and iPhone.
- System mode follows the host platform's current appearance; Day and Night use explicit Infiltrator palettes.
- Moved semantic theme ownership to Infiltratr Common 1.19.1 instead of maintaining Calc-private colour truth.
- Native Windows now follows the Windows app-theme preference, reacts to setting changes and persists the explicit override.
- Linux follows GTK theme changes in System mode; iPhone follows SwiftUI ColorScheme and persists the selected override.
- Extended parity/runtime guards so desktop shells and iPhone cannot silently lose the shared theme contract.

## 0.1.16

- Added shared responsive desktop visual states: compact, regular and wide layouts now come from the same Calc UI contract on GTK and Win32.
- Added a wide desktop history presentation that docks history beside the calculator when enough horizontal space is available, while retaining the History popup at normal widths.
- Added shared state-aware command enablement so impossible actions are disabled consistently on Linux and Windows, including empty memory actions, invalid Programmer digits and unavailable unary/equals operations.
- Added Standard-mode immediate left-to-right arithmetic semantics with contextual percentage behaviour (for example, `2+3*4` -> `20` in Standard while Scientific remains order-of-operations, and `100+10%` -> `110`).
- Added explicit memory-availability state to the shared Session so MC/MR enablement reflects whether memory has actually been set.
- Added regression coverage for responsive breakpoints, Standard-vs-Scientific semantics, contextual percentages, Programmer radix enablement and memory availability.

## 0.1.15

- Added a canonical cross-platform calculator UI contract that owns desktop metrics, mode names, button order, labels, roles and commands for Standard, Scientific and Programmer.
- Added a platform-neutral UI controller that owns mode, expression, result/status, DEG/RAD, programmer base/width/signedness, memory, variables and history behaviour.
- Migrated both GTK and native Win32 shells to render and dispatch through the same shared controller instead of maintaining separate calculator state machines.
- Removed platform-local keypad definitions so Linux and Windows can no longer silently reorder or reinterpret calculator controls independently.
- Added shared UI contract/controller tests plus a source-level parity guard that fails if either desktop shell reintroduces local calculator layout or state ownership.
- Documented the ownership boundary: Common owns product-neutral design tokens, Calc owns calculator-specific UI/state, and platform shells own only native rendering mechanics.

## 0.1.14

- Brought the Linux GTK shell up to the denser Windows 0.1.13 interaction hierarchy while retaining native GTK implementation and the shared Infiltrator design language.
- Reduced the Linux default window from 440×690 to 380×620 and tightened header, mode strip, display and keypad spacing.
- Split Standard memory controls into a compact MC/MR/M+/M− strip above a six-row keypad, matching the improved Windows control hierarchy.
- Removed visible subtitle/footer chrome from the main Linux calculator surface and softened the expression field so the display reads as one coherent panel.
- Added explicit compact memory-strip styling and updated the Linux layout regression contract to prevent the older oversized layout returning.

## 0.1.13

- Refined the native Windows Standard layout using the open-source Microsoft Calculator interaction model as a reference while retaining Infiltrator styling and the existing shared calculation core.
- Split the Standard memory controls into a compact strip above a six-row keypad, bringing the information density and control hierarchy closer to a polished Windows calculator.
- Added native hover feedback to owner-drawn Windows controls and preserved rounded-button backgrounds cleanly.
- Reduced the Windows default client area to 360×610 with a 320×520 minimum while keeping Scientific and Programmer grids responsive.
- Expanded Windows runtime CI to verify the memory-strip geometry and exercise the actual 3 + 3 = 6 button path in addition to startup, mode switching, font fallback and dependency checks.

## 0.1.12

- Reworked the native Windows layout to a compact Windows-calculator density with a 380×650 default client area, smaller header/mode chrome, a tighter display and 6px keypad gaps.
- Fixed owner-drawn mode and programmer selection repainting so changing Standard, Scientific, Programmer, base and width controls clears the previous visual selection immediately.
- Added an explicit Segoe UI fallback when the MB Corpo design-contract faces are not installed, avoiding uncontrolled Windows font substitution.
- Fixed rounded owner-drawn button corner artefacts by painting the control background before the rounded surface.
- Removed the redundant Windows subtitle/footer chrome from the visible calculator surface.
- Strengthened Windows CI to launch the EXE, verify compact geometry, switch all three modes, confirm the correct visible keypad counts and validate the title font family.

## 0.1.11

- Fixed native Windows keypad creation by binding the main window handle during `WM_CREATE` before constructing child key controls.
- Added a Windows startup regression test that launches the built EXE and verifies the complete calculator button set is actually created.
- Retained the standalone native Win32/MSVC packaging introduced in 0.1.10 and the compact Linux layout repair.

## 0.1.10

- Replaced the Windows GTK/MSYS2 shell with a native Win32 interface over the existing shared C++ calculator core.
- Switched Windows CI to Visual Studio/MSVC and the static MSVC runtime so the published EXE does not require GCC runtime DLLs.
- Removed the GTK/GLib portable-runtime ZIP from Windows releases; the Windows x64 EXE is now the complete application.
- Added a release gate that inspects Windows imports and rejects GTK/GLib, GCC, libstdc++, winpthread or Visual C++ redistributable dependencies.
- Kept Linux on GTK4 and iPhone on SwiftUI while preserving Infiltratr Common 1.18.1 as the shared foundation.
- Restored compact Linux desktop sizing: 440×690 default window, 34px keypad minimum height, 5px row spacing, and no vertical keypad expansion.
- Added a Linux layout regression guard so the bottom row and equals key cannot silently be pushed off-screen again.

## 0.1.9

- Advanced the Common gitlink to the immutable Infiltratr Common 1.18.1 release commit.
- Updated the CMake dependency guard and documentation to require Common 1.18.1.
- Rebuilt Linux, Windows and iPhone release artifacts against the same Common 1.18.1 foundation.


## 0.1.8

- Pinned the Common gitlink to the immutable Infiltratr Common 1.18.0 release commit.
- Added a CMake guard that rejects stale or mismatched Common submodules.
- Rebuilt all published platform artifacts against the same Common 1.18.0 foundation.


## 0.1.7

- Added a native SwiftUI iPhone application using the same Standard, Scientific and Programmer model as the desktop shells.
- Added an Objective-C++ bridge into the shared C++ session and programmer engines instead of reimplementing calculator logic in Swift.
- Added iPhone-native history, memory, variable assignment, DEG/RAD and programmer base/width/signedness controls.
- Added explicit iOS Simulator and real-device ARM64 build validation.
- Made the GTK desktop shell optional in CMake so the portable core can build and test independently on Apple runners.
- Release publication now requires Linux, Windows and iOS builds to succeed before GitHub assets are published.
- iOS CI produces simulator and unsigned device bundles; a signed installable IPA remains gated on Apple signing credentials.

## 0.1.6

- Reworked the calculator shell around the canonical Infiltrator Design v1 graphite/silver tokens.
- Increased control size and spacing and separated number, operator, utility, clear and primary-action treatments.
- Replaced the old cycling MODE button with direct Standard, Scientific and Programmer mode selection.
- Added visible selected states for Programmer base, integer width and signedness controls.
- Added a Windows x64 build/test path alongside the Linux Debian build.
- Release publication now requires both Linux and Windows to succeed and publishes Debian, Windows executable and portable Windows runtime assets together.

## 0.1.5

- Fixed Programmer shift counts so they are interpreted as decimal quantities regardless of the selected number base.
- Kept 8/16/32/64-bit width controls distinct from numeric keypad input.

## 0.1.4

- Added Programmer mode with binary, octal, decimal and hexadecimal bases.
- Added 8/16/32/64-bit widths, signed/unsigned display, bitwise operations and shifts.
- Added Programmer-mode regression tests.

## 0.1.3

- Added reusable calculation variables with `name=expression` assignment.
- Added a shared calculation-session layer for variables, memory and bounded history.
- Added a desktop calculation-history window with clear-history support.
- Added regression coverage for variable evaluation, session memory and history retention.
- Kept the existing Standard and Scientific calculator behaviour intact.
- Finalised the release regression test include and release verification path.
