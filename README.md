<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Calculator

Calculator is a native cross-platform calculator for the software family, with first-class Linux, Windows and iPhone interfaces.

The project is deliberately larger in ambition than a four-function calculator, but the implementation grows in layers. The calculation engine is independent of the graphical interface, while a shared platform-neutral UI contract/controller defines calculator layout, commands and interaction state once for the desktop shells.

**Current source version:** 0.1.21  
**Language:** C++17 shared calculation core, GTK4 Linux shell, native Win32 Windows shell, SwiftUI iPhone shell with Objective-C++ bridge  
**Shared foundation:** Common 1.19.2  
**Design contract:** shared Design v1  
**Licence:** GPL-3.0-or-later

## Engineering ethos

Calculator is a first-principles engineering project. The question is not
simply how to reproduce an existing calculator, but what a calculator should be
when it is designed today with the strongest available mathematics, numerical
methods, human-interface research, platform capabilities and software
engineering practice.

"Modern" is not treated as a synonym for "better". New techniques are adopted
when they improve correctness, precision, performance, resilience, usability,
accessibility or maintainability; older techniques are retained when they
remain the better solution. Existing mature implementations are evidence to
study, not specifications to clone. Their hard-won lessons are separated from
their historical platform and compatibility constraints, then reconsidered
against current research and the needs of this project.

The implementation therefore aims for the best justified approach rather than
the newest fashionable one or the easiest conventional one. Architectural
decisions should be measurable, testable and explainable. Where a capability
can be shared cleanly across platforms, it is implemented once; platform code
is kept for genuinely platform-specific behaviour. Features are not considered
finished merely because they work in the common case: edge conditions,
numerical behaviour, accessibility, failure modes and regression protection are
part of the feature.

The same principle applies across the wider software family: start from the
problem, study what is known now, preserve proven ideas that still deserve to
survive, replace assumptions that no longer do, and prove the result in real
use.

## Appearance

Calculator supports **System**, **Day** and **Night** appearance modes on Linux, Windows and iPhone. System delegates light/dark choice to the host platform; Day and Night consume the canonical semantic palettes from Common 1.19.2. Platform code owns only theme detection, persistence and native rendering.

## Current capabilities

Calculator has three explicitly switchable modes presented as a visible mode strip on desktop and iPhone, so Standard, Scientific and Programmer are directly selectable rather than hidden behind a cycling control:

- Standard — traditional desktop-calculator immediate arithmetic, contextual percentages, powers, unary operations and memory;
- Scientific — expression/order-of-operations evaluation, trigonometric, inverse trigonometric, logarithmic, exponential and related functions with degree/radian control; and
- Programmer — binary, octal, decimal and hexadecimal integer arithmetic, bitwise operations, shifts, complement, 8/16/32/64-bit widths, and unsigned/signed display. Width controls are explicitly labelled W8, W16, W32 and W64 so they do not conflict with numeric keypad entry.

The calculator also provides reusable variables, bounded calculation history and a shared calculation-session layer above the parser. Variables can be assigned directly with expressions such as `x=42` and reused in subsequent calculations.

Desktop controls are state-aware: commands that cannot currently succeed are disabled consistently by the shared controller (for example MR/MC before memory is populated, invalid Programmer digits for the selected radix, and unary/equals operations without a usable operand). Features are considered complete only when implementation, tests and documented behaviour agree.

## Planned capability families

The architecture is intended to grow into:

- Engineering mode and specialised engineering calculators;
- exact integer and wider numeric domains where useful;
- mathematical and physical constants;
- unit conversion;
- ICT/network calculations;
- storage and filesystem calculations;
- date/time and epoch calculations;
- statistics, graphing and equation solving;
- complex and arbitrary-precision mathematics where justified; and
- Project-specific engineering calculators.

The application is intentionally being built as one calculator with selectable modes rather than as separate calculator applications.

The Linux and Windows desktop shells consume the same Calc-owned UI contract and controller: the same modes, button order, commands, desktop sizing metrics, responsive breakpoints and interaction state are defined once, then rendered through GTK4 or native Win32. Compact, regular and wide desktop states are shared; wide layouts dock calculation history beside the keypad. Platform code owns only toolkit mechanics such as widgets, HWND/GDI rendering, DPI and native window integration. iPhone keeps native SwiftUI touch sizing while sharing the calculation core and shared Design v1 visual language.

## Release platforms

Releases are multi-platform by default. The same versioned source is built and tested across all three targets:

- Linux x64: `infiltrator-calc_<version>_amd64.deb`;
- Windows x64: `infiltrator-calc_<version>_windows_x64.exe`, a native standalone Win32 executable built with the static MSVC runtime and no GTK/GLib runtime bundle;
- iPhone: a native SwiftUI application is compiled for both iOS Simulator and real iPhone ARM64 device architecture. CI publishes an iOS Simulator bundle and an unsigned device bundle for build verification.

A signed installable `.ipa` requires an Apple signing identity and provisioning profile. Those credentials are deliberately not stored in the repository. Once signing is configured, the same Xcode target is ready to archive and export as an `.ipa`.

A release is published only after Linux, Windows and iOS builds and the shared-core tests succeed.

### Compatibility identifiers

The user-facing product name is **Calculator**. The Debian package/executable/configuration identifier `infiltrator-calc` and the existing iOS bundle identifier `net.ssmith.infiltrator.calc` are retained as stable compatibility identities so upgrades, settings and installed application identity are not broken by the rename. They are not the product name.

## Shared design

Calculator uses Common as its reusable software foundation and follows the canonical shared Design v1 visual contract. The common design language is a graphite/silver foundation with near-black backgrounds, layered dark panels, restrained silver borders and MB Corpo typography roles.

The calculation core links against `InfiltratrCommon::Portable` from the exact released Common 1.19.2 gitlink. CMake verifies the checked-out Common `VERSION` is exactly `1.19.2`, so a stale or mismatched submodule fails configuration instead of silently building. Linux and Windows consume the Common theme C API directly; the iPhone target compiles the same Common Portable source set and obtains its Day/Night semantic palette through the Objective-C++ bridge instead of carrying a Swift colour mirror.

## Typography

Calculator prefers locally installed MB Corpo fonts when available and falls back automatically to normal system fonts when they are absent. The native Windows shell explicitly uses Segoe UI when the preferred MB Corpo faces are unavailable, matching the Windows fallback defined by the shared design contract.

The project does **not** redistribute proprietary MB Corpo font binaries. The current UI roles are:

- `MB Corpo S Title WEB` for normal interface text and controls;
- `MB Corpo A Title Cond WEB` for product/display titles.

This follows the established desktop applications in the software family.

## Build

Clone recursively so the exact Common revision is available:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Infiltrator-Calc.git
cd Infiltrator-Calc
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Debian-family systems the initial development dependencies are the standard C/C++ toolchain, CMake, GTK4 development packages and pkg-config. Windows builds use Visual Studio/MSVC and the Windows SDK only; GTK, GLib, MinGW and third-party runtime DLLs are not required for the Windows executable.

## Project structure

```text
src/
├── app/                 Native GTK/Linux and Win32/Windows platform shells
├── core/                Portable calculation engine, session state and Programmer engine
├── ui/                  Shared desktop UI contract, controller and theme adapter
└── infiltratr-common/   Exact shared Common gitlink

ios/
├── Sources/             Native SwiftUI iPhone application
├── Bridge/              Objective-C++ bridge into the shared C++ core
└── project.yml          Reproducible XcodeGen project definition

tests/                   Core, session, UI-contract and cross-platform regression tests
docs/                    Maintained architecture, design and implementation documentation
```

## Release direction

The project uses `main` as its working branch and follows the project release discipline: test the exact source commit, build from that commit, and publish immutable release identities.

The packaged form is a generic Debian package suitable for the Package Repository and future distribution integration.

## Licence

Copyright © 2026 Shannon Smith.

Calculator is licensed under the GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`).
