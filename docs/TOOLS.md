<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Advanced Calculator Tools

Calculator 0.2.0 exposes twelve extended calculation families through the **Tools** button on Linux, Windows and iPhone. The workbench is native on each platform, but the catalogue, prompts, examples, evaluation rules and graph samples come from one shared C++ implementation in `src/core/advanced_tools.*`.

The input field is a compact domain command, not a shell or scripting language. It never executes host commands.

## Engineering

Supported forms:

- `ohm V I` — voltage/current/resistance/power relationship
- `power V I`
- `reactance-c Hz F`
- `reactance-l Hz H`
- `resonance H F`
- `parallel R1,R2,...`
- `three-phase V I PF` where PF is 0..1
- `db ratio` for amplitude ratios
- `db-power ratio` for power ratios

Example: `ohm 12 2`.

## Unit conversion

Form: `value FROM TO`.

Dimensions are checked; conversion between unrelated dimensions is rejected. Supported groups include length, mass, temperature, area, volume, speed, pressure, energy, power and angle. Representative symbols include `m km cm mm in ft yd mi nmi`, `kg g lb oz`, `C F K`, `m2 ft2 acre ha`, `L galUS galUK`, `mps kph mph knot`, `Pa kPa bar psi atm`, `J kWh BTU`, `W kW hp`, and `deg rad grad`.

Example: `100 km mi`.

## ICT / network

- `subnet IPv4/prefix` — address, netmask, network, broadcast, usable range and count
- `cidr usable-hosts` — smallest conventional IPv4 subnet including network/broadcast reservation
- `transfer bytes bits-per-second` — transfer duration

Example: `subnet 192.168.10.42/24`.

## Storage / filesystem

- `convert value FROM TO` for decimal B/kB/MB/GB/TB/PB and binary KiB/MiB/GiB/TiB/PiB
- `raid LEVEL disks size-per-disk unit` for RAID 0/1/5/6/10
- `clusters file-bytes cluster-bytes` for allocation and slack

Example: `raid 5 6 4 TiB`.

## Date / time

- `diff YYYY-MM-DD YYYY-MM-DD`
- `add YYYY-MM-DD days`
- `unix YYYY-MM-DDTHH:MM:SSZ`

Gregorian dates are validated for years 1..9999. Unix conversion is explicitly UTC.

## Constants

Enter `list` or a constant name/alias. The catalogue includes `pi`, `e`, `tau`, `phi`, speed of light `c0`, gravitational constant `G`, Planck constants `h`/`hbar`, Boltzmann `kB`, Avogadro `NA`, elementary charge `qe`, electron/proton masses, standard gravity, vacuum permittivity/permeability and the molar gas constant.

The ASCII names are also accepted by the Scientific expression parser.

## Statistics

Enter comma- or space-separated finite values. Output includes count, sum, mean, median, Q1/Q3, min/max/range, population variance/stddev and sample variance/stddev when defined.

## Graphing

Form: `expression ; xmin ; xmax ; samples`.

`x` is the independent variable and the ordinary Scientific grammar is used. Samples default to 201 and are limited to 2..4096. Invalid points are preserved as discontinuities. GTK, Win32 and SwiftUI render the same sampled data natively.

Example: `sin(x) ; -6.283185307 ; 6.283185307 ; 241`.

## Equation solving

Form: `expression ; xmin ; xmax`. The solver finds validated real roots of `expression = 0` using deterministic interval scanning and safeguarded bisection. This is numerical real root solving, not symbolic algebra.

Example: `x^2-2 ; 0 ; 2`.

## Exact decimal

Use decimal literals, `+`, `-`, `*`, `/` and parentheses. Decimal literals are converted directly into exact integer ratios, never through binary64.

Example: `0.1 + 0.2` returns exact `3/10` and `0.3`.

## Arbitrary precision

Signed integer arithmetic supports `+`, `-`, `*`, `^`, parentheses and postfix `!`. Exponents are non-negative integers; factorial is supported through 1000. A 20,000-digit ceiling bounds resource use.

Example: `2^256`.

## Complex numbers

Complex values use `real,imaginary`.

- `add|sub|mul|div a,b c,d`
- `conj|abs|arg|polar a,b`

Example: `mul 1,2 3,-4`.

## Ownership and validation

The platform workbenches only collect input and render shared results. They do not contain duplicate mathematical implementations. See [ARCHITECTURE.md](ARCHITECTURE.md), [NUMERICS.md](NUMERICS.md), [UI_PARITY.md](UI_PARITY.md) and [VALIDATION.md](VALIDATION.md) for maintained contracts and evidence limits.
