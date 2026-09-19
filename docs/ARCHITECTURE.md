<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Architecture

Calculator is structured so that calculation semantics are independent of any graphical toolkit. The architecture separates mathematical domains, session state, calculator interaction state, shared product-neutral infrastructure and platform rendering.

## First-principles design

Calculator begins with explicit mathematical and interaction contracts rather than treating another calculator application as the specification. Conventional calculator behaviour, numerical standards, platform conventions and mature implementations are evidence to examine; Calculator retains ownership of the semantics it exposes.

First principles does not mean reimplementing every mechanism. The C++ standard library, native platform toolkits and pinned Common are used where their documented contracts are the stronger engineering choice. Dependencies provide mechanisms; Calculator-specific arithmetic, parser, mode and interaction policy remain Calculator-owned.

## Architectural drivers

The design is guided by five constraints:

1. the same calculation semantics must be testable without a GUI;
2. Standard, Scientific and Programmer modes have intentionally different evaluation rules and must not be collapsed into one ambiguous parser;
3. Linux, Windows and iPhone should share domain logic without forcing one platform's UI toolkit onto another;
4. reusable product-neutral facilities belong in Common, while calculator-specific behaviour remains in this repository; and
5. releases must be reproducible from an exact source revision and exact Common dependency.

## Dependency direction

The principal dependency direction is:

```text
Linux GTK shell ────────┐
Windows Win32 shell ────┼─> UI contract/controller ─> Session ─> expression core
iPhone SwiftUI + bridge ─┘                             └──────> Programmer core

Calculator core / theme adapters / iOS bridge ─> Infiltratr Common
```

Higher layers may adapt lower-layer state for presentation. Lower layers must not depend on GTK, Win32, SwiftUI or other platform UI APIs.

## Calculation domains

### Expression core

`src/core/calculator.*` owns the binary64 expression domain used by Scientific calculations and reusable variables. It implements operator precedence, parentheses, constants, mathematical functions, postfix percentage/factorial operations and finite-result validation.

Decimal token boundaries are identified by Calculator, then numeric conversion is delegated to Common's locale-independent decimal parser. This keeps expression grammar calculator-specific while sharing the product-neutral conversion primitive.

### Standard immediate evaluation

Standard mode deliberately uses a separate immediate evaluator. Binary operators are applied from left to right in conventional desktop-calculator order, and percentage interpretation is contextual to the pending operator. This is a user-interaction semantic, not a variant of the Scientific grammar.

### Programmer domain

`src/core/programmer.*` owns fixed-width integer calculation. Values are represented as bit patterns constrained to the selected 8, 16, 32 or 64-bit width. Arithmetic overflow wraps by masking to that width; this is intentional Programmer-mode behaviour rather than an unchecked error in the general arithmetic domain.

Signedness affects decimal presentation of the resulting bit pattern. It does not create a separate stored numeric representation.

## Session layer

`src/core/session.*` owns process-local state that spans individual calculations:

- reusable variables;
- calculator memory, including whether memory has been explicitly populated; and
- bounded calculation history.

The session delegates mathematical evaluation to the core. It does not implement an alternate expression grammar.

History is bounded by construction so an indefinitely running UI cannot grow it without limit. Variables, memory and history are currently session state rather than durable user data.

## Shared interaction layer

`src/ui/calculator_ui_contract.hpp` defines the Calculator command vocabulary and canonical button specifications. It also owns desktop-only logical layout metrics and responsive layout classes.

`src/ui/calculator_ui_controller.*` owns calculator interaction state and command dispatch across all three platforms. GTK and Win32 render it directly; the iPhone Objective-C++ bridge exposes its state and command surface to SwiftUI. Platform code must not maintain competing calculator state machines.

The shared desktop metrics remain logical units. GTK consumes them through its layout system; Win32 maps them through per-monitor DPI-aware platform scaling. SwiftUI does not consume those desktop geometry values.

## Platform shells

### Linux

`src/app/main.cpp` is a GTK4 adapter. It owns GTK widgets, CSS generation, system-theme observation, font discovery, clipboard/window integration and Linux theme persistence.

### Windows

`src/app/windows_main.cpp` is a native Win32 adapter. It owns HWND/GDI rendering, DPI conversion, Windows theme observation, native font selection, Registry-backed theme persistence and Windows message handling.

### iPhone

The iPhone interface is native SwiftUI. `ios/Bridge/CalculatorBridge.mm` is the language boundary between Swift and the shared C++ controller.

SwiftUI owns touch-native composition, platform appearance observation and presentation. Expression edits, mode changes, command enablement, memory/history state and calculator operations are routed through the same C++ controller used by the desktop shells. The bridge also exposes the canonical Common Day/Night palette. Desktop geometry and cursor mechanics remain desktop concerns and are not imposed on SwiftUI.

## Common boundary

Common is an exact git submodule dependency. It owns reusable facilities whose semantics are not specific to Calculator, including deterministic decimal conversion and the shared Design v1 semantic theme contract.

Calculator owns expression grammar, immediate-calculator behaviour, Programmer-mode width semantics, calculator state, command semantics and calculator-specific layout. Code is moved into Common only when it has a stable product-neutral contract and a demonstrated shared consumer.

## Numerical representation

Standard and Scientific currently use the binary64 real-number domain; Programmer uses explicit fixed-width integer bit patterns. Representation is part of the contract rather than an implementation detail hidden by formatting.

The detailed numerical model, parser semantics, evidence hierarchy, display policy and cross-platform expectations are maintained in [NUMERICS.md](NUMERICS.md). Cross-platform representation and ABI assumptions are maintained in [PORTABILITY.md](PORTABILITY.md).

## Error model

Calculation APIs return explicit success/error results. Invalid syntax, division by zero, unsupported domains and non-finite results are reported as calculation errors rather than silently propagated into the UI.

Platform shells translate those results into presentation state; they do not reinterpret mathematical failure.

## Verification and release boundary

The test suite covers the expression grammar, bounded parser depth, immediate semantics, Programmer behaviour, session state, UI contract/controller and cross-platform ownership rules. Windows additionally performs native runtime smoke checks. iOS CI compiles both the Simulator and unsigned ARM64 device targets; the Simulator is test evidence, not a public release artifact.

A release is published only after Linux, Windows and iOS jobs succeed from the same source revision. Public binaries and a deterministic source bundle containing the exact Common checkout are derived from that revision, and the Debian package is then verified through the Package Repository publication path. [VALIDATION.md](VALIDATION.md) defines what each evidence class proves and does not prove.

## Compatibility and runtime identity

The user-facing product and repository are Calculator. Runtime/configuration identifiers that would break installed upgrades, persisted settings or application identity may intentionally retain compatibility naming.

Compatibility identity is not branding. A migration is justified only when it preserves or deliberately transforms existing user state and package continuity. This distinction is recorded in [DECISIONS.md](DECISIONS.md) and [PORTABILITY.md](PORTABILITY.md).

## Security and trust model

Calculator parses local expression/variable text and platform configuration as external input. Malformed syntax, numeric overflow/domain failure and unsupported Programmer input must remain contained calculation failures rather than causing undefined behaviour or host command execution.

The product has no project-owned privileged daemon or network service. Platform shells own local preference and native UI integration; the portable core must not acquire platform authority merely because a frontend supplies input.

Release integrity is part of the trust boundary: published artifacts must derive from the exact qualified source revision and must match the release checksum set.

## Specialist documents

- [NUMERICS.md](NUMERICS.md) — numeric domains, parser/percentage semantics, evidence basis and precision/error expectations.
- [PORTABILITY.md](PORTABILITY.md) — platform, representation, locale, ABI and compatibility boundaries.
- [UI_PARITY.md](UI_PARITY.md) — shared desktop interaction ownership and iPhone parity boundary.
- [VALIDATION.md](VALIDATION.md) — automated, native-environment and release evidence limits.

## Design rule

A calculation rule must have one authoritative implementation. If a platform shell needs to reproduce mathematical or calculator-state logic in order to render a control, the abstraction boundary is wrong and should be corrected before adding another copy.


## Packaging boundary

Common is a build/link dependency only. Calculator adds it with `EXCLUDE_FROM_ALL`; Calculator packages must not install Common libraries, headers or CMake metadata.
