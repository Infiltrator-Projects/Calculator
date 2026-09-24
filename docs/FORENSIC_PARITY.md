<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Forensic calculator parity ledger

This document prevents external-comparison reviews from repeatedly presenting
the same capability as new work. It is a classification baseline, not a claim
that another calculator is a specification for this project.

Before proposing a Microsoft Calculator, GNOME/Debian Calculator, KDE Kalk,
Qalculate/libqalculate, SpeedCrunch, Numbat or other calculator finding, re-read
the current Calculator source and tests, then classify the finding below.
Only a genuinely new, still-applicable difference should become a fresh action.

## Implemented — do not re-flag as missing

The current Calculator already has conventional operator precedence; Standard
CE and C separation; repeated Equals; structured recallable history; configurable
history retention; a newest-first multi-memory stack; Scientific history
angle/precision context; arbitrary-precision real/complex Scientific evaluation;
16–1000 digit precision; DEG/RAD/GRAD; sec/csc/cot and inverse/hyperbolic
families; live Scientific preview and completion; user variables/functions;
exact decimal/rational tools; arbitrary-precision integer tools; graphing and
real-root solving; statistics; financial calculations; date arithmetic; broad
unit conversion; GCD/LCM/permutation/combination/factorisation; Programmer
BIN/OCT/DEC/HEX views, 8/16/32/64-bit widths, grouping, AND/OR/XOR/NAND/NOR/XNOR,
logical shifts, explicit arithmetic-right shift, rotations, modulo, bit editing
and byte-order reversal; cross-platform native Linux/Windows/iPhone shells;
keystroke-latency tests; Linux packaged idle-CPU tests; sanitizer builds; and
independent MPFR/MPC numerical oracles.

Advanced Tools Complex arithmetic and arbitrary roots reuse the Scientific
multiprecision engine. They are not separate binary64 implementations.

## Intentional differences — do not report as defects without new evidence

Standard mode remains a smaller binary64 arithmetic domain while Scientific is
the arbitrary-precision real/complex domain. This separation is deliberate and
documented.

Currency conversion is currently an explicit non-goal. A competitor offering
live currency does not by itself create a Calculator defect.

Platform shells are native rather than pixel-identical. Equivalent semantics
are required; identical toolkit implementation is not.

## Known deferred candidates — not new discoveries

These items are known comparison opportunities and must not be presented as
fresh forensic findings unless new evidence changes their priority or design:

- sexagesimal degree-minute-second input/output;
- locale-native digit and decimal-separator input canonicalisation;
- parser source spans/highlighting for the exact failing input token;
- persistence of actual history entries and memory slots, rather than only their
  current policy/semantic settings;
- expression undo/redo and selective insertion from a prior history item;
- richer screen-reader result/status announcements beyond native control
  accessibility;
- vector/matrix algebra;
- uncertainty propagation and interval arithmetic;
- first-class dimensional quantities inside the Scientific expression grammar;
- safe decomposition of the large native GTK and Win32 shell translation units.

These are candidates, not promises. Each still requires a clear ownership,
numeric-domain, user-interaction and validation design before implementation.

## Audit rule

A forensic comparison must use this order:

1. establish the exact current Calculator main revision;
2. verify the candidate against current source and tests;
3. check this ledger and maintained roadmap/decisions;
4. classify it as implemented, intentional, known/deferred or genuinely new;
5. only then recommend or implement work.

If a previously deferred item is implemented, move it to the implemented
section in the same change. If an intentional difference changes, record the
new rationale in the appropriate ADR or numerical/design document.
