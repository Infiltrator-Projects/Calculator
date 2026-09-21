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

Advanced Tools ───────────────────────────────> expression core (graph/root evaluation)
Calculator core / theme adapters / iOS bridge ─> Infiltratr Common
```

Higher layers may adapt lower-layer state for presentation. Lower layers must not depend on GTK, Win32, SwiftUI or other platform UI APIs.

## Calculation domains

### Expression core

src/core/scientific.* owns the arbitrary-precision real/complex expression domain used by Scientific calculations and precise reusable variables. It implements operator precedence, parentheses, constants, mathematical functions, postfix percentage/factorial operations and finite-result validation without routing Scientific intermediates through binary64. src/core/calculator.* remains the binary64 foundation used by Standard mode and supporting finite-real facilities.

Calculator decides where the expression grammar expects a number, then Common 1.19.10's `infiltratr_parse_double_token()` owns token recognition and exact locale-independent binary64 conversion. This removes private scanner mechanics without transferring Calculator's operator/function grammar into Common.

Elementary unary/scientific operations are exposed once by the calculation core through `apply_real_function()`. The expression parser and interactive controller both consume that contract, preventing platform/controller copies of square-root, reciprocal, trigonometric and logarithmic semantics.

### Standard immediate evaluation

Standard mode deliberately uses a separate immediate evaluator. Binary operators are applied from left to right in conventional desktop-calculator order, and percentage interpretation is contextual to the pending operator. This is a user-interaction semantic, not a variant of the Scientific grammar.

### Programmer domain

`src/core/programmer.*` owns fixed-width integer calculation. Values are represented as bit patterns constrained to the selected 8, 16, 32 or 64-bit width. Arithmetic overflow wraps by masking to that width; this is intentional Programmer-mode behaviour rather than an unchecked error in the general arithmetic domain. NAND/NOR and ROL/ROR live in the same width-aware layer, so masking and rotate-count semantics are portable and testable.

Signedness affects decimal presentation of the resulting bit pattern. It does not create a separate stored numeric representation.

### Advanced tool domains

`src/core/advanced_tools.*` owns the twelve non-keypad calculation families. It is a portable C++ layer over explicit domain contracts rather than a platform helper: engineering formulae, dimensional unit conversion, IPv4/CIDR and transfer calculations, storage/filesystem calculations, civil-date arithmetic, constants, descriptive statistics, graph sampling, real root solving, exact decimal/rational arithmetic, arbitrary-precision integer arithmetic and complex arithmetic.

Graphing and equation solving reuse the Scientific expression evaluator with a supplied `x` variable; they do not carry a second expression grammar. Graph sampling is shared, while each shell renders the resulting point sequence natively. Exact decimal arithmetic uses decimal literals as exact rationals rather than converting them through binary64. Arbitrary precision is a separate integer domain with an explicit 20,000-digit safety ceiling. Complex arithmetic is an explicit pair-of-binary64 domain and is not silently mixed into the ordinary real parser.

The Tools workbench is intentionally on-demand. It keeps the main Standard/Scientific/Programmer keypads compact while giving all three platform shells the same catalogue, prompt/example metadata, evaluation results and graph points.

## Session layer

`src/core/session.*` owns process-local state that spans individual calculations:

- reusable variables;
- calculator memory, including explicit store (MS), recall, clear and arithmetic update state; and
- calculation history, unbounded by default with an optional explicit limit for embedders/tests.

The session delegates mathematical evaluation to the core. It does not implement an alternate expression grammar.

History is unbounded by default to match the maintained desktop behaviour, while embedders/tests may request an explicit Session limit. Session owns a monotonically advancing history revision and the canonical bounded history-text projection used by compact docks; platform shells update history views only when that revision changes instead of rebuilding history on every keystroke. Variables, memory and history are currently session state rather than durable user data.

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

SwiftUI owns touch-native composition, platform appearance observation and presentation. Expression edits, mode changes, command enablement, Scientific angle/2nd/HYP/F-E state, memory/history state and calculator operations are routed through the same C++ controller used by the desktop shells. The bridge also exposes the canonical Common Day/Night palette. Desktop geometry and cursor mechanics remain desktop concerns and are not imposed on SwiftUI.

## Common boundary

Common is an exact git submodule dependency. Common 1.19.10 owns reusable facilities whose semantics are not specific to Calculator, including exact decimal-token conversion, the Design v1 semantic palette and structural roles, rendering metrics, canonical typography identity and immutable MB Corpo asset provenance. Calculator consumes those contracts while deliberately retaining its stricter no-fallback font policy.

Calculator must consume Common through its published public surface. It must not include Common private headers such as internal ASCII/read helpers, because doing so would reverse the intended ownership boundary. Common's expanded semantic appearance roles are consumed where their meanings fit Calculator; connection-specific roles remain unused because Calculator has no connection concept.

Calculator owns expression grammar, immediate-calculator behaviour, Programmer-mode width semantics, calculator state, command semantics and calculator-specific layout. Code is moved into Common only when it has a stable product-neutral contract and a demonstrated shared consumer.

## Numerical representation

Standard uses the binary64 real-number domain for conventional immediate-calculator semantics; Scientific uses the shared arbitrary-precision real/complex domain; Programmer uses explicit fixed-width integer bit patterns. Representation is part of the contract rather than an implementation detail hidden by formatting.

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
- [TOOLS.md](TOOLS.md) — Advanced Tools catalogue, input grammar, domains and validation rules.
- [PORTABILITY.md](PORTABILITY.md) — platform, representation, locale, ABI and compatibility boundaries.
- [UI_PARITY.md](UI_PARITY.md) — shared desktop interaction ownership and iPhone parity boundary.
- [VALIDATION.md](VALIDATION.md) — automated, native-environment and release evidence limits.

## Design rule

A calculation rule must have one authoritative implementation. If a platform shell needs to reproduce mathematical or calculator-state logic in order to render a control, the abstraction boundary is wrong and should be corrected before adding another copy.


## Packaging boundary

Common is a build/link dependency only. Calculator adds it with `EXCLUDE_FROM_ALL`; Calculator packages must not install Common libraries, headers or CMake metadata.


## Structured history and recall

History is a bounded domain model rather than a platform-owned text log. Each entry records its Calculator mode, input, exact rendered output and success state. Programmer entries additionally preserve radix, fixed width and signed-display context so recall is deterministic even for values that cannot be represented exactly by binary64.

The shared Controller exposes newest-first history access and recall. Platform shells render and select those entries but do not reconstruct mode state or reinterpret history themselves. Wide desktop history docking remains a non-interactive summary; explicit recall is handled by the dedicated history surface.


## Common 1.19.10 semantic presentation boundary

Calculator treats Common's semantic design palette as data, not as a product-specific widget implementation. Native shells resolve the same Common Day/Night structure and map only semantically relevant roles: headings/results, summaries/input text, kicker/detail/note text, status borders, selected summaries, warning states and focus/hover accents. Connection-specific roles remain unused because Calculator has no connection concept; consuming every field mechanically would weaken rather than strengthen the ownership model.

Windows additionally consumes Common's titlebar and status-border roles for DWM non-client rendering. GTK and Win32 expose keyboard-visible focus using the shared accent-hover role, while SwiftUI keeps native focus/accessibility behaviour and receives explicit key labels/selection state.


## Additional Results boundary

Additional Results is a Calculator-owned representation model, not another calculation engine. The shared Controller evaluates the active domain once and emits labelled representations for native shells to display.

Standard exposes decimal, scientific and engineering forms of its finite binary64 value. Scientific builds General/Fixed/Scientific/Engineering representations directly from its retained arbitrary-precision real/complex value. Programmer exposes hexadecimal, signed-or-unsigned decimal, octal and grouped binary forms of the same masked fixed-width bit pattern. Platform shells do not re-evaluate expressions, down-convert precise Scientific values or reinterpret signedness to build these rows.

This deliberately creates an extensible result surface before future exact, unit, complex or symbolic domains are introduced. New result families can extend the shared model without increasing keypad density or creating platform-specific mathematics.
