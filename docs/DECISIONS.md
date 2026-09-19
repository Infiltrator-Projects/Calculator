# Decisions

This file records durable architectural choices for Calculator.

## ADR-001 — One calculator application, multiple selectable modes

**Decision.** Standard, Scientific and Programmer are modes of one product rather than separate applications.

**Rationale.** They share session, history, variables and command-state concepts, and users should move between them without switching products.

**Consequence.** Mode-specific engines live under one application/session architecture and share release identity.

## ADR-002 — Calculation semantics are platform-neutral

**Decision.** Arithmetic, expressions, session state and Programmer logic live in the shared C++ core.

**Rationale.** Linux, Windows and iPhone should not calculate the same input differently because each shell reimplemented mathematics.

**Consequence.** Platform shells translate controls to shared commands/state rather than implementing calculation rules.

## ADR-003 — Desktop interaction is shared; rendering remains native

**Decision.** Linux and Windows share the desktop layout/command/controller contract while GTK4 and Win32 remain native shells.

**Rationale.** Behavioural parity does not require a cross-platform widget toolkit.

**Consequence.** Toolkits own widgets, DPI and window integration; Calculator owns control ordering, enabled state and command meaning.

## ADR-004 — iPhone stays native SwiftUI over the same core

**Decision.** iPhone uses SwiftUI with an Objective-C++ bridge to the C++ calculation engine.

**Rationale.** Touch layout should follow iPhone conventions while mathematical semantics remain common.

**Consequence.** iPhone may have different physical layout metrics without becoming a separate calculator implementation.

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

**Consequence.** Parity tests focus on commands, state and layout contracts rather than byte-identical UI implementations.
