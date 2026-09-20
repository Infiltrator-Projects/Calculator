# Linux Mint / GNOME Calculator parity

## Baseline

Linux Mint 22.x deliberately ships the GTK3-era GNOME Calculator 41.1 line rather than the newer GTK4 application. The package used by Mint 22/22.3 is the Mint rebuild of that 41.1 source line. Calculator treats that application as a replacement-compatibility baseline for the Linux desktop, not as its architecture or visual specification.

The comparison is made against the upstream GNOME Calculator 41.1 source and help contract. Newer GNOME Calculator releases are also useful research inputs, but they are not substituted for the actual Mint 22.x baseline when making replacement claims.

## Behaviour adopted or exceeded

Calculator provides Standard, Scientific and Programmer calculation modes plus a shared native Tools workbench. The current implementation covers or exceeds the Mint baseline in these areas:

- ordinary and order-of-operations expressions, contextual percentages, memory and recallable history;
- DEG/RAD/GRAD trigonometry, inverse and hyperbolic functions, logarithms, roots, powers, factorial and physical constants;
- Programmer BIN/OCT/DEC/HEX, 8/16/32/64-bit widths, signed/unsigned interpretation, Boolean operations, shifts plus rotate/NAND/NOR, and simultaneous radix representations;
- the complete mature financial family Ctrm/Ddb/Fv/Gpm/Pmt/Pv/Rate/Sln/Syd/Term;
- dimensional conversion with a catalogue at least as broad as the GNOME 41-era ordinary unit set, plus Calculator-specific ICT/network and storage/filesystem tools;
- exact decimal/rational arithmetic, bounded arbitrary-precision integer arithmetic, statistics, graphing and real equation solving, which are not all presented as equivalent first-class families by GNOME Calculator 41.1;
- expression conveniences including `mod`, `**`, implicit multiplication, `frac`, `int`, `round`, `sgn`, previous result `_`, `rand`, Unicode ×/÷/−, π/τ/√, absolute-value bars, superscript powers and inverse-function ⁻¹ notation;
- Linux keyboard compatibility for Escape/Ctrl+Delete clear, Ctrl+P π, Ctrl+R square-root entry, Ctrl+E exponent entry, Programmer Ctrl+B/O/D/H base selection and Alt+Left/Right history navigation;
- unbounded history by default, with an optional explicit limit available to embedders/tests.

## Deliberate architectural differences

Calculator is cross-platform and keeps one C++ calculation/controller contract behind native GTK4, Win32 and SwiftUI shells. It therefore does not copy GNOME Calculator's Vala/GTK3 UI, GSettings schema or MPFR/MPC implementation.

Additional Results presents decimal, scientific and engineering representations together instead of requiring a single global result-format choice. Theme and typography follow the Infiltrator Common design rather than the Cinnamon/GTK3 theme contract.

## Remaining parity work

The following GNOME Calculator 41.1 capabilities remain meaningful comparison targets rather than being falsely claimed complete:

- live network-backed currency conversion with cached/offline behaviour and an explicit privacy/off switch;
- user-defined multi-argument functions persisted as reusable calculator state;
- an integrated arbitrary-precision real/complex Scientific value domain comparable to GNOME's MPFR/MPC path, rather than Calculator's current binary64 Scientific engine plus separate exact/arbitrary/complex tools;
- configurable output accuracy, thousands grouping and trailing-zero presentation;
- direct mouse/touch bit toggling in the Programmer bit view;
- persistent desktop session preferences such as selected conversion pair and geometry where platform conventions make that useful.

These items are treated as real engineering work. They are not considered complete merely because another Calculator tool can approximate the same end result.

## Validation rule

Replacement parity is demonstrated by behaviour and regression tests, not by source similarity. Calculator may adopt an interaction learned from GNOME Calculator while implementing it independently in the shared Calculator architecture. Where Calculator intentionally differs, the difference should either strengthen the product or remain explicitly documented until the gap is closed.
