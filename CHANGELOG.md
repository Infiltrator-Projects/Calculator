# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for Calculator. Detailed commit-by-commit history remains in Git.

## Unreleased

No unreleased changes.

## 0.2.20 — 2026-09-22

- Forensically validate Scientific mode with a new 8192-bit MPFR/MPC oracle covering deterministic complex arithmetic fuzz, every Scientific function family, complex principal branches/branch cuts, DEG/RAD/GRAD conversion, constants and requested precision from 16 through 1000 digits.
- Add a portable Scientific mathematical regression suite for grammar, precedence, exact complex algebra, identities, custom-function precision boundaries, state/display isolation and deterministic parser/resource failures.
- Fix exact negative-real principal-branch handling so `ln(-1)` uses +pi, non-integer negative-real powers use the same principal branch, and real half-integer powers preserve exact quadrantal phases.
- Replace generic complex power for exact integral exponents with bounded exponentiation-by-squaring and preserve exact trigonometric quadrant results instead of exposing tiny finite-pi residues.
- Keep nested custom-function evaluation at full internal precision so factoring an expression into a user function cannot introduce an extra rounding boundary.
- Tighten Scientific parser and function recursion bounds for cross-platform stack safety, fix ScientificValue-to-binary64 imaginary-zero handling, and add controller regressions proving display formatting and memory cannot alter retained Scientific state.
- Extend independent constant validation across mathematical constants, SI-defined exact constants, current measured central values, derived hbar and the defining N_A*k_B gas-constant relation.

## 0.2.19 — 2026-09-22

- Separate Standard computational state from presentation with exact shortest-round-trip binary64 serialization.
- Fix unary transforms so fixed precision, grouping and ordinary display rounding can never change subsequent arithmetic.
- Fix Standard memory recall to preserve the exact stored binary64 value; reject overflowing memory updates transactionally and keep Scientific-only out-of-range memory unavailable rather than substituting zero.
- Add a dedicated Standard mathematical forensic suite covering precedence/grouping matrices, power/unary/postfix/domain boundaries and 50,000 deterministic finite binary64 round trips.
- Extend the Linux MPFR oracle to cross-check Standard binary arithmetic and powers against independent 256-bit reference calculations.
- Document the computation/presentation boundary and memory invariants as ADR-017.

## 0.2.18 — 2026-09-22

- Make Standard mode evaluate displayed infix expressions with conventional mathematical operator precedence instead of historical immediate left-to-right execution.
- Give postfix `%` one literal mathematical meaning in Standard: divide the preceding value by 100, removing contextual pending-operator percentage behaviour.
- Keep Standard's smaller capability surface by rejecting named constants, variables and functions while sharing the binary64 arithmetic parser.
- Add direct core and shared-controller regressions for `200 + 200 / 2 = 300` and update maintained architecture/numerics/design documentation to make the greenfield rule explicit.

## 0.2.17 — 2026-09-22

- Remove the Linux GTK permanent frame-clock callback used for responsive layout polling; it kept Calculator active while idle and could drive very high CPU usage, especially under software rendering or remote desktops.
- Make Linux responsive layout event-driven using GtkWindow default-width/default-height property notifications, with no continuous idle polling.
- Add regression coverage forbidding a permanent GTK tick callback in the Linux shell.

## 0.2.16 — 2026-09-22

- Preserve Advanced Tools structured output on Windows by normalizing shared LF line endings to Win32 CRLF before assigning multiline EDIT text.
- Replace the Windows Programmer Bases information MessageBox with a native Programmer Representations & Bits window matching the Linux capability, including interactive 64-bit toggles and shared controller state.
- Treat the empty Programmer expression as the zero bit-pattern for representations, matching the displayed zero result and existing bit-grid semantics.
- Add regression coverage for Windows multiline Tools output, native Bases/Bits parity and zero-state Programmer representations.

## 0.2.15 — 2026-09-22

- Fix Advanced Tools graph contrast across Linux, Windows and iPhone by using Common's semantic text role for plotted data: light on Night surfaces and dark on Day surfaces.
- Fix Linux Advanced Tools result text disappearing in Day mode by explicitly styling GtkTextView's inner text node instead of relying on host-theme inheritance.
- Align Windows tool-result foreground with the same contrast-safe Common text role and add cross-platform source regression protection.

## 0.2.14 — 2026-09-21

- Complete the post-edit documentation audit by correcting remaining stale architecture, portability and UI-parity statements about Scientific binary64 use, the Tools catalogue, history persistence and future result domains.
- Make the binary64/Common decimal-parser boundary explicit so maintained documentation no longer implies that arbitrary-precision Scientific literals pass through `double`.
- Document the Objective-C++ bridge dictionary schemas and remove unnecessary release-number coupling from the maintained Tools catalogue description.

## 0.2.13 — 2026-09-21

- Bring canonical design, decisions, numerics, roadmap and README documentation back into agreement with the released arbitrary-precision Scientific domain, fourteen-tool catalogue, unbounded default history and Common 1.19.20 baseline.
- Mark superseded architectural decisions explicitly and add current ADRs for Scientific's multiprecision domain and the Common 1.19.20 public dependency boundary.
- Strengthen public API and implementation commentary around precision, parser limits, persistence transactions, controller caching/state transitions and native bridge contracts without narrating straightforward syntax.
- Clarify the repository's documentation/comment maintenance standard so non-obvious lifetime, ownership, side-effect, failure, persistence and precision contracts remain reconstructible.

## 0.2.12 — 2026-09-21

- Complete the remaining safe Common-to-Calculator reuse identified by the post-1.19.20 forensic audit.
- Use Common range parsers for bounded integer and floating-point inputs in network, date/time, graph, exact-decimal, engineering and number-utility paths.
- Use Common checked signed addition for civil-date offsets so extreme valid int64 input cannot trigger signed overflow.
- Use Common allocation-backed text reads for Linux theme, variables and custom-function persistence.
- Use Common deterministic ASCII case-insensitive comparison for Linux font-family validation.

## 0.2.11 — 2026-09-21

- Move Calculator to the reviewed Common 1.19.20 baseline.
- Replace remaining duplicate complete numeric parsing and checked unsigned multiplication with Common contracts.
- Use Common's deterministic ASCII classification/case-folding for Calculator grammar and Programmer keywords.
- Use Common's canonical theme persistence keys/parser and durable POSIX directory/atomic-file primitives in the Linux and CLI persistence paths.
- Keep Calculator-specific mathematical, parser-grammar, UI-state and platform-policy semantics local.

## 0.2.10 — 2026-09-21

- Complete the final Linux Mint/GNOME Calculator replacement target with integrated arbitrary-precision real/complex Scientific evaluation in the shared C++ core.
- Preserve high-precision Scientific values through Session and Controller state without silent binary64 truncation, including variables, history, Additional Results and native Linux/Windows/iPhone shells.
- Add independent MPFR/MPC oracle coverage for high-precision real and complex arithmetic plus release-CI validation, while keeping those libraries test-only rather than runtime dependencies.
- Preserve Fixed, Scientific and Engineering display preferences on precise Scientific results and document the completed numeric architecture.

## 0.2.9 — 2026-09-21

- Cache expression validity/value once per state mutation so command enablement no longer reparses the same expression for every visible control; Scientific validation now uses the full Session variable/function/assignment context.
- Add side-effect-free Session preview semantics so persisted custom functions and direct assignments are valid before Equals without mutating state during UI enablement checks.
- Make Standard CE cursor-aware for editable expressions, and stop Windows/GTK history surfaces from rebuilding unchanged history on every render by tracking a shared history revision.
- Restrict Win32 command-state updates to the active calculator mode and add regression coverage for custom functions, assignments, cursor-aware CE and history revisions.

## 0.2.8 — 2026-09-21

- Add a first-class Standard-mode CE action that clears only the current operand while preserving the pending expression and resets the entry display to zero.
- Align the six-row Standard keypad with the familiar `% / CE / C / backspace` and `1/x / x² / √ / divide` arrangement on Linux, Windows and iPhone; power remains available in Scientific mode and typed expressions.
- Add regression coverage for ordinary operands, exponent notation, grouped entries, command enablement and iPhone presentation parity.

## 0.2.7 — 2026-09-21

- Replace the stale Windows ICO with the canonical Calculator graphite and #00ADEF artwork so the executable, title bar and taskbar use the same product identity as the maintained Linux application.
- Keep the Windows result display at the shared canonical height and give the large result text and status line separate geometry, preventing the right-aligned result from being clipped by the READY/status area.
- Add regression guards for both the exact Windows icon asset and the unclipped result/status layout contract.

## 0.2.6 — 2026-09-20

- Add GNOME/Mint-compatible Ctrl+N independent Calculator windows without sharing GTK widget/controller state.
- Launch secondary windows as non-unique application processes so each session has independent expression, memory, history and mode state.
- Add source-level regression protection for the independent-window contract.
- Correct the maintained Tools documentation to the current 0.2.x release line.
- Reduce the explicit GNOME Calculator 41.1 parity backlog to live currency conversion and an integrated arbitrary-precision real/complex Scientific domain.

## 0.2.5 — 2026-09-20

- Complete the Linux Mint/GNOME 41.1 ordinary unit-conversion catalogue gap with decimal and IEC digital-storage units, metric cups and microlitres.
- Add native Linux mode/workbench, quit/close and discoverable shortcut-key parity learned from the GNOME 41.1 source.
- Add regression coverage for digital-storage conversions and the Linux shortcut contract.
- Re-audit replacement parity against the exact GNOME Calculator 41.1 source and explicitly retain currency, integrated arbitrary-precision real/complex Scientific arithmetic and independent multi-window sessions as remaining engineering work.

## 0.2.4 — 2026-09-20

- Persist user-defined variables under the Calculator XDG data directory with transactional validation and round-trip-safe binary64 text.
- Restore Scientific angle units and Programmer radix, word width and signed-display state across Linux launches.
- Add shared Session/Controller persistence contracts and regression tests instead of implementing semantic state in the GTK shell.
- Close two further GNOME Calculator 41.1 replacement gaps discovered by reading its settings and MathVariables source.

## 0.2.3 — 2026-09-20

- Pin replacement validation to Linux Mint 22.3's actual `gnome-calculator 1:41.1+mint1+wilma` baseline and upstream GNOME 41.1 source revision.
- Add a first-class GTK unit converter with dimension/source/target selectors, live conversion and one-click swapping.
- Persist the selected conversion pair under Calculator's XDG configuration directory.
- Expose shared unit catalogue metadata so the GTK shell cannot drift from the C++ conversion engine.
- Add core and Linux regression protection for the new converter workflow.

## 0.2.2 — 2026-09-20

- Fix the Linux Advanced Tools title-bar close action so it closes only the Tools workbench and never terminates the Calculator application.
- Give the GTK Tools workbench explicit secondary-window ownership, reuse it when reopened and destroy it with the main Calculator window.
- Extend mature GNOME Calculator parity work with broader expression/unit coverage and keyboard expression handling already present on current main.
- Add a cross-platform source regression guard for the Linux Tools window lifecycle while retaining the already-correct Win32 close behaviour.

## 0.2.1 — 2026-09-20

- Reset Scientific F-E notation on Clear and protect the behaviour with shared-controller regression coverage.
- Canonically reduce exact rational results with arbitrary-size GCD/exact division and emit exact decimals for all terminating rationals, not only power-of-ten denominators.
- Extend the 256-bit MPFR oracle across denormal, extreme logarithmic/inverse-hyperbolic and near underflow/overflow boundaries.
- Add a generous 5 ms average controller-keystroke CI budget to detect major interaction-performance regressions.
- Preserve the corrected Linux desktop identity introduced after 0.2.0.

## 0.2.0 — 2026-09-20

- Complete the twelve previously planned capability families in one shared Advanced Tools engine.
- Add engineering/electrical, dimensional unit, ICT/network, storage/filesystem and civil date/time calculators.
- Add a broader mathematical/physical constant catalogue and make its ASCII names available in Scientific expressions.
- Add descriptive statistics, shared graph sampling with native GTK/Win32/SwiftUI graph rendering, and deterministic real equation solving.
- Add exact decimal/rational arithmetic that never passes decimal literals through binary64.
- Add bounded arbitrary-precision signed integer arithmetic with powers and factorial.
- Add explicit complex-number arithmetic, magnitude/phase and conjugation.
- Add native Tools workbenches on Linux, Windows and iPhone over the same shared catalogue/evaluator.
- Add cross-platform ownership regressions and portable tests spanning every advanced tool family.
- Add maintained Tools, architecture, numerical, parity and validation documentation.

## 0.1.43 — 2026-09-20

- Harden engineering notation so the smallest binary64 subnormals never pass through an underflowing decimal scale.
- Derive engineering mantissas from one rounded scientific representation so carry across a 10^3 boundary is handled before presentation.
- Add core and shared Additional Results regressions for negative values, subnormal values, the largest finite binary64 value and exponent-boundary rounding.
- Expand the independent 256-bit MPFR oracle to cover hyperbolic/inverse-hyperbolic functions plus base-2 and base-10 exponentials.
- Tighten MPFR comparison tolerance so very small expected values are not implicitly compared against a unit-scale error budget.

## 0.1.42 — 2026-09-20

- Add a shared Additional Results model above the calculation engines so platform shells render representations instead of deriving their own mathematics.
- Add deterministic engineering notation alongside canonical decimal and scientific notation for Standard and Scientific results.
- Reuse the same Additional Results model for Programmer HEX/DEC/OCT/BIN representations rather than maintaining a separate representation path.
- Add native Additional Results surfaces on GTK, Win32 and iPhone while preserving the compact Standard window and the existing Programmer Bases interaction.
- Keep iPhone and GTK Additional Results selectable for copy workflows and preserve the Windows click-to-copy primary result behaviour.
- Update active architecture/numerics/design documentation to the actual Common 1.19.10 baseline while retaining historical Common 1.19.8 decisions as history.
- Extend core, controller, cross-platform and native Windows smoke regressions for the new representations and mode-specific Results/Bases visibility contract.

## 0.1.41 — 2026-09-20

- Release the exact Common 1.19.10 dependency already reviewed on main and consume its expanded semantic appearance roles rather than flattening them back to older generic colours.
- Use Common heading, summary, kicker, detail/note, status-border, accent-hover, selected-summary and warning roles where they match Calculator semantics on GTK, Win32 and SwiftUI.
- Use Common's explicit titlebar, heading and status-border roles for native Windows non-client rendering.
- Make the native Windows result a direct click-to-copy surface while Linux and iPhone retain selectable result text, without introducing a second result model.
- Replace the native Windows dotted focus rectangle with a DPI-aware two-pixel semantic accent focus ring and add matching GTK focus treatment.
- Strengthen iPhone calculator-key accessibility with explicit labels/selection state while preserving native SwiftUI controls.
- Add an opt-in MPFR 256-bit CI oracle and enable it in the Linux release build to compare transcendental Calculator results against an independent high-precision reference.
- Standardise the Linux application artwork on the non-automotive Infiltrator family: graphite tile and canonical `#00ADEF` linework instead of the older lighter cyan.
- Extend Common/design and cross-platform regressions so future releases cannot silently drop the 1.19.10 semantic palette or the new copy/focus contracts.

## 0.1.40 — 2026-09-20

- Add one shared Programmer representation contract that renders the current fixed-width bit pattern simultaneously as BIN, OCT, DEC and HEX.
- Preserve signed interpretation for the decimal representation while keeping binary/octal/hexadecimal as exact masked bit-pattern views.
- Add an on-demand Programmer Representations surface on Linux, Windows and iPhone without increasing Standard or Programmer main-window geometry.
- Group binary output by nibbles for readability while retaining the exact full-width representation underneath.
- Make Linux and iPhone result text directly selectable for copy workflows and expose selectable representations on iPhone.
- Add regression coverage for representation semantics and cross-platform presentation wiring.

## 0.1.39 — 2026-09-20

- Replace presentation-only history with structured history entries carrying mode, exact rendered result and Programmer radix/width/signed context.
- Record Programmer calculations and errors in the same bounded history model without converting 64-bit results through binary64.
- Add history recall through the shared Controller so replay restores the originating calculator mode and exact Programmer context.
- Make Linux history rows directly recallable, add native Win32 list selection/double-click recall, and expose structured recall in the iPhone history sheet.
- Keep wide desktop history docking as a read-only summary while the dedicated history surface owns explicit recall interaction.
- Add regression coverage for history ordering, bounds, exact Programmer context and cross-platform replay wiring.

## 0.1.38 — 2026-09-20

- Expand Scientific mode without increasing its keypad row count: add 2nd and HYP layers, DEG/RAD/GRAD cycling and F-E scientific-notation display switching while preserving the existing direct constants/operators.
- Add inverse and hyperbolic trig command routing through the shared C++ core, plus square/cube, floor/ceiling and base-2/base-10 exponential transforms exposed through the 2nd layer.
- Extend the expression engine with asinh/acosh/atanh, square/cube, exp2/exp10, floor and ceil functions and add gradian-aware trigonometric conversion.
- Extend Programmer mode with fixed-width ROL/ROR and NAND/NOR operators while preserving wrapping, radix and width semantics.
- Add an explicit MS memory-store command alongside MC/MR/M+/M− on every platform.
- Keep Standard at its compact 480-unit geometry; extended controls remain shared through the controller/UI contract rather than platform-specific implementations.
- Release the previously unpublished copyright-normalisation commits together with the functional changes so main and the immutable release are aligned.

## 0.1.37 — 2026-09-19

- Advance the exact shared foundation from Common 1.19.7 to immutable Common 1.19.8.
- Retain the existing public Common API boundary because 1.19.8 is an ABI-compatible internal-consolidation release rather than an API expansion.
- Re-run the Calculator/Common ownership audit and confirm no private Common implementation headers or Calculator-specific semantics should be pulled across the boundary.
- Strengthen the Common integration regression so Calculator is pinned to the exact 1.19.8 release commit and consumes only published Common interfaces.
- Preserve all Standard, Scientific, Programmer, history, typography, platform and packaging behaviour while rebuilding every release target against Common 1.19.8.

## 0.1.36 — 2026-09-19

- Advance Calculator to immutable Common 1.19.7 and replace its private decimal-token scanner with Common's exact locale-independent token parser.
- Consume Common's native typography identity, structural design metrics and MB Corpo asset provenance instead of maintaining desktop copies.
- Centralise real unary/scientific transforms in the Calculator core so parser and controller no longer implement the same mathematics separately.
- Centralise bounded history text rendering in Session/Controller and remove platform-shell reach-through into mutable session state.
- Preserve the deliberate C/C++ split: Common remains C11 portable infrastructure, Calculator domain/state remains C++17, and native shells retain only platform adaptation.

## 0.1.35 — 2026-09-19

- Corrected the 0.1.34 Standard-mode sizing regression: Standard is compact again instead of stretching its keypad to fill an oversized window.
- Moved the 480-unit Standard preferred/minimum height into the shared desktop contract, with Scientific and Programmer retaining the 610-unit preferred / 520-unit minimum extended geometry.
- Made GTK and Win32 consume the same mode-aware height contract and added regression coverage preventing a return to platform-local or stretched Standard geometry.

## 0.1.34 — 2026-09-19

- Removed the dead vertical strip below the Linux Standard keypad by allowing the Standard panel, grid and keypad rows to consume the window's shared 520px allocation.
- Kept the shared desktop geometry contract intact rather than reintroducing a Linux-only undersized window exception.
- Added a regression guard for the Standard vertical-fill contract.

## 0.1.33 — 2026-09-19

- Replaced the Linux Calculator artwork with the shared non-automotive Infiltrator icon language: dark graphite field, #72dcff cyan linework and a simplified calculator glyph.
- Kept the same project-owned SVG wired through the desktop launcher and Linux Mint Software Manager package alias.

## 0.1.32 — 2026-09-19

- Bound Programmer-mode recursive parsing so excessively nested parentheses or unary operators fail deterministically with `expression nesting too deep` instead of risking native stack exhaustion.
- Added permanent Programmer regression coverage for deep parenthesis and unary chains.

## Recording policy

Record additions, removals, behavioural fixes, compatibility changes, dependency changes that affect consumers, and material validation/release changes. Pure refactoring needs an entry only when it changes maintenance, numerical, portability or compatibility expectations.

## Historical releases

Existing Git tags and GitHub Releases remain the authoritative identity for exact historical source and release assets. Do not reconstruct detailed historical claims here without evidence from those immutable records.
