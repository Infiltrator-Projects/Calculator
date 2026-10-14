<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Architecture

Calculator separates mathematical domains, session state, calculator interaction state, shared product-neutral infrastructure and native platform rendering. Calculation semantics are intentionally independent of graphical toolkits.

## First-principles design

Calculator begins with explicit mathematical and interaction contracts rather than treating another calculator application as its specification. Conventional behaviour, numerical standards, platform conventions and mature implementations are evidence; Calculator retains ownership of the semantics it exposes.

First principles does not mean reimplementing mechanisms that already have a stronger generic implementation. The C++ standard library, Boost.Multiprecision, native platform toolkits and pinned Common are used where their contracts are stronger. Calculator-specific arithmetic, parser, mode and interaction policy remain Calculator-owned.

## Architectural drivers

The design is governed by five constraints:

1. calculation semantics must be testable without a GUI;
2. Standard, Scientific and Programmer have deliberately different numeric/capability domains and must not be collapsed into one ambiguous parser;
3. Linux, Windows and iPhone share domain logic without forcing one UI toolkit across platforms;
4. reusable product-neutral facilities belong in Common, while calculator-specific behaviour remains here; and
5. a release is an exact source revision plus exact dependency identities, not merely a version string.

## Dependency direction

```text
Linux GTK shell ────────┐
Windows Win32 shell ────┼─> UI contract/controller ─> Session ─> expression core
iPhone SwiftUI + bridge ─┘                             └──────> Programmer core

Advanced Tools ───────────────────────────────> Scientific/domain mechanisms
Calculator core / theme adapters / iOS bridge ─> Infiltratr Common
```

Higher layers may adapt lower-layer state for presentation. Lower layers must not depend on GTK, Win32, SwiftUI or other platform UI APIs.

## Calculation domains

### Expression core

`src/core/scientific.*` owns the arbitrary-precision real/complex expression domain used by Scientific calculations and precise reusable variables. It implements operator precedence, parentheses, constants, mathematical functions, postfix percentage/factorial operations and finite-result validation without routing Scientific intermediates through binary64. `src/core/calculator.*` remains the binary64 foundation used by Standard mode and supporting finite-real facilities.

For the binary64 expression path, Calculator decides where its grammar expects a number and Common 1.19.35 `infiltratr_parse_double_token()` owns locale-independent binary64 token conversion. Scientific decimal literals are parsed directly into the multiprecision domain. This shares conversion mechanics without transferring Calculator grammar into Common or lowering Scientific input through `double`.

Elementary unary/scientific operations are exposed once by the calculation core through `apply_real_function()`. The expression parser and interactive controller consume that contract rather than carrying controller/platform copies of square-root, reciprocal, trigonometric and logarithmic semantics.

### Standard arithmetic

Standard uses the binary64 arithmetic parser with named constants, variables and functions disabled. Its smaller keypad and grammar surface are capability distinctions, not permission to redefine arithmetic: written infix expressions use conventional precedence, exponentiation is right-associative, parentheses override precedence, and postfix `%` means division by 100.

The same written arithmetic operators therefore have the same mathematical meaning in Standard and Scientific. The modes differ in available terms and numeric representation/precision.

Binary64 values crossing back into computational text use shortest exact round-trip serialization. Human result formatting may round or group for presentation but never becomes later computational state. Memory maintains an explicit binary64-availability invariant so overflow or a Scientific-only out-of-range value cannot silently become a different Standard value.

### Scientific resource boundary

Scientific evaluation and the iterative/arbitrary-precision Advanced Tools share `EvaluationBudget`. It owns the cumulative-work counter, Scientific parser/function recursion limits, input ceiling, arbitrary-precision output ceiling and optional deadline/cancellation path. Caller-supplied limits may tighten product safety bounds but may not relax the hard 20,000-digit arbitrary-precision output ceiling.

Standard's binary64 parser deliberately retains its own parser-depth policy. That is not duplicated ownership of one concern: the Standard and Scientific evaluators are different numeric domains with different workload and representation contracts.

### Programmer domain

`src/core/programmer.*` owns fixed-width integer calculation. Values are bit patterns constrained to the selected 8, 16, 32 or 64-bit width. Arithmetic overflow wraps by masking to that width; this is intentional Programmer behaviour rather than an unchecked error in a general integer engine. Bit operations, shifts, rotates and byte swap live in the same width-aware layer.

Signedness affects decimal presentation of the resulting bit pattern. It does not create a second stored representation.

### Advanced Tool domains

`src/core/advanced_tools.hpp` is the public boundary for fourteen non-keypad calculation families. Implementation is decomposed by cohesive domain: the dispatcher/catalogue in `advanced_tools.cpp`; private adapters in `advanced_tools_internal.hpp`; conversion/network/storage/time in `advanced_tools_conversion.cpp`; statistics/graph/root in `advanced_tools_analysis.cpp`; exact rational/arbitrary integer in `advanced_tools_exact.cpp`; and financial/Unicode/number/complex in `advanced_tools_number.cpp`.

Graphing and equation solving reuse the Scientific evaluator with supplied `x`; they do not carry a second expression grammar. One `EvaluationBudget` spans all probes so thousands of individually small evaluations cannot reset resource accounting. Samples remain at Advanced Tools Scientific precision until finite graph geometry is converted to binary64.

Unit conversion evaluates Calculator-owned exact decimal/ratio factor expressions in the Scientific domain, so factors such as 5/9 and pi/180 are not first rounded into binary64. Exact decimal arithmetic represents decimal literals as exact rationals. Exact rational and arbitrary-integer arithmetic use Boost.Multiprecision `cpp_int` as the generic integer mechanism; Calculator owns only the grammar, rational normalization, operation limits and user-visible failure semantics. Complex Advanced Tools route through the Scientific multiprecision domain rather than maintaining a weaker parallel binary64 complex engine.

The Tools workbench is intentionally on demand. It keeps the primary Standard/Scientific/Programmer surfaces compact while all native shells consume the same catalogue, prompt/example metadata, evaluation results and graph points.

## Session layer

`src/core/session.*` owns process-local state spanning calculations: reusable variables and functions, a newest-first memory stack, and structured calculation history. History is unbounded by default; embedders/tests may set an explicit limit.

Session delegates mathematical evaluation to the core. It owns no alternate expression grammar. Its history revision allows compact views to update only when history changes. Memory slots retain Scientific values plus an optional exact binary64 view, so Standard recall never fabricates a lossy substitute for a Scientific-only value.

## Shared interaction layer

`src/ui/calculator_ui_contract.hpp` defines the command vocabulary, canonical button specifications and desktop logical layout metrics. `calculator_ui_controller.cpp` owns interaction mutation and command dispatch; `calculator_ui_state.cpp` owns session/history and the versioned semantic-state document; and `calculator_ui_presentation.cpp` owns derived labels, enablement and representations. These are translation-unit boundaries around one `Controller` object, not competing state objects. GTK and Win32 render that controller directly; the iPhone Objective-C++ bridge exposes it to SwiftUI.

Scientific live preview, precision and identifier completion are controller-owned interaction semantics. Controller also owns the versioned persistent-state document for reusable variables/functions, Scientific precision and result-presentation preferences. Linux, Windows and iPhone store exactly those bytes through native mechanisms; no shell defines a competing persistence grammar.

## Platform shells

### Linux

`src/app/main.cpp` owns GTK widget composition, CSS generation, system-theme observation, font discovery and event/window orchestration. `src/app/linux_persistence.*` is the sole Linux storage adapter for XDG locations, compatibility readers and durable writes. The entry point therefore adapts Controller state without also owning persistence mechanics.

### Windows

`src/app/windows_main.cpp` owns HWND/GDI composition, native font selection and message/event orchestration. `src/app/windows_platform.*` owns reusable Win32 text/DPI/theme primitives; `src/app/windows_persistence.*` is the sole Calculator Registry-storage adapter. The main translation unit no longer mixes rendering choreography with storage policy and reusable native mechanics.

### iPhone

The iPhone interface is SwiftUI. `ios/Bridge/CalculatorBridge.mm` is the language boundary to the same C++ Controller. SwiftUI owns touch-native composition and platform presentation; expression edits and calculator commands remain shared semantics.

XcodeGen consumes Calculator's own `src/core` and `src/ui` directories as cohesive source trees. This avoids a second per-file Calculator manifest that could drift from the desktop build. Common is intentionally different: only its portable C subset belongs in iOS, and `tests/test_common_contract.py` derives the expected portable list from Common's own CMake contract.

## Common boundary

Common is an exact git-submodule dependency. The current authority is Common 1.19.35 at commit `7cc5de3de0e94ed2cfcff0840bbb5346eb5c9c9f`. CMake and `tests/test_common_contract.py` validate both public VERSION and exact gitlink because a different commit carrying the same version text is not an interchangeable release input.

Common owns product-neutral numeric parsing, deterministic ASCII helpers, checked arithmetic, durable POSIX I/O, Design v1 semantic palette/metrics, typography identity and immutable MB Corpo asset provenance. Calculator consumes only Common's published headers. Private Common headers are forbidden by contract tests.

Calculator retains expression grammar, mode capability restrictions, Programmer width semantics, calculator/session state, command semantics and Calculator-specific layout. A mechanism moves to Common only when it has a stable product-neutral contract and Common's contract is at least as strong.

## Numerical representation

Standard is IEEE-754 binary64 real arithmetic; Scientific is arbitrary-precision real/complex; Programmer is explicit fixed-width bit patterns; exact rational and arbitrary integer tools use separate exact domains. Representation is a product contract, not a formatting detail.

Detailed numerical semantics and evidence are maintained in [NUMERICS.md](NUMERICS.md); cross-platform representation and ABI assumptions in [PORTABILITY.md](PORTABILITY.md).

## Error model

Calculation APIs return explicit success/error results. Invalid syntax, division by zero, unsupported domains, resource-limit exhaustion and non-finite results are calculation failures rather than silently propagated state. Platform shells present those results; they do not reinterpret mathematical failure.

## Verification and release boundary

The suite covers grammar and numeric boundaries, parser depth, Standard precedence, Programmer semantics, session/controller state, Advanced Tools resource limits, Common integration, source ownership, native runtime/package behaviour and platform builds. Windows performs a native runtime/dependency smoke check. iOS CI compiles Simulator and unsigned ARM64-device targets; that proves target compilation and package structure, not signed physical-device installation.

A release is published only after Linux, Windows, iOS and sanitizer gates succeed from one source revision. VERSION is the version authority and redundant Debian/CHANGELOG/AppStream metadata must agree. Manual publication is recovery only: an existing release may be reconciled only while checked out at its exact tag commit; a not-yet-tagged release may be recovered only from the commit that introduced that forward VERSION transition. This prevents later same-version source from replacing immutable release bytes.

Public platform binaries, the deterministic source bundle, native installer, licence and checksum manifest derive from the qualified revision. [VALIDATION.md](VALIDATION.md) defines what each evidence class proves and does not prove.

## Compatibility and runtime identity

The user-facing product and repository are Calculator. Runtime/configuration identifiers that would break installed upgrades, persisted settings or application identity may intentionally retain compatibility naming. Compatibility identity is not branding; migration is justified only when it safely preserves or deliberately transforms user state/package continuity.

## Security and trust model

Calculator parses local expressions, persisted state and platform configuration as external input. Malformed syntax, numeric-domain failure and resource exhaustion must remain contained calculation failures rather than undefined behaviour or host command execution.

The product has no project-owned privileged daemon or network service. Platform shells own local preference/native UI integration; the portable core must not acquire platform authority merely because a frontend supplies input.

Release integrity is part of this trust boundary: published artifacts must derive from the exact qualified source revision and match the canonical checksum set.

## Specialist documents

- [NUMERICS.md](NUMERICS.md) — numeric domains, parser/percentage semantics, evidence basis and precision/error expectations.
- [TOOLS.md](TOOLS.md) — Advanced Tools catalogue, input grammar, domains and validation rules.
- [PORTABILITY.md](PORTABILITY.md) — platform, representation, locale, ABI and compatibility boundaries.
- [UI_PARITY.md](UI_PARITY.md) — shared interaction ownership and iPhone parity boundary.
- [VALIDATION.md](VALIDATION.md) — automated, native-environment and release evidence limits.

## Design rule

A calculation rule must have one authoritative implementation. If a shell must reproduce mathematical or calculator-state logic to render a control, the boundary is wrong and should be corrected before adding another copy.

## Packaging boundary

Common is a build/link dependency only. Calculator adds it with `EXCLUDE_FROM_ALL`; Calculator packages must not install Common libraries, headers or CMake metadata.

## Structured history and recall

History is structured domain state rather than a platform-owned text log. Each entry records mode, input, exact rendered output and success state. Programmer entries additionally preserve radix, width and signed-display context so recall is deterministic. Platform shells render/select entries but do not reconstruct mode state or reinterpret history.

## Common 1.19.35 semantic presentation boundary

Calculator treats Common's semantic design palette as data, not as a product-specific widget implementation. Native shells map only roles whose semantics apply: headings/results, summaries/input text, kicker/detail/note text, status borders, selected summaries, warnings and focus/hover accents. Connection-specific roles remain unused because Calculator has no connection concept.

Windows additionally consumes Common titlebar/status-border roles for DWM non-client rendering. GTK and Win32 expose keyboard-visible focus through the shared accent semantics; SwiftUI retains native focus/accessibility behaviour while consuming shared labels and selection state.

## Additional Results boundary

Additional Results is a Calculator-owned representation model, not another calculation engine. Controller evaluates the active domain once and emits labelled representations for native shells.

Standard exposes decimal/scientific/engineering forms of its finite binary64 value. Scientific builds General/Fixed/Scientific/Engineering forms from its retained arbitrary-precision value. Programmer exposes hexadecimal, signed-or-unsigned decimal, octal and grouped binary forms of one masked bit pattern. Shells do not re-evaluate expressions, down-convert Scientific values or reinterpret signedness to build these rows.
