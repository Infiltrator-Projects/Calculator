<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Portability

Calculator is portable at its calculation and interaction contracts rather than by pretending GTK, Win32 and SwiftUI are interchangeable. Shared semantics remain below explicit platform seams; each frontend uses the strongest native mechanism available without becoming a second calculator implementation.

## Language and interface policy

The shared calculation/session/controller code targets C++17. Linux uses GTK4, Windows uses native Win32/MSVC, and iPhone uses SwiftUI with an Objective-C++ bridge.

Introducing another language/runtime requires a concrete technical advantage that the current C++/native-platform architecture cannot reasonably provide. Portability does not mean selecting the lowest-common-denominator toolkit.

Portable calculation interfaces must not expose GTK widgets, HWNDs, Swift objects, platform registry/settings handles or other frontend-owned state.

## Platform boundary

The shared core owns arithmetic, expression grammar, session state and Programmer semantics. The shared controller owns calculator command/state behaviour on Linux, Windows and iPhone; the desktop contract additionally owns desktop button order and logical geometry.

Linux owns GTK widgets, CSS, Linux theme/font integration and window mechanics. Windows owns HWND/GDI, per-monitor DPI mapping, Registry-backed preferences and Windows message handling. iPhone owns SwiftUI composition, platform appearance observation and touch-native presentation.

A platform may adapt shared state for native presentation; it must not reinterpret the mathematics.

## Real-number representation

Standard and Scientific currently use C++ `double`, relying on the supported toolchains/platforms providing the conventional IEEE-754 binary64 representation.

Code must not assume that formatted decimal text is the exact internal value. Numerical semantics and tolerance expectations are defined in [NUMERICS.md](NUMERICS.md).

If support is added for a platform with materially different floating-point semantics, that is a compatibility decision requiring explicit validation rather than an automatic consequence of successful compilation.

## Fixed-width integer representation

Programmer mode uses explicit-width integer semantics and masks results to the selected 8/16/32/64-bit domain.

External or cross-language boundaries should use explicit-width types where bit width is part of the contract. Native signed overflow must not be relied upon to implement Programmer wrapping; wrapping remains an explicit bit-pattern policy.

## Locale and text boundaries

Numeric expression syntax is locale-independent. Decimal token conversion uses Common's deterministic parser rather than the host process locale.

User-interface text and platform font rendering may vary by locale/platform, but locale must not silently change the meaning of Calculator's decimal grammar, operators or Programmer radices.

Strings crossing Objective-C++/Swift and native desktop boundaries must use the platform APIs' documented encoding contracts. A presentation encoding issue must not become a second numeric grammar.

## iPhone bridge

`CalculatorBridge` is the language boundary between Swift and the C++ controller. It converts Swift expression/mode/key operations into controller operations, snapshots controller state for presentation, and exposes Common semantic palette values to Swift.

SwiftUI owns touch layout and System appearance resolution. The bridge is deliberately thin: it must not implement arithmetic, memory, history, Programmer or mode semantics independently.

An iOS Simulator build proves Simulator-target compatibility. An unsigned iPhoneOS build proves device-architecture compilation. Neither substitutes for signed physical-device installation evidence.

## Desktop geometry and DPI

The desktop UI contract expresses logical Calculator layout metrics and responsive classes. GTK maps them through its layout system; Win32 applies platform DPI scaling when mapping them to physical coordinates.

Portability requires equivalent interaction/state ownership, not pixel-identical rasterisation. Native font metrics, compositor behaviour and accessibility scaling may differ and require platform observation.

## Theme and typography

Common owns Day/Night semantic design values. Each platform owns System-theme detection and native rendering.

Calculator-owned text is restricted to three canonical MB Corpo faces: S Regular, S Bold and A Condensed Regular. Release builds bundle or embed those exact hash-verified font resources on every platform. Missing or mismatched Calculator fonts are a build/startup failure; silently substituting a fourth font family is outside the product contract.

## Compatibility and installed identity

The visible product/repository/release-asset name is Calculator. Some established runtime/configuration identities may intentionally differ where changing them would break settings, launch paths, bundle identity or installed-package migration.

Compatibility naming is an explicit migration concern, not an excuse for stale user-facing branding. A new platform or installer must distinguish display name from stable technical identity.

## Dependency boundary

Common is pinned exactly and supplies reusable product-neutral primitives whose contract matches Calculator's needs. Calculator retains expression grammar, immediate-calculator behaviour, Programmer policy and calculator-specific interaction.

If Calculator has a stronger implementation of a genuinely generic mechanism, improve Common until the shared contract preserves those advantages; do not weaken the Calculator domain simply to increase reuse.

## Review test

A portable change should preserve these properties:

- arithmetic/session semantics remain platform-neutral;
- platform objects do not leak into the portable core;
- real-number representation assumptions remain explicit;
- fixed-width Programmer behaviour does not rely on undefined signed overflow;
- numeric grammar remains locale-independent;
- Objective-C++/Swift adaptation routes commands through the shared controller rather than duplicating calculation rules;
- native DPI/theme/font behaviour stays a presentation concern, with Windows using per-monitor DPI awareness;
- compatibility identifiers change only through a deliberate migration; and
- another native frontend could consume the domain contracts without copying an existing platform shell.
