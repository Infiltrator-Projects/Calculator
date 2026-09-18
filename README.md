<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Infiltrator Calc

Infiltrator Calc is a native cross-platform calculator for the Infiltrator software family, with first-class Linux, Windows and iPhone interfaces.

The project is deliberately larger in ambition than a four-function calculator, but the implementation grows in layers. The calculation engine is independent of the graphical interface so it can become a reusable foundation rather than a collection of button callbacks.

**Current source version:** 0.1.12  
**Language:** C++17 shared calculation core, GTK4 Linux shell, native Win32 Windows shell, SwiftUI iPhone shell with Objective-C++ bridge  
**Shared foundation:** Infiltratr Common 1.18.1  
**Design contract:** Infiltrator Design v1  
**Licence:** GPL-3.0-or-later

## Current capabilities

Infiltrator Calc has three explicitly switchable modes presented as a visible mode strip on desktop and iPhone, so Standard, Scientific and Programmer are directly selectable rather than hidden behind a cycling control:

- Standard — arithmetic, percentages, powers, unary operations and memory;
- Scientific — trigonometric, inverse trigonometric, logarithmic, exponential and related functions with degree/radian control; and
- Programmer — binary, octal, decimal and hexadecimal integer arithmetic, bitwise operations, shifts, complement, 8/16/32/64-bit widths, and unsigned/signed display. Width controls are explicitly labelled W8, W16, W32 and W64 so they do not conflict with numeric keypad entry.

The calculator also provides reusable variables, bounded calculation history and a shared calculation-session layer above the parser. Variables can be assigned directly with expressions such as `x=42` and reused in subsequent calculations.

Features are considered complete only when implementation, tests and documented behaviour agree.

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
- Infiltrator-specific engineering calculators.

The application is intentionally being built as one calculator with selectable modes rather than as separate calculator applications.

The Linux, Windows and iPhone shells share the Infiltrator Design v1 visual language without forcing identical physical sizing. Linux uses a compact desktop density so every calculator row, including `=`, remains visible on normal 768–800 px work areas; Windows uses native Win32 controls over the shared C++ core; and iPhone keeps larger native touch targets. All three retain layered graphite surfaces, restrained silver borders, explicit selected states and a high-contrast primary equals action.

## Release platforms

Releases are multi-platform by default. The same versioned source is built and tested across all three targets:

- Linux x64: `infiltrator-calc_<version>_amd64.deb`;
- Windows x64: `infiltrator-calc_<version>_windows_x64.exe`, a native standalone Win32 executable built with the static MSVC runtime and no GTK/GLib runtime bundle;
- iPhone: a native SwiftUI application is compiled for both iOS Simulator and real iPhone ARM64 device architecture. CI publishes an iOS Simulator bundle and an unsigned device bundle for build verification.

A signed installable `.ipa` requires an Apple signing identity and provisioning profile. Those credentials are deliberately not stored in the repository. Once signing is configured, the same Xcode target is ready to archive and export as an `.ipa`.

A release is published only after Linux, Windows and iOS builds and the shared-core tests succeed.

## Shared Infiltrator design

Infiltrator Calc uses Infiltratr Common as its reusable software foundation and follows the canonical Infiltrator Design v1 visual contract. The common design language is a graphite/silver foundation with near-black backgrounds, layered dark panels, restrained silver borders and MB Corpo typography roles.

The calculation core links against `InfiltratrCommon::Portable` from the exact released Common 1.18.1 gitlink. CMake verifies the checked-out Common `VERSION` is exactly `1.18.1`, so a stale or mismatched submodule fails configuration instead of silently building. The desktop UI consumes the same named typography roles and palette defined by the shared design contract rather than introducing a project-specific visual system.

## Typography

Infiltrator Calc prefers locally installed MB Corpo fonts when available and falls back automatically to normal system fonts when they are absent. The native Windows shell relies on Windows font substitution with Segoe UI as the platform fallback defined by the shared design contract.

The project does **not** redistribute proprietary MB Corpo font binaries. The current UI roles are:

- `MB Corpo S Title WEB` for normal interface text and controls;
- `MB Corpo A Title Cond WEB` for product/display titles.

This follows the established Infiltrator desktop applications.

## Build

Clone recursively so the exact Infiltratr Common revision is available:

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
├── app/                 GTK desktop application
├── core/                Portable calculation engine, session state and programmer engine
└── infiltratr-common/   Exact shared Common gitlink

ios/
├── Sources/             Native SwiftUI iPhone application
├── Bridge/              Objective-C++ bridge into the shared C++ core
└── project.yml          Reproducible XcodeGen project definition

tests/                   Core, session and programmer regression tests
docs/                    Maintained engineering/design documentation
```

## Release direction

The project uses `main` as its working branch and follows the Infiltrator release discipline: test the exact source commit, build from that commit, and publish immutable release identities.

The packaged form is a generic Debian package suitable for Infiltrator Repository and, ultimately, Infiltrator Mint.

## Licence

Copyright © 2026 Shannon Smith.

Infiltrator Calc is licensed under the GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`).
