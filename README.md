<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Calculator

**Project copyright:** © 1993-2026 Shannon Smith

Calculator is a native cross-platform calculator for the software family, with first-class Linux, Windows and iPhone interfaces.

The project is deliberately larger in ambition than a four-function calculator, but the implementation grows in layers. The calculation engine is independent of the graphical interface, while a shared platform-neutral controller defines calculator commands and interaction state once for every native shell. Desktop layout is additionally defined by a shared logical UI contract.

**Current source version:** 0.1.41  
**Language:** C++17 shared calculation core, GTK4 Linux shell, native Win32 Windows shell, SwiftUI iPhone shell with Objective-C++ bridge  
**Shared foundation:** Common 1.19.10  
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

Calculator supports **System**, **Day** and **Night** appearance modes on Linux, Windows and iPhone. System detects the host light/dark preference and resolves it to the exact Common Day or Night palette; Day is the white Infiltrator palette and Night is the MB graphite/black palette with the canonical blue accent. Platform code owns only theme detection, persistence and native rendering. Calculator consumes Common 1.19.10's semantic heading, summary, kicker, detail/note, status-border, focus/hover and warning roles where those meanings apply instead of collapsing the richer design contract into a few generic colours.

## Current capabilities

Calculator has three explicitly switchable modes presented as a visible mode strip on desktop and iPhone, so Standard, Scientific and Programmer are directly selectable rather than hidden behind a cycling control:

- Standard — traditional desktop-calculator immediate arithmetic, contextual percentages, powers, unary operations and memory;
- Scientific — expression/order-of-operations evaluation, trigonometric and inverse/hyperbolic trigonometric functions, logarithmic/exponential functions, roots and powers, factorial/percentage operations, DEG/RAD/GRAD angle units, a 2nd-function layer and F-E scientific-notation display switching; and
- Programmer — binary, octal, decimal and hexadecimal fixed-width integer arithmetic, complement, AND/OR/XOR/NAND/NOR, shifts, rotate-left/right, 8/16/32/64-bit widths, unsigned/signed display, and an on-demand simultaneous BIN/OCT/DEC/HEX representation view. Width controls are explicitly labelled W8, W16, W32 and W64 so they do not conflict with numeric keypad entry.

The calculator also provides reusable variables, bounded structured calculation history with exact Programmer-mode context and recall, explicit MC/MR/MS/M+/M− memory controls and a shared calculation-session layer above the parser. Scientific expressions can assign reusable variables such as `x=42`; the built-in constants `pi` and `e` are reserved and cannot be overwritten. Standard mode remains deliberately confined to immediate-calculator grammar rather than silently falling through to Scientific parsing.

Controls are state-aware: commands that cannot currently succeed are disabled consistently by the shared controller (for example MR/MC before memory is populated, invalid Programmer digits for the selected radix, and unary/equals operations without a usable operand). Result text is selectable on Linux, Windows and iPhone, keyboard focus is visibly indicated on desktop controls, and native accessibility semantics remain attached to platform controls. Features are considered complete only when implementation, tests and documented behaviour agree.

## Architecture summary

Linux and Windows consume the same Calculator-owned UI contract and controller: modes, button order, commands, logical desktop sizing metrics, responsive breakpoints and interaction state are defined once, then rendered through GTK4 or native Win32. iPhone retains native SwiftUI touch composition, but its Objective-C++ bridge routes expression edits and calculator commands through the same C++ controller instead of maintaining a second calculator state machine.

Detailed ownership, numerical, portability and validation contracts are maintained under [docs/](docs/README.md).

## Release platforms

Releases are multi-platform by default. The same versioned source is built and tested across all three targets:

- Linux x64: `calculator_<version>_amd64.deb` (Debian/APT package identity `infiltrator-calculator`);
- Windows x64: `calculator_<version>_windows_x64.exe`, a native standalone Win32 executable built with the static MSVC runtime and no GTK/GLib runtime bundle;
- iPhone: a native SwiftUI application is compiled for both iOS Simulator and real iPhone ARM64 device architecture. CI uses the Simulator build as test evidence and publishes only the unsigned ARM64 device bundle as the release artifact;
- Source: `calculator_<version>_source.tar.gz`, a deterministic source bundle containing Calculator and the exact checked-out Common dependency used by the release.

A signed installable `.ipa` requires an Apple signing identity and provisioning profile. Those credentials are deliberately not stored in the repository. Once signing is configured, the same Xcode target is ready to archive and export as an `.ipa`.

A release is published only after Linux, Windows and iOS builds and the shared-core tests succeed.

### Compatibility identifiers

The user-facing product name is **Calculator**. The Debian/APT package identity is `infiltrator-calculator`. The existing executable/configuration identifier `infiltrator-calc` and iOS bundle identifier `net.ssmith.infiltrator.calc` remain stable compatibility identities so settings, launch paths and installed application identity are not broken. Existing `infiltrator-calc` Debian package installations migrate through the central repository transition package.

## Shared design

Calculator uses Common as its reusable software foundation and follows the canonical shared Design v1 visual contract. The common design language is a graphite/silver foundation with near-black backgrounds, layered dark panels, restrained silver borders and MB Corpo typography roles.

Common 1.19.10 is the released shared presentation and infrastructure baseline. Calculator keeps its proven domain boundary while consuming the complete Day/Night semantic palette, canonical typography/metrics, parser and checked infrastructure through Common rather than maintaining local copies. No Calculator-domain semantics are moved into Common merely to increase reuse.

The calculation core links against `InfiltratrCommon::Portable` from the exact released Common 1.19.10 gitlink. CMake verifies both the checked-out Common `VERSION` and, in repository builds, the exact immutable `33e69c0a…` 1.19.10 commit, so a stale or same-version/wrong-revision submodule fails configuration instead of silently building. Calculator now consumes Common's exact decimal-token parser, native semantic palette, structural design metrics and typography identity rather than maintaining private copies of those product-neutral contracts. The iPhone target compiles the same Common Portable source set and obtains its Day/Night semantic palette through the Objective-C++ bridge instead of carrying a Swift colour mirror.

## Typography

Calculator-owned text is restricted to the same three established MB Corpo faces on Linux, Windows and iPhone. There is no deliberate generic fourth text family such as Sans or Segoe UI.

The three approved font files and roles are:

- `mb_corpo_s_regular.ttf` — `MB Corpo S Title WEB` regular interface text;
- `mb_corpo_s_bold.ttf` — `MB Corpo S Title WEB` bold interface text, actions and emphasis;
- `mb_corpo_a_cond_regular.ttf` — `MB Corpo A Title Cond WEB` product and display titles.

Common 1.19.10 owns the canonical MB Corpo family names, role weights, filenames and immutable first-party archive provenance. Calculator deliberately applies Common's permitted strict no-fallback policy: release builds fetch the exact Common-declared archive, verify its archive/file hashes, then package the three faces with Linux, embed them privately in the Windows executable, and bundle them into the iPhone application. Linux and iPhone refuse silent Calculator-owned text-font substitution if the required faces are unavailable; Windows registers the embedded faces into the process before creating UI fonts.

## Build

Clone recursively so the exact Common revision is available:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Calculator.git
cd Calculator
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Debian-family systems the initial development dependencies are the standard C/C++ toolchain, CMake, GTK4 development packages and pkg-config. MPFR is optional for ordinary builds; release CI enables the dedicated high-precision numerical oracle with `-DCALCULATOR_ENABLE_MPFR_ORACLE_TESTS=ON` and `libmpfr-dev`. Windows builds use Visual Studio/MSVC and the Windows SDK only; GTK, GLib, MinGW and third-party runtime DLLs are not required for the Windows executable.

## Project structure

```text
src/
├── app/                 Native GTK/Linux and Win32/Windows platform shells
├── core/                Portable calculation engine, session state and Programmer engine
├── ui/                  Shared command/controller contract, desktop metrics and theme adapter
└── infiltratr-common/   Exact shared Common gitlink

ios/
├── Sources/             Native SwiftUI iPhone application
├── Bridge/              Objective-C++ bridge into the shared C++ core
└── project.yml          Reproducible XcodeGen project definition

tests/                   Core, session, UI-contract and cross-platform regression tests
docs/                    Architecture, design, numerics, portability and validation contracts
```

## Release direction

The project uses `main` as its working branch and follows the project release discipline: test the exact source commit, build from that commit, and publish immutable release identities.

The packaged form is a generic Debian package suitable for the Package Repository and future distribution integration.

## Licence

Copyright © 1993-2026 Shannon Smith.

Calculator is licensed under the GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`).
