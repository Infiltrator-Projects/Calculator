# Validation

## Purpose

Validation distinguishes implemented behaviour from behaviour that has actually been demonstrated. Compilation, deterministic numerical tests, sanitizer runs, native runtime checks, Simulator/device-target builds, package inspection and physical-device observation prove different things.

## Automated evidence

The repository uses one cross-platform workflow, `.github/workflows/release.yml`. The workflow orchestrates rather than reimplements release mechanics. Focused helpers under `support/ci/` and `support/release/` own Debian inspection, Linux idle-CPU measurement, Windows runtime/dependency smoke testing, deterministic source packaging, native-installer assembly, release-asset/checksum generation and Package Repository publication checks.

`VERSION` is the sole source-version authority. Preflight rejects disagreement with the newest Debian changelog, human changelog or AppStream release before expensive platform work begins. A forward VERSION transition, not commit-message wording, is what creates release intent.

The `tests/` tree protects expression grammar and nesting, Scientific precision and completion, canonical constant ownership, multiprecision conversion/storage, versioned Controller persistence with transactional malformed-state rejection, calendar arithmetic, graph/root evaluation, Standard precedence/percentage and exact binary64 state round-tripping, memory invariants, Programmer width/radix/logic/shift/modulo behaviour, structured history, Additional Results, engineering notation, controller ownership, desktop contracts, Common integration and source/build ownership.

`calculator-evaluation-budget` additionally protects Scientific/Advanced Tools input, cumulative-work, recursion, factorial/exponent, cancellation and hard arbitrary-precision output ceilings. Exact/arbitrary parser recursion is exercised directly so an 8192-byte input ceiling cannot be mistaken for a stack-safety policy.

The dedicated `calculator-standard-math` forensic suite enumerates two-operator `+ - * /` precedence combinations over signed operands, explicit grouping, power/unary/postfix/domain boundaries, and computational-state round trips for curated IEEE-754 boundaries plus 50,000 deterministic finite binary64 patterns. Controller regressions prove fixed-decimal/grouped presentation cannot alter later arithmetic or memory recall and that repeated Equals captures the intended top-level operation.

Pull requests, every `main` push and manual dispatch run the same platform qualification jobs. A manual `publish` dispatch is recovery only. If `vVERSION` already exists, preflight requires the checked-out commit to be that exact tag target. If publication failed before a tag existed, manual recovery is permitted only at the commit whose parent carries the previous version. Later same-version commits therefore cannot be used to replace bytes behind an immutable version identity.

Verification:

- builds/tests the shared core on supported hosts;
- runs portable core/controller tests under Clang AddressSanitizer and UndefinedBehaviorSanitizer;
- builds the native Windows application and exercises native runtime/keypad/dependency smoke paths;
- builds the GTK/Linux target and Debian package, then applies packaged idle-CPU checks;
- enables Linux-only MPFR/MPC oracle tests for independent high-precision real/complex reference evidence;
- builds the iPhone Simulator target and validates AppIcon metadata;
- compiles and packages an unsigned ARM64 iPhoneOS IPA with bundle-identifier/structure checks;
- verifies Common version/gitlink/public API ownership and derives the expected iOS Common Portable source subset from Common's own CMake;
- verifies Calculator's iOS source ownership is directory-based rather than a duplicated per-file build manifest;
- constructs a deterministic source bundle containing the exact Common checkout and a hardware-native Linux installer from that source payload;
- derives one canonical release-asset list plus SHA-256 manifest; and
- verifies central Package Repository publication for the Debian package.

GitHub Actions dependencies are pinned to immutable commit revisions and updated through controlled Dependabot proposals. Workflow permissions are read-only by default; only publication receives repository-content write permission.

## What platform evidence proves

A successful iOS Simulator build proves Simulator-target compilation/integration. An unsigned iPhoneOS build proves device-architecture compilation and IPA structure. Neither proves signing, provisioning, physical-device installation, hardware behaviour or App Store acceptance.

The Windows runtime smoke path demonstrates selected native window/control interactions in the CI environment; it does not exhaustively prove visual behaviour at every DPI/theme configuration.

Linux build/tests and layout contracts demonstrate compiled GTK behaviour and deterministic layout rules. They do not replace observation of real compositor/font/theme/allocation behaviour. Reproducible native-layout defects should gain permanent regression protection where practical.

## Numerical oracle

Release CI enables `CALCULATOR_ENABLE_MPFR_ORACLE_TESTS`. Standard arithmetic is cross-checked against high-precision reference arithmetic rounded back to binary64. Scientific has independent MPFR/MPC evidence: focused precision checks plus 8192-bit reference coverage over deterministic complex fuzzing, Scientific function families including reciprocal/inverse trig/hyperbolic branches, DEG/RAD/GRAD conversion, constants and requested precision sweeps from 16 through 1000 digits.

Production remains Boost.Multiprecision; MPFR/MPC are validation-only dependencies, so the production engine is not its own only oracle and the oracle does not become a second shipped calculation engine.

## Advanced Tools validation

`calculator-advanced-tools-test` exercises every released family: engineering, dimensional/temperature conversion, IPv4/CIDR, RAID/allocation, civil dates, constants, statistics, graphing, real-root solving, exact rational arithmetic, arbitrary-precision powers/factorial, complex operations, financial operations and number utilities. Values beyond binary64 range prove the relevant complex/root paths remain on shared Scientific multiprecision semantics.

`calculator-evaluation-budget` verifies cumulative resource behaviour across Scientific and Advanced Tools. The exact/integer implementation uses Boost `cpp_int`; tests protect Calculator's grammar, operation bounds, parser recursion and product ceiling rather than coupling to Boost internals.

## Semantic appearance and focus regression

The exact pinned Common 1.19.35 public contract is checked for semantic design roles consumed by Calculator. GTK, Win32 and SwiftUI source-contract tests protect use of relevant roles, while desktop focus treatment is checked so keyboard-visible focus cannot disappear during visual refactoring.

## Release criterion

The exact source revision intended for release must pass all applicable automated gates. Published assets must derive from that revision, checksums must cover every published project payload, the source bundle must contain the exact Common dependency revision, and redundant release metadata must agree with VERSION.

A normal release is created only by a forward VERSION transition on `main`. Manual publication may recover/reconcile only the same source identity as described above. It is not a mechanism for rebuilding an old version from newer same-version source. Published tags/releases are immutable identities even when a recovery action uses `--clobber`: reconciliation is permitted only because preflight has already proved the source commit is identical to the tag source.

Documentation must not advertise removed, known-failing or merely planned behaviour as supported.

## Manual/environment-dependent evidence

Physical-device installation, signing/provisioning, accessibility, visual layout across real DPI/theme/font combinations and interactions hosted CI cannot faithfully reproduce require explicit native-environment observation. Manual evidence supplements automation and should record what environment and behaviour were actually observed.

## Regression rule

Every reproducible defect should gain the narrowest useful permanent regression where practical. Tests protect behaviour/boundaries/invariants, not incidental implementation shape unless the shape itself is an ownership or build contract.

## Limits

Green CI is necessary, not sufficient. It cannot establish correctness for every mathematical input, every native UI environment or unsigned iPhone installation. The forensic review therefore also checks architecture, ownership, failure semantics, build/release identity and maintained documentation for contradictions that compilation alone cannot reveal.
