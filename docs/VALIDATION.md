# Validation

## Purpose

Validation distinguishes implemented behaviour from behaviour that has actually been demonstrated. Compilation, deterministic numerical tests, native runtime checks, Simulator builds and physical-device observation prove different things.

## Automated evidence

The repository currently uses:

- `.github/workflows/release.yml`

The `tests/` tree covers expression grammar and nesting limits, Standard immediate semantics, gradian/inverse/hyperbolic Scientific transforms and F-E formatting, Programmer width/radix/ROL/ROR/NAND/NOR behaviour, explicit memory store/update state, structured history ordering/context/recall, shared controller behaviour, desktop UI contracts and source-level cross-platform ownership rules.

Every `main` push and manual workflow dispatch runs the cross-platform verification jobs. Release publication remains gated to explicit `Release ...` commits. Verification:

- builds and tests the shared core on supported build hosts;
- runs the portable Calculator core/controller tests under Clang AddressSanitizer and UndefinedBehaviorSanitizer;
- builds the native Windows application and runs a Win32 runtime/keypad smoke path;
- builds the GTK/Linux target and Debian package;
- compiles the iPhone application for iOS Simulator through the shared C++ controller and verifies AppIcon metadata;
- compiles the unsigned ARM64 iPhoneOS target;
- verifies expected binary assets plus a deterministic source bundle containing the exact Common checkout, and checksums all published payloads; and
- verifies Package Repository publication for the Debian package.

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
