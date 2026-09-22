# Validation

## Purpose

Validation distinguishes implemented behaviour from behaviour that has actually been demonstrated. Compilation, deterministic numerical tests, native runtime checks, Simulator builds and physical-device observation prove different things.

## Automated evidence

The repository currently uses:

- `.github/workflows/release.yml`

The `tests/` tree covers expression grammar and nesting limits, Standard precedence and percentage semantics, exact binary64 state serialization, display/state isolation, memory range invariants, gradian/inverse/hyperbolic Scientific transforms and F-E formatting, Programmer width/radix/ROL/ROR/NAND/NOR behaviour and simultaneous multi-radix representations, explicit memory store/update state, structured history ordering/context/recall, Additional Results and engineering-notation representation, shared controller behaviour, desktop UI contracts and source-level cross-platform ownership rules.

The dedicated `calculator-standard-math` forensic suite exhaustively enumerates all two-operator `+ - * /` precedence combinations over a signed operand set, checks explicit left/right grouping, verifies power/unary/postfix/domain boundaries, and proves decimal computational-state round-tripping for curated IEEE-754 boundaries plus 50,000 deterministic finite binary64 bit patterns. Controller regressions additionally prove that fixed-decimal and thousands-grouping presentation cannot alter subsequent unary arithmetic or memory recall.

Every `main` push and manual workflow dispatch runs the cross-platform verification jobs. Release publication remains gated to explicit `Release ...` commits. Verification:

- builds and tests the shared core on supported build hosts;
- runs the portable Calculator core/controller tests under Clang AddressSanitizer and UndefinedBehaviorSanitizer;
- builds the native Windows application and runs a Win32 runtime/keypad smoke path;
- builds the GTK/Linux target and Debian package;
- enables Linux-only MPFR and MPC oracle tests that independently compare representative high-precision real and complex Scientific results without adding MPFR/MPC to Calculator's runtime;
- compiles the iPhone application for iOS Simulator through the shared C++ controller and verifies AppIcon metadata;
- compiles the unsigned ARM64 iPhoneOS target;
- verifies expected binary assets plus a deterministic source bundle containing the exact Common checkout, and checksums all published payloads; and
- verifies Package Repository publication for the Debian package; and
- enforces a deliberately loose 5 ms average shared-controller keystroke budget so order-of-magnitude input-path regressions are caught without turning CI scheduling noise into failures.

Automated checks should cover ordinary behaviour, boundary/error cases and release/package contracts appropriate to the affected domain.

## What platform evidence proves

A successful iOS Simulator build proves that the Swift/Objective-C++/C++ application compiles for the Simulator environment. It is not evidence of physical-iPhone installation, signing, provisioning, hardware behaviour or App Store acceptance.

A successful unsigned iPhoneOS build proves device-architecture compilation. It is not an installable signed release and must not be described as one.

The Windows runtime smoke path demonstrates selected native window/control interactions on the CI environment; it does not exhaustively prove visual behaviour at every DPI, theme or desktop configuration.

Linux build/tests and layout contracts demonstrate compiled GTK behaviour and deterministic layout rules. They do not replace observation of real compositor, font, theme and allocation behaviour. Reproducible GTK layout defects should gain permanent regression protection where practical.

## Manual and environment-dependent evidence

Physical-device installation, signing/provisioning, platform accessibility, visual layout across real DPI/theme/font combinations and interaction behaviour that hosted CI cannot faithfully reproduce require explicit native-environment testing.

Manual evidence supplements automation and should record the platform, environment and behaviour actually observed. Simulator, mocked, fixture or compile-only evidence must not be promoted into stronger claims.

## Release criterion

The exact revision intended for release must pass the required automated gates. Published assets must derive from that revision, checksums must cover every published project payload, and the project source bundle must contain the exact Common dependency used for the build. Documentation must not advertise known-failing, removed or merely planned behaviour as supported.

## Regression rule

Every reproducible defect should gain the narrowest useful permanent regression when practical. Tests are part of the product contract rather than disposable scaffolding.

## Limits

The validation system is evidence, not a proof of all possible numerical or platform behaviour. It does not establish correctness for untested mathematical domains, atypical floating-point implementations, every native UI environment or unsigned iPhone installation.


## Independent numerical oracle

Release CI enables CALCULATOR_ENABLE_MPFR_ORACLE_TESTS. The MPFR oracle independently cross-checks Standard binary `+ - * /` over representative extreme/subnormal/ordinary operands against 256-bit reference arithmetic rounded back to binary64, verifies non-finite rejection, and compares Standard powers against the same high-precision reference. The binary64 transcendental oracle continues to protect deliberate Standard/supporting paths. Scientific now has two independent layers: `calculator-precision-oracle` retains focused MPFR/MPC high-precision checks, while `calculator-scientific-oracle` uses 8192-bit MPFR/MPC references for deterministic complex arithmetic fuzzing, every Scientific function family, principal-branch/branch-cut behaviour, DEG/RAD/GRAD conversions, mathematical and physical constants, and precision sweeps across requested 16, 50, 100, 250, 500 and 1000 digits. `calculator-scientific-math` separately protects grammar, precedence, exact special values, identities, custom-function precision boundaries, state/display isolation and deterministic parser/resource failures on every platform. MPFR/MPC remain validation-only dependencies and are not shipped as a second production calculation engine.

## Semantic appearance and focus regression

Common 1.19.23 adds explicit semantic roles for heading, summary, kicker, detail/note, status borders, hover accents and warning states. Calculator regression tests assert those fields exist in the exact pinned Common public contract and that GTK, Win32 and SwiftUI consume the roles that match Calculator semantics. Desktop focus treatment is also checked at source-contract level so keyboard-visible focus is not accidentally removed during visual refactoring.


## Advanced Tools validation

`calculator-advanced-tools-test` exercises every released tool family: engineering formulae, dimensional and temperature conversion, IPv4/CIDR, RAID and cluster allocation, leap-day civil dates, constants, descriptive statistics, graph sampling, real root solving, exact decimal/rational arithmetic, arbitrary-precision powers/factorial and complex arithmetic/error handling.

The cross-platform source contract additionally requires native Tools surfaces and graph renderers on GTK, Win32 and SwiftUI and requires the iPhone target to compile the same `advanced_tools.cpp` implementation. Native shells are therefore tested for ownership/parity while the mathematical/domain expectations remain portable core tests.
