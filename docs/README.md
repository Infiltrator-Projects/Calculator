# Documentation

This directory is the canonical documentation entry point for Calculator. The Infiltrator project family uses the same baseline document roles across repositories so readers can move between projects without relearning the structure.

## Canonical baseline

- [Architecture](ARCHITECTURE.md) — ownership, layers, dependencies and system boundaries.
- [Design](DESIGN.md) — first-principles goals, non-goals, trade-offs and failure philosophy.
- [Decisions](DECISIONS.md) — durable architectural decisions and consequences.
- [Roadmap](ROADMAP.md) — current foundation, near-term priorities and longer-term direction.
- [Validation](VALIDATION.md) — automated, manual and environment-specific evidence boundaries.
- [Project README](../README.md) — product overview, current capabilities, build/use entry point and engineering ethos.
- [Changelog](../CHANGELOG.md) — user-visible and contract-relevant change history.
- [Contributing](../CONTRIBUTING.md) — development, ownership and verification rules.
- [Security](../SECURITY.md) — vulnerability scope, trust boundaries, reporting and response policy.

## Documentation authority

The baseline files have distinct responsibilities and should not compete as alternate sources of truth. Architecture describes where behaviour belongs; Design explains why; Decisions preserve durable choices; Roadmap describes direction; Validation states what evidence is required.

Code and tests remain authoritative for executable behaviour. Immutable tags/releases identify historical source and published assets. Specialist documents may go deeper into one domain, but should not recreate release history or duplicate baseline ownership.

## Specialist documentation

- [Numerical semantics](NUMERICS.md) — evidence hierarchy, numeric representations, expression/immediate/Programmer semantics, display precision and numerical failure contracts.
- [Portability](PORTABILITY.md) — language, platform, representation, locale, bridge, ABI and compatibility boundaries.
- [UI parity](UI_PARITY.md) — shared controller ownership, desktop-layout responsibilities and native-shell parity boundaries.

## Maintenance rule

When a change moves an ownership boundary, support boundary, numerical contract, compatibility identity, validation claim or major design decision, update the corresponding canonical document in the same change.

Avoid copying the same capability/status statement into several files. Historical release detail belongs in Git tags/releases; implementation commentary belongs near the owning code/tests unless it establishes a maintained cross-cutting contract.


- [TOOLS.md](TOOLS.md) — user-facing Advanced Tools catalogue, command forms, examples, numeric domains and limits.
