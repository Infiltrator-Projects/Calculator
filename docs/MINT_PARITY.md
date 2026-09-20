# Linux Mint / GNOME Calculator parity

## Baseline

Linux Mint 22.x deliberately ships the GTK3-era GNOME Calculator 41.1 line rather than the newer GTK4 application. Linux Mint 22.3 Zena carries `gnome-calculator 1:41.1+mint1+wilma`. The corresponding upstream GNOME 41.1 tag resolves to commit `1e576c9b024439c3d6fe814b40c22edfa94618e6`; that exact source tree and Mint package metadata are the replacement-compatibility baseline.

The comparison is made against the upstream GNOME Calculator 41.1 source and help contract. Newer GNOME Calculator releases are also useful research inputs, but they are not substituted for the actual Mint 22.x baseline when making replacement claims.

## Behaviour adopted or exceeded

Calculator provides Standard, Scientific and Programmer calculation modes plus a shared native Tools workbench. The current implementation covers or exceeds the Mint baseline in these areas:

- ordinary and order-of-operations expressions, contextual percentages, memory and recallable history;
- DEG/RAD/GRAD trigonometry, inverse and hyperbolic functions, logarithms, roots, powers, factorial and physical constants;
- Programmer BIN/OCT/DEC/HEX, 8/16/32/64-bit widths, signed/unsigned interpretation, Boolean operations, shifts plus rotate/NAND/NOR, and simultaneous radix representations;
- the complete mature financial family Ctrm/Ddb/Fv/Gpm/Pmt/Pv/Rate/Sln/Syd/Term;
- dimensional conversion covering the GNOME 41.1 ordinary unit families including decimal/IEC digital storage, plus Calculator-specific pressure, energy, power, ICT/network and storage/filesystem tools;
- exact decimal/rational arithmetic, bounded arbitrary-precision integer arithmetic, statistics, graphing and real equation solving, which are not all presented as equivalent first-class families by GNOME Calculator 41.1;
- expression conveniences including `mod`, `**`, implicit multiplication, `frac`, `int`, `round`, `sgn`, previous result `_`, `rand`, Unicode ×/÷/−, π/τ/√, absolute-value bars, superscript powers and inverse-function ⁻¹ notation;
- Linux keyboard compatibility for Escape/Ctrl+Delete clear, Ctrl+P π, Ctrl+R square-root entry, Ctrl+E exponent entry, Programmer Ctrl+B/O/D/H base selection, Ctrl+Alt B/A/P mode selection, Ctrl+Alt F/K/T Tools access, Ctrl+W/Ctrl+Q close/quit, F1/Ctrl+? shortcut discovery and Alt+Left/Right history navigation;
- unbounded history by default, with an optional explicit limit available to embedders/tests;
- reusable one- and multi-argument user functions with optional descriptions, deterministic XDG persistence and transactional reload;
- direct click/touch Programmer bit toggling in the Linux representations view while preserving shared fixed-width semantics;
- persistent Linux result-presentation preferences for Automatic, Fixed, Scientific and Engineering output, decimal-place control, thousands grouping and trailing-zero presentation;
- persistent Cinnamon/GTK desktop mode and window size without forcing legacy absolute window positioning under Wayland;
- persistent Scientific angle unit plus Programmer radix, word width and signed-display state, matching the useful semantic settings GNOME Calculator restores;
- persisted user variables under the XDG data directory, alongside the already-persisted reusable function definitions;
- a dependency-light console calculator, `infiltrator-calc-cli`, covering the Mint/GNOME `gcalccmd` one-shot and interactive workflows while sharing the same expression/session engine and persisted custom functions;
- native GTK expression undo/redo enabled explicitly through the GTK4 editable contract, retaining familiar Ctrl+Z/Ctrl+Shift+Z editing behaviour;
- Ctrl+N opens a genuinely independent Calculator process/window instead of sharing the GTK shell's global widget/controller state; this preserves the useful GNOME multi-window workflow without introducing cross-window state contamination;
- a first-class Linux unit-conversion picker with dimension/source/target selectors, live conversion, one-click swapping and persisted pair selection.

## Deliberate architectural differences

Calculator is cross-platform and keeps one C++ calculation/controller contract behind native GTK4, Win32 and SwiftUI shells. It therefore does not copy GNOME Calculator's Vala/GTK3 UI, GSettings schema or MPFR/MPC implementation.

Additional Results presents decimal, scientific and engineering representations together instead of requiring a single global result-format choice. Theme and typography follow the Infiltrator Common design rather than the Cinnamon/GTK3 theme contract.

## Remaining parity work

There is now one required GNOME Calculator 41.1 replacement target:

- an integrated arbitrary-precision real/complex Scientific value domain comparable in mathematical capability to GNOME's MPFR/MPC path, rather than Calculator's current binary64 Scientific engine plus separate exact/arbitrary/complex tools.

Currency conversion is deliberately **not** part of the replacement target. Live network-backed currency rates add privacy, freshness and service-dependency concerns that are outside Calculator's required system-calculator role. The absence of currency conversion must therefore not be counted as a Mint-parity defect.

### Required arbitrary-precision Scientific work

The remaining target is not satisfied by exposing separate Exact Decimal, Arbitrary Precision or Complex tools. Scientific mode itself must gain a first-class high-precision numeric value domain.

The implementation should:

- keep the expression/session/controller architecture shared across Linux, Windows and iPhone;
- support arbitrary-precision real values directly in ordinary Scientific expressions;
- support complex values directly in the same Scientific expression domain where mathematically meaningful;
- cover arithmetic, powers, roots, logarithmic/exponential functions, trigonometric and inverse/hyperbolic functions without silently reducing intermediate values to binary64;
- preserve exact/fixed-width Programmer semantics as a separate integer domain;
- retain Standard mode's deliberate desktop-calculator interaction semantics unless a precision improvement can be introduced without changing those semantics;
- use a proven arbitrary-precision mathematical implementation for transcendental and complex behaviour rather than attempting an unvalidated hand-written replacement;
- keep platform shells free of duplicated numerical logic;
- add independent high-precision oracle/regression coverage across huge, tiny, complex and branch/domain boundary cases;
- document precision selection, rounding behaviour, error/domain handling and conversion between real/complex/high-precision and display representations.

The preferred engineering direction is a contained MPFR/MPC-backed value layer (or an equivalently rigorous implementation) behind Calculator's shared C++ core. Any new dependency must remain isolated from the native UI shells and justified by correctness rather than convenience.

### Completion criteria

This final Mint replacement item is complete only when ordinary Scientific expressions can perform arbitrary-precision real and complex calculations directly, the shared controller/session preserves those values without binary64 truncation, Linux/Windows/iPhone expose the same semantics, and release CI proves the numerical boundary behaviour. Separate tool access does not satisfy this criterion.

## Validation rule

Replacement parity is demonstrated by behaviour and regression tests, not by source similarity. Calculator may adopt an interaction learned from GNOME Calculator while implementing it independently in the shared Calculator architecture. Where Calculator intentionally differs, the difference should either strengthen the product or remain explicitly documented until the gap is closed.
