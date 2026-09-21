# Decisions

This file records durable architectural choices for Calculator. `docs/DESIGN.md` explains project-wide rationale; this file preserves choices that future work should not casually reverse.

## ADR-001 — One calculator application, multiple selectable modes

**Decision.** Standard, Scientific and Programmer are modes of one product rather than separate applications.

**Rationale.** They share session, history, variables and command-state concepts, and users should move between them without switching products.

**Consequence.** Mode-specific engines live under one application/session architecture and share release identity.

## ADR-002 — Calculation semantics are platform-neutral

**Decision.** Arithmetic, expressions, session state and Programmer logic live in the shared C++ core.

**Rationale.** Linux, Windows and iPhone should not calculate the same input differently because each shell reimplemented mathematics.

**Consequence.** Platform shells translate controls to shared commands/state rather than implementing calculation rules.

## ADR-003 — Interaction state is shared; rendering remains native

**Decision.** Linux, Windows and iPhone share the Calculator command/controller state machine. Linux and Windows additionally share desktop layout metrics while GTK4, Win32 and SwiftUI remain native shells.

**Rationale.** Behavioural parity does not require a cross-platform widget toolkit, but calculator semantics should still have one owner.

**Consequence.** Toolkits own widgets, DPI and window integration; Calculator owns control ordering where applicable, enabled state, mode state and command meaning.

## ADR-004 — iPhone stays native SwiftUI over the shared controller

**Decision.** iPhone uses SwiftUI with an Objective-C++ bridge to the same C++ controller used by the desktop shells.

**Rationale.** Touch layout should follow iPhone conventions while calculator interaction and mathematical semantics remain common.

**Consequence.** iPhone may have different physical layout metrics without becoming a separate calculator implementation or a second state machine.

## ADR-005 — Common owns product-family theme semantics

**Decision.** System/Day/Night semantic palette values come from pinned Common.

**Rationale.** Shared appearance contracts should not drift into separate Linux, Windows and Swift colour definitions.

**Consequence.** Platform shells own theme detection/application but not Day/Night palette meaning.

## ADR-006 — Numerical edge behaviour is part of feature completion

**Decision.** Invalid domains, integer width/radix rules, overflow behaviour, contextual percentages and disabled-state rules are product contracts.

**Rationale.** Calculator defects usually appear at boundaries rather than in simple arithmetic.

**Consequence.** New mathematical features require deterministic edge-case regression tests as well as UI exposure.

## ADR-007 — Platform parity means equivalent capability, not identical toolkit code

**Decision.** Shared behaviour should be equivalent across supported platforms while genuinely platform-specific interaction stays native.

**Rationale.** Forcing one toolkit abstraction over every platform would reduce native quality without improving mathematical consistency.

**Consequence.** Parity tests focus on commands, state and ownership contracts rather than byte-identical UI implementations.

## ADR-008 — Compatibility identifiers survive user-facing renames

**Decision.** Stable runtime/configuration identifiers remain when changing them would break settings, application identity or installed upgrade continuity.

**Rationale.** Product naming and compatibility identity solve different problems. A cleaner internal name is not an improvement if it needlessly strands user state or installed packages.

**Consequence.** The visible product, repository and release assets are Calculator, while documented legacy executable/configuration or bundle identifiers may remain compatibility-only until an explicit migration replaces them safely.

## ADR-009 — Numeric domains are explicit rather than hidden behind one representation

**Status.** Superseded in representation detail by ADR-014; the domain-separation principle remains active.

**Decision.** At the time of this decision Standard/Scientific used binary64 real values, while Programmer mode used explicit fixed-width integer bit patterns. Exact-decimal, arbitrary-precision or complex capabilities were required to introduce an appropriate domain instead of pretending binary64 had semantics it did not.

**Rationale.** Representation is part of numerical correctness. Formatting cannot turn an inexact representation into an exact domain.

**Consequence.** New mathematical capability must state its representation, error model and validation strategy before it is treated as production behaviour.


## ADR-010 — Share mechanics at the narrowest correct layer

**Decision.** Product-neutral mechanics are consumed from pinned Common, while duplicate Calculator-domain semantics are consolidated inside the Calculator C++ core rather than promoted merely because two internal callers exist.

**Rationale.** Common supplies product-neutral parsing, checked arithmetic, deterministic ASCII, native design metrics, typography identity, asset provenance and platform infrastructure where its public contract is at least as strong as Calculator's need. Calculator mathematics and history/session presentation remain Calculator semantics and therefore have one Calculator-owned implementation.

**Consequence.** The C/C++ split remains intentional: Common stays C11 infrastructure; Calculator state, parsers and domain logic stay C++17; native shells remain procedural adapters without acquiring mathematical/session ownership.


## ADR-011 — Common 1.19.8 is consumed through the existing public boundary

**Status.** Historical; superseded by ADR-015.

**Decision.** Calculator pins immutable Common 1.19.8 at commit `3bfcb6f76ca44ac33bc2fee54fb114caa0eca5f9` and continues to use only published Common APIs.

**Rationale.** Common 1.19.8 removes duplicated implementation mechanics inside Common without changing its public ABI. Calculator already consumes the relevant product-neutral contracts: decimal-token parsing, theme/palette, structural metrics, typography identity and font-asset provenance. Reaching into Common's new private ASCII or POSIX helpers would make the dependency less stable, not more complete.

**Consequence.** Calculator receives the 1.19.8 implementation improvements everywhere Common is built, while Standard/Scientific/Programmer semantics, session state, history policy, controller behaviour and native platform adaptation remain in their correct Calculator-owned layers.


## ADR-012 — Common 1.19.10 semantic roles are consumed selectively

**Status.** Historical; superseded by ADR-015 while its selective-semantic-reuse rule remains active.

**Decision.** Calculator pins immutable Common 1.19.10 at commit `33e69c0a462b56d388881d89c4eb49f72fa0b0fe` and consumes its expanded public semantic appearance roles where their meanings match Calculator.

**Rationale.** Common 1.19.10 adds richer product-family semantics without changing ownership of Calculator arithmetic or interaction rules. Mechanical consumption of unrelated roles would be coupling rather than reuse.

**Consequence.** Heading, summary, kicker, detail/note, status, warning, accent and titlebar semantics are shared. Connection-specific roles remain unused because Calculator has no connection domain.

## ADR-013 — Additional Results is a shared representation model

**Decision.** Secondary representations are generated by the shared Calculator Controller and shown on demand by native platform shells.

**Rationale.** Derived display forms should not enlarge the primary keypad or cause GTK, Win32 and SwiftUI to independently re-evaluate expressions. A shared representation model also creates the correct extension point for future unit, symbolic or other representations without coupling platform shells to calculation engines.

**Consequence.** Standard/Scientific currently provide decimal, scientific and engineering representations; Programmer provides HEX/DEC/OCT/BIN. Platform shells own presentation and copy interaction only.


## ADR-014 — Scientific owns an arbitrary-precision real/complex domain

**Decision.** Standard remains an IEEE-754 binary64 immediate-calculator domain. Scientific uses Calculator's Boost.Multiprecision real/complex domain at 50 decimal digits by default with a maintained ceiling of 1000 digits. Programmer remains an explicit fixed-width integer domain, while exact decimal/rational and arbitrary-precision integer tools retain their own representations.

**Rationale.** Representation determines what correctness claims are possible. Scientific expressions need precision and complex-domain behaviour that binary64 cannot provide without silent loss, while Standard's interaction contract does not benefit from importing that complexity.

**Consequence.** Scientific decimal literals and intermediate operations remain in the multiprecision backend. The Session/Controller boundary transports real and imaginary components as decimal text, preserving precision without exposing the third-party multiprecision ABI to platform shells.

## ADR-015 — Common 1.19.20 is the current public shared-foundation boundary

**Decision.** Calculator pins immutable Common 1.19.20 at commit `336ab8f7f8b7364b6296c7560fc67b242b27eb9d` and consumes only published Common interfaces whose contracts are at least as strong as the Calculator behaviour they replace.

**Rationale.** Common 1.19.20 provides product-neutral numeric parsing and ranges, deterministic ASCII handling, checked arithmetic, theme/palette/metrics/typography contracts and POSIX persistence primitives. Keeping those mechanics shared reduces duplicated infrastructure without moving Calculator-specific grammar, numeric-domain policy or interaction semantics into Common.

**Consequence.** Calculator's portable core remains linked to `InfiltratrCommon::Portable`; POSIX helpers are confined to Linux/CLI application layers. Historical ADR-011 and ADR-012 remain useful provenance but no longer describe the active dependency baseline.
