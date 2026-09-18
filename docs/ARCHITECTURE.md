<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Calculator Architecture

Calculator is structured so that calculation semantics are independent of any graphical toolkit. The architecture separates mathematical domains, session state, calculator interaction state, shared product-neutral infrastructure and platform rendering.

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
Linux GTK shell ─┐
Windows Win32 ───┼─> desktop UI contract/controller ─> Session ─> expression core
                 │                                      └──────> Programmer core
iPhone SwiftUI ──┴─> Objective-C++ bridge ─────────────> Session / Programmer core

Calculator core / desktop theme adapter / iOS bridge ─> Infiltratr Common
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

## Desktop interaction layer

`src/ui/calculator_ui_contract.hpp` is the declarative desktop interaction contract. It defines mode names, commands, button labels/roles/order, shared logical layout metrics and responsive layout classes.

`src/ui/calculator_ui_controller.*` owns desktop calculator interaction state and command dispatch. GTK and Win32 render the controller state and return user commands to it; they must not maintain competing calculator state machines.

The shared metrics are logical desktop units. GTK consumes them through its layout system; Win32 maps them through DPI-aware platform scaling.

## Platform shells

### Linux

`src/app/main.cpp` is a GTK4 adapter. It owns GTK widgets, CSS generation, system-theme observation, font discovery, clipboard/window integration and Linux theme persistence.

### Windows

`src/app/windows_main.cpp` is a native Win32 adapter. It owns HWND/GDI rendering, DPI conversion, Windows theme observation, native font selection, Registry-backed theme persistence and Windows message handling.

### iPhone

The iPhone interface is native SwiftUI. `ios/Bridge/InfiltratorCalcBridge.mm` is the language boundary between Swift and the shared C++ core.

The iPhone does not use the desktop UI controller because desktop layout/cursor mechanics are not an appropriate abstraction for a touch-native SwiftUI interface. It does, however, reuse the calculation/session/programmer engines and obtains the canonical Common Day/Night palette through the bridge. Swift owns only the platform interaction adaptation and System appearance resolution.

## Common boundary

Common is an exact git submodule dependency. It owns reusable facilities whose semantics are not specific to Calculator, including deterministic decimal conversion and the shared Design v1 semantic theme contract.

Calculator owns expression grammar, immediate-calculator behaviour, Programmer-mode width semantics, calculator state, command semantics and calculator-specific layout. Code is moved into Common only when it has a stable product-neutral contract and a demonstrated shared consumer.

## Numerical representation

The current real-number domain is IEEE-754 binary64 (`double`). This is sufficient for the current Standard and Scientific feature set and maps efficiently to platform math libraries.

Binary64 is not treated as the final answer for every future capability. Exact integer, arbitrary-precision, complex or unit-aware domains should be introduced as distinct representations when their correctness requirements justify them rather than being simulated through formatting around `double`.

## Error model

Calculation APIs return explicit success/error results. Invalid syntax, division by zero, unsupported domains and non-finite results are reported as calculation errors rather than silently propagated into the UI.

Platform shells translate those results into presentation state; they do not reinterpret mathematical failure.

## Verification and release boundary

The test suite covers the expression grammar, immediate semantics, Programmer behaviour, session state, UI contract/controller and cross-platform ownership rules. Windows additionally performs native runtime smoke checks. iOS CI builds both Simulator and unsigned ARM64 device applications.

A release is published only after Linux, Windows and iOS jobs succeed from the same source revision. The Debian package is then verified through the Package Repository publication path.

## Design rule

A calculation rule must have one authoritative implementation. If a platform shell needs to reproduce mathematical or calculator-state logic in order to render a control, the abstraction boundary is wrong and should be corrected before adding another copy.
