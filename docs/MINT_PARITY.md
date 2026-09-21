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

## Replacement parity status

The required Linux Mint 22.x / GNOME Calculator 41.1 replacement target is complete on current main.

Scientific mode now carries first-class arbitrary-precision real and complex values in the shared C++ core. Decimal input is parsed directly into the high-precision domain; arithmetic, powers, roots, logarithmic/exponential functions, trigonometric and inverse/hyperbolic functions remain in that domain instead of silently reducing intermediates to binary64. Session variables, assignments, history, controller results and Additional Results preserve the precise value as decimal component text, so GTK4, Win32 and SwiftUI consume the same semantics without owning numerical logic.

The production backend uses Boost.Multiprecision standalone cpp_bin_float / cpp_complex with a maintained ceiling of 1000 decimal digits and a default Scientific precision of 50 digits. MPFR/MPC remain independent release-CI oracle dependencies rather than runtime requirements.

Standard mode intentionally retains its conventional immediate-calculator binary64 interaction semantics, and Programmer mode retains exact fixed-width integer semantics. Currency conversion remains an explicit non-goal and is not a Mint-parity defect.

Completion evidence includes shared Scientific parser tests, precise Session/Controller persistence tests, cross-platform builds, independent MPFR/MPC oracle regression, sanitizers and native Linux/Windows/iPhone CI.

## Validation rule

Replacement parity is demonstrated by behaviour and regression tests, not by source similarity. Calculator may adopt an interaction learned from GNOME Calculator while implementing it independently in the shared Calculator architecture. Where Calculator intentionally differs, the difference should either strengthen the product or remain explicitly documented until the gap is closed.
