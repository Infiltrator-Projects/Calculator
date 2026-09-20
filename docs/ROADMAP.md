# Roadmap

This is a direction document, not a dated promise. The released source and tests define what is actually supported.

## Current foundation

- maintain Standard, Scientific and Programmer modes over one shared calculation/session architecture, including stateful Scientific 2nd/HYP/angle/notation controls, fixed-width Programmer rotate/NAND/NOR operations and shared Additional Results representations
- preserve native GTK4, Win32 and SwiftUI interfaces without duplicating calculation semantics
- keep numerical edge behaviour, cross-platform parity and exact Common integration protected by regression tests
- maintain the shared Advanced Tools engine and the native Linux, Windows and iPhone workbenches over the same domain contracts and graph samples

## Completed capability families

Calculator 0.2.0 completes the previously listed feature families:

- engineering calculator functions
- unit conversion
- ICT/network calculations
- storage/filesystem calculations
- date/time calculations
- broader mathematical and physical constants
- descriptive statistics
- graphing
- real equation solving
- exact-decimal/rational arithmetic
- arbitrary-precision integer arithmetic
- complex-number arithmetic
- mature desktop-calculator financial functions and number utilities
- GNOME/Mint-class expression conveniences and a substantially expanded unit catalogue
- Mint-compatible Linux keyboard accelerators for clear, scientific entry, Programmer bases and history navigation
- unbounded default calculation history, while retaining an optional bounded session contract for embedded/test use
- a first-class Linux conversion surface with persistent dimension/source/target selection and live conversion
- persistent Linux user variables and calculator semantic state (angle unit plus Programmer radix/width/signed mode)

These are implemented in the shared C++ core, exercised by deterministic tests and exposed through native Tools workbenches on all three release platforms. Graph rendering is platform-native while graph sampling remains shared.

## Final Mint replacement target

The only remaining required Linux Mint / GNOME Calculator 41.1 parity item is an **integrated arbitrary-precision real/complex Scientific value domain**.

This is a core numeric-architecture change, not another Tools entry. Scientific expressions must be able to carry high-precision real and complex values through arithmetic and transcendental operations without reducing intermediate values to binary64. The existing Exact Decimal, Arbitrary Precision and Complex tools remain useful specialist surfaces, but they do not complete this requirement by themselves.

The preferred direction is a rigorously contained MPFR/MPC-backed value layer, or an equivalently proven implementation, behind the shared C++ expression/session/controller architecture. Native GTK4, Win32 and SwiftUI shells must continue to render shared results rather than owning mathematical semantics.

Completion requires:

- direct high-precision real and complex Scientific expression evaluation;
- high-precision arithmetic, powers, roots, logarithmic/exponential, trigonometric, inverse and hyperbolic operations where defined;
- explicit precision, rounding, domain/error and real/complex promotion rules;
- no silent binary64 truncation in the Scientific calculation path;
- preserved fixed-width Programmer semantics and deliberate Standard interaction semantics;
- cross-platform parity through the shared controller/session;
- independent numerical regression/oracle evidence for huge, tiny, complex and branch/domain boundary cases;
- maintained architecture, numerics and validation documentation matching the released implementation.

Live currency conversion is an explicit non-goal and must not block Mint replacement completeness.

## Continuing priorities

- complete and validate the integrated arbitrary-precision real/complex Scientific value domain
- strengthen numerical reference evidence and cross-platform boundary testing as mathematical features evolve
- improve accessibility and native interaction quality without weakening shared calculator semantics
- expand individual tool catalogues only when their units, numeric domain and validation rules are explicit
- continue consolidating genuinely generic mechanisms into Common only when its contract remains at least as strong as Calculator's local implementation

## Admission rule

A proposed capability enters the roadmap only when its numeric/domain representation, ownership, user interaction and credible validation strategy are clear. Features are not admitted merely because another calculator exposes them.

## Completion rule

An item is complete when implementation, relevant edge-case tests, user-visible behaviour and maintained documentation agree. A checkbox or release number cannot substitute for missing evidence.
