## About this edition

**Calculator 0.2.64** provides this complete user manual as one self-contained, illustrated PDF. The Markdown manuscript is a development source; only the PDF is installed. Its interface diagrams are illustrative rather than screenshots.

## About this manual
This is the end-user manual supplied with Calculator. It explains how to use
the application, what results mean, what is intentionally supported, and the
assumptions behind its operation. It is not a compiler log, source-tree tour,
release ledger or installation transcript.

The first part is written for the operator. The second part records operating
principles and assumptions useful to experienced users and maintainers.

## Contents
PART I   OPERATING INSTRUCTIONS
  1. Purpose and start-up
  2. Everyday calculations (Standard)
  3. Scientific calculations
  4. Programmer calculations
  5. Memory, history and additional results
  6. Advanced Tools reference
  7. Preferences and appearance
  8. Linux keyboard shortcuts
  9. Linux command-line calculator
 10. Errors and troubleshooting

PART II  OPERATING PRINCIPLES AND ASSUMPTIONS
 11. Objectives, non-goals and operating assumptions
 12. What each numeric mode actually represents
 13. Expression interpretation and result integrity
 14. Shared engines and native interfaces
 15. State, persistence and privacy boundaries
 16. Resource limits and failure policy
 17. Installation, documentation and compatibility
 18. Glossary and further information


# Part I - Operating instructions

## 1. Purpose And Start-up
Calculator is one application with three calculation modes:

  Standard     Everyday real-number calculations.
  Scientific   Expressions, functions, variables and higher precision.
  Programmer   Fixed-width integer and bit-pattern calculations.

A separate Tools workbench offers specialised calculations including unit
conversion, networking, storage, date/time, graphing and financial functions.

On Linux, start Calculator from the application menu or execute:

  /Programs/calculator

On Windows, open the Calculator application executable. On iPhone, launch
the installed app. All three interfaces use the same calculation rules,
although controls may be arranged differently to suit each platform.

Select Standard, Scientific or Programmer using the visible mode selector.
Enter an expression using the on-screen controls or the available input
field, then press = or Enter to calculate. Disabled controls mean that the
current mode or expression does not admit that action.

Example: 2 + 3 * 4 = 14 (normal operator precedence).

## 2. Everyday Calculations - Standard
Standard is intended for ordinary arithmetic. Its numeric domain is finite
IEEE-754 binary64 ("double"), not arbitrary-precision decimal arithmetic.

Operators include +, -, *, /, parentheses and powers. The keypad also has
square, square root, reciprocal, sign change, clear and backspace.

The key labels mean:

  C       Clear the current calculation.
  CE      Clear the current operand/entry, retaining the pending expression.
  Backspace  Remove the last editable character.
  +/-     Change the sign of the entry.
  =       Evaluate the expression.
  %       Divide the preceding value by 100.

Standard uses conventional precedence and grouping, not strictly left-to-
right pocket-calculator evaluation. Thus:

  2 + 3 * 4       yields 14
  (2 + 3) * 4     yields 20
  200 + 200 / 2   yields 300
  -2^2            yields -4

> **Percentage rule:** The % key divides by 100; it is not a contextual percentage-of-the-previous-value operation.

Important percentage rule: % is a literal postfix "divide by 100", not a
contextual "percentage of the first number" operation. Consequently:

  100 + 10%       yields 100.1
  100 * 10%       yields 10

Repeated = reapplies the previous top-level operation to the prior result
when that operation is available. Use C to begin a wholly new calculation.
CE changes the current entry without wiping the earlier expression.

For exact decimal fractions such as 0.1 + 0.2, use Tools > Exact decimal.
A rounded display in Standard does not imply exact decimal representation.

## 3. Scientific Calculations
Scientific adds mathematical functions, constants, variables, named
functions, complex numbers and adjustable calculation precision.

Enter expressions using numbers, parentheses and operators, for example:

  2^10
  sqrt(2)
  sin(pi/2)          (in RAD mode)
  2pi
  2(3+1)
  0.1 + 0.2

Exponentiation is right associative; a leading sign binds less tightly
than a power. The expression 2^-2 is accepted.

The angle control cycles among DEG (degrees), RAD (radians) and GRAD
(gradians). Select the correct unit before evaluating trigonometry.
Hyperbolic functions do not depend on the angle setting. The 2nd and HYP
controls select alternate function layers. F-E temporarily switches
scientific notation; Clear resets that temporary override.

The default calculation precision is 50 decimal digits. Linux Preferences
accepts 16 through 1000 digits; Windows and iPhone offer native precision
controls or presets. Display formatting is independent of the retained
calculated value.

Common constants include pi, e, tau, phi, i and the documented physical
constants c0, G, h, hbar, kB, NA, qe, me, mp, g0, eps0, mu0 and Rgas.
The name '_' refers to the previous result. The value rand produces a
random value in the interval [0,1). Built-in and reserved names cannot
be overwritten.

Reusable names can be defined in Scientific:

  radius=5
  pi*radius^2
  double(x)=x*2
  double(21)

Multi-argument functions use semicolon-separated parameter names, e.g.:

  hyp2(a;b)=a*a+b*b
  hyp2(3;4)

Function names and argument names must be valid, non-reserved identifiers.
The full built-in catalogue and syntax are owned by the Scientific engine.
On Linux and Windows desktop expression fields, Tab completes recognised
Scientific constants, functions and reusable identifiers.

Complex operations use the maintained complex mathematical domain. For
non-integer powers and logarithms, the standard principal complex branch
is used; consult the numerical semantics when branch selection matters.

## 4. Programmer Calculations
Programmer treats values as bit patterns of a chosen width, rather than
floating-point values.

Choose a number base:
  BIN = base 2     OCT = base 8
  DEC = base 10    HEX = base 16

Choose a width:
  W8, W16, W32 or W64 (8, 16, 32 or 64 bits)

U/S switches unsigned/signed decimal presentation of the same bit pattern.
It does not change the stored bits. Binary, octal and hexadecimal displays
show the underlying unsigned bit pattern even in signed mode.

Available operations include arithmetic, integer remainder (MOD), AND,
OR, XOR, NOT, NAND, NOR, XNOR, left/right shift, arithmetic right shift
(ASR), rotate left/right (ROL/ROR) and byte swap (BSWAP).

  &       Bitwise AND
  |       Bitwise OR
  ^       Bitwise XOR in Programmer (power in Scientific)
  ~       Bitwise complement
  <<      Shift left
  >>      Logical shift right
  ASR     Sign-extending arithmetic shift right
  ROL/ROR Rotate bits within the selected width
  BSWAP   Reverse byte order within the selected width
  MOD     Integer remainder

After an operation the result is masked to the selected width. Overflow
wraps intentionally: for instance, in W8, 255 + 1 becomes zero. An 8-bit
byte swap leaves a value unchanged. Shifts use decimal shift counts; unsafe
counts are rejected. A divide or remainder by zero is an error.

Digits not legal in the selected base are disabled. Changing base or width
changes how expressions and results are interpreted; verify the current
settings before copying a bit pattern to another system.

Use the Bases/Bits representation surface to see BIN, OCT, DEC and HEX
views together and, where provided, inspect or toggle individual bits.

## 5. Memory, History And Additional Results
Calculator provides a memory stack and calculation history.

  MS   Store the current value in a new memory slot.
  MR   Recall the newest memory slot.
  M+   Add to the newest stored value.
  M-   Subtract from the newest stored value.
  MC   Clear all memory slots.

History & Memory lets you inspect, recall or remove individual entries;
whole-history clearing is also available. History is newest-first and
unlimited by default. A Preferences setting can cap its retention.

History recall preserves the result and relevant calculation context;
Scientific precision and angle setting and Programmer radix/width/sign
setting are not reconstructed from rounded display text.

Additional Results displays alternate representations without changing
the computed value: Standard and Scientific offer decimal/scientific/
engineering forms; Programmer offers HEX/DEC/OCT/BIN forms.

Do not assume the current memory stack or result history survives closing
the process. Reusable variables/functions and selected semantic preferences
have a separate persistent-storage contract.

## 6. Advanced Tools Reference
Open Tools to select a specialist calculation. Each command is entered in
the Tools workbench input, NOT as an operating-system shell command. The
workbench supplies prompts and examples. Select the tool, enter one
domain-specific command, and activate Run.

Tool family               Example input
Engineering               ohm 12 2
Unit conversion           100 km mi
ICT/network               subnet 192.168.10.42/24
Storage/filesystem        raid 5 6 4 TiB
Date/time                 diff 2024-01-31 2024-02-29
Constants                 pi
Statistics                1,2,3,4,5
Graphing                  sin(x) ; -6.28 ; 6.28 ; 241
Equation solving          x^2-2 ; 0 ; 2
Exact decimal             0.1 + 0.2
Arbitrary precision       2^256
Complex numbers           mul 1,2 3,-4
Financial                 pmt 250000 0.005 360
Number utilities          factor 360

### Engineering
  Examples include ohm V I, power V I, reactance-c, reactance-l, resonance,
  parallel resistances, three-phase power and decibel functions. Use
  appropriate SI units and check the requested command's operand order.

### Unit conversion
  Enter "value FROM TO". Units must belong to the same dimension.
  Supported families include length, mass, area, temperature, volume,
  speed, pressure, energy, power, time, frequency and angle.
  Affine conversions (including temperature) are handled explicitly.

### ICT/network
  subnet IPv4/prefix reports address, mask, network, broadcast, usable
  range and count. "cidr usable-hosts" selects a conventional IPv4 subnet;
  "transfer bytes bits-per-second" estimates transfer duration. Network
  operations use IPv4 arithmetic, not an IPv6 subnet model.

### Storage/filesystem
  "convert value FROM TO" handles bits/bytes, decimal storage units and
  binary KiB-style units. "raid LEVEL disks size-per-disk unit" estimates
  supported RAID 0/1/5/6/10 capacity. "clusters file-bytes cluster-bytes"
  calculates allocation and slack. These are mathematical estimates and
  do not probe a physical filesystem.

### Date/time
  "diff YYYY-MM-DD YYYY-MM-DD" gives calendar-aware differences.
  "add YYYY-MM-DD days" adds a raw day count.
  "add YYYY-MM-DD N years N months N weeks N days" and "sub ..." perform
  calendar-aware shifts. Example: add 2024-01-31 0 years 1 months
  0 weeks 0 days results in 2024-02-29 (end-of-month clamping).
  "unix YYYY-MM-DDTHH:MM:SSZ" interprets the timestamp as UTC.
  The supported Gregorian year range is 1 through 9999.

### Constants
  Enter "list" or a named mathematical/physical constant. Constants are
  also available in Scientific expressions.

### Statistics
  Enter comma- or space-separated finite samples. Output includes count,
  sum, mean, median, quartiles, spread, population variance and standard
  deviation; sample statistics are shown when defined.

### Graphing
  "expression ; xmin ; xmax ; samples" samples an expression in x.
  The default is 201 samples; permitted count is 2 through 4096.
  Discontinuities remain gaps instead of false lines between invalid points.

### Equation solving
  "expression ; xmin ; xmax" searches for real roots where the expression
  equals zero. This is a bounded numerical estimate, not an exhaustive
  symbolic proof; an interval may contain roots the search cannot certify.

### Exact decimal
  Use ordinary decimal literals and +, -, *, /, parentheses. Values are
  exact rational numbers. "0.1 + 0.2" is exactly 3/10 = 0.3.

### Arbitrary precision
  Exact signed integers support +, -, *, powers and factorial.
  Exponents must be non-negative; this tool limits factorial to 1000.

### Complex numbers
  Use real,imaginary pairs, e.g. "mul 1,2 3,-4". Commands include
  add, sub, mul, div, conj, abs, arg and polar.

### Financial
  Available operations include ctrm, ddb, fv, gpm, pmt, pv, rate, sln,
  syd and term. Interest rates are DECIMAL PERIODIC rates: 0.005 is
  0.5 percent per period, not 5 percent. Verify period units.

### Number utilities
  Commands include mod, factor, gcd, lcm, perm, comb, root, char, code,
  ones and twos. The workbench prompt specifies argument form.

The Tools workbench reports errors for missing arguments, inconsistent
units, unsupported forms and mathematical domain failures. Refer to the
per-tool prompt for complete argument syntax.

## 7. Preferences And Appearance
System, Day and Night appearance modes are available. System follows the
host's light/dark preference; Day and Night select the application's
defined light and dark palettes.

On Linux, Preferences includes:
- Result presentation: Automatic, Fixed, Scientific or Engineering.
- Number of displayed decimal places.
- Scientific calculation precision (16..1000 digits).
- History limit (0 means unlimited).
- Thousands separators and trailing-zero presentation.

These settings change presentation and/or the chosen calculation context
as labelled. Changing a display format or enabling grouped digits does
not silently change the retained underlying value.

## 8. Linux Keyboard Shortcuts
Linux's built-in F1 shortcut window is the authority if a key mapping
changes in a later release. Current principal shortcuts are:

  Enter                  Calculate
  Escape / Ctrl+Delete   Clear
  Ctrl+Z                 Undo expression edit
  Ctrl+Shift+Z           Redo expression edit
  Ctrl+N                 New independent Calculator window
  Ctrl+W                 Close this Calculator window
  Ctrl+Q                 Quit Calculator
  F1 or Ctrl+?           Keyboard shortcuts
  Tab                    Complete a Scientific name/function

  Ctrl+Alt+B             Standard mode
  Ctrl+Alt+A             Scientific mode
  Ctrl+Alt+P             Programmer mode
  Ctrl+Alt+F/K/T         Tools workbench

  Ctrl+P                 Insert pi (Scientific)
  Ctrl+R                 Insert sqrt( (Scientific)
  Ctrl+E                 Insert e (Scientific)
  Ctrl+B/O/D/H           Choose Programmer base
  Alt+Left/Right         Older/newer history entry

The Windows and iPhone shells use native control and input conventions;
the Linux-only key list is not a promise of identical shortcuts elsewhere.

## 9. Linux Command-line Calculator
The Linux executable also provides a Scientific expression command line:

  /Programs/calculator /cli "2+2"

With no expression after /cli it opens an interactive prompt:

  /Programs/calculator /cli
  > sqrt(2)
  > quit

The CLI evaluates Scientific expressions using the saved calculation
context. It shares reusable variables and functions with the Linux GUI,
subject to conflict-checked persistence. It does not implement a separate
Programmer or Tools command-line dialect. An erroneous expression produces
an error rather than an invented value.

## 10. Errors And Troubleshooting
  Unexpected result:
    Check Standard versus Scientific versus Programmer, input parentheses,
    the angle unit, Programmer base/width/sign, precision and % semantics.

  Disabled digit or key:
    The selected base or current calculator state does not admit the action.

  Variable/function error:
    Check spelling, parentheses, argument count and reserved built-in names.
    Scientific names are evaluated under their documented identifier rules.

  Invalid conversion:
    Verify that FROM and TO units share a physical dimension.

  Blank gap in graph:
    The function was undefined or could not produce a safe finite point.

  No or fewer equation roots than expected:
    This is an interval-sampling real numerical solver, not an exhaustive
    proof of all mathematical roots.

  Domain/overflow/resource error:
    The engine rejected an invalid or unrepresentable result. Change the
    input or use an appropriate exact/high-precision calculation domain.

  Lost history after exit:
    History and memory are not part of the saved reusable-name document.


# Part II - Operating principles and assumptions

## 11. Objectives, Non-goals And Operating Assumptions
Calculator is intended to be a native, readable, precise and independently
installable member of the InfiltratorOS application family.

Core assumptions:
  A. A calculation must have one defined interpretation and error policy.
  B. Numerical domains are not interchangeable merely because they display
     similar decimal text.
  C. Platform-specific drawing should not change mathematical answers.
  D. Operator entry must not silently change the stored numeric result.
  E. Invalid results should be reported, not made to look plausible.
  F. Product-neutral mechanisms belong in Common; Calculator-specific
     arithmetic and user interaction rules belong in Calculator.
  G. A feature is complete when implementation, boundary tests and
     documented behaviour agree, not just when an ordinary example works.
  H. Mature, proven techniques are preferred when they are the sounder
     engineering choice; novelty by itself is not an objective.

Non-goals:
- Making Standard behave like a full symbolic algebra system.
- Making real floating-point arithmetic exact for every decimal input.
- Presenting every Programmer operation as ordinary signed arithmetic.
- Treating graph/real-root estimates as formal mathematical proofs.
- Forcing Linux, Windows and iPhone to use identical UI toolkits.
- Including compiler logs or internal development artifacts at runtime.

## 12. What Each Numeric Mode Actually Represents
Standard: finite IEEE-754 binary64. Many decimal fractions are only
approximately representable. The stored binary64 value remains authoritative
even when displayed with fewer decimals.

Scientific: an arbitrary-precision real/complex calculation backend using
Boost.Multiprecision. Default precision is 50 decimal digits, user-adjustable
from 16 through 1000. It is more precise, but requested finite precision
is not a guarantee that every result is mathematically exact.

Programmer: explicitly masked 8/16/32/64-bit integer bit patterns. Wraparound
is required behaviour, not accidental overflow. Signed decimal presentation
is a view of those same bits.

Exact Decimal: exact numerator/denominator rational arithmetic. Use it
when decimal fractions must be exact rather than floating-point estimates.

Arbitrary Precision: exact signed integers subject to operation and output
resource ceilings.

Graph and root search ultimately require finite binary64 coordinates or
interval endpoints. Certain physical/financial tools also have a checked
binary64 input/output boundary, even when their intermediate work benefits
from the Scientific backend.

## 13. Expression Interpretation And Result Integrity
Shared arithmetic precedence is defined: powers bind tighter than leading
unary minus, then multiplication/division, then addition/subtraction.
Parentheses override precedence. Powers associate right-to-left.

The % operator represents multiplication by 0.01, not an implicit
percentage-of-earlier-operand convention. In Programmer mode %/MOD is
integer remainder; interpretation is mode-specific and explicit.

Scientific complex functions use their documented principal branches.
Factorials, exponents, shift counts and recursive functions have defined
domain and resource limits.

Display text is a presentation of a result, not a second computational
authority. In particular, a formatted or rounded output string must not be
reparsed as a replacement for the retained full-precision value.

History stores relevant result semantics so recalling earlier expressions
does not inadvertently re-run a random function or assignment.

## 14. Shared Engines And Native Interfaces
The C++17 calculation core owns mathematical meaning, parsing and
domain-specific evaluation. The shared Controller owns mode and command
behaviour, enablement, memory/history and formatting choices.

Linux uses GTK4, Windows uses native Win32 and iPhone uses SwiftUI with a
thin Objective-C++ bridge. These interfaces may draw differently, but they
call the same calculation/interaction mechanisms rather than independently
reimplementing the mathematics.

Common is an immutable, pinned shared dependency for cross-product utility,
theme, typography and platform infrastructure. Common does not own
Calculator-specific expression grammar or Programmer wraparound semantics.

## 15. State, Persistence And Privacy Boundaries
Saved preferences and reusable Scientific variables/functions use a
versioned shared Controller state format. Linux stores this through XDG
persistence, Windows through native settings storage, and iPhone through
native application preferences.

History and the memory stack are session concerns. They are not guaranteed
to survive application exit merely because selected settings do.

The Linux CLI and GUI share the same reusable variable/function document.
Concurrent conflicting edits can produce an explicit save error rather
than silently overwriting another writer's changes.

The Tools command input is a domain grammar, not a shell command. No
advanced-tool command executes an arbitrary operating-system program.

## 16. Resource Limits And Failure Policy
The Scientific/Advanced Tool input boundary is 8192 UTF-8 bytes. The
multiprecision evaluator applies cumulative work and recursion limits,
including a maintained maximum of 20,000 materialised decimal digits.
Scientific factorial can reach 100000 subject to its budget; the distinct
Arbitrary Precision tool limits factorial to 1000. Graphing admits 2..4096
sample points.

These are intentional protections against excessive computation,
unbounded memory use and apparently successful wrong answers. Overflow,
non-zero underflow, invalid domains, malformed expressions, divide by
zero, unsupported constants and resource exhaustion must fail explicitly.

An engine may refuse a computation it cannot justify; this is preferable
to displaying a mathematically misleading result.

## 17. Installation, Documentation And Compatibility
On Linux, the Calculator executable resides at /Programs/calculator.
A standalone Debian/Mint installation can establish the InfiltratorOS
directory skeleton while retaining normal desktop and package metadata.

This manual is intentionally installed as the user-facing document at:

  /System/Resources/Documentation/Calculator/Calculator-Manual.pdf

The documentation directory must not contain native compiler profiles,
development build reports, copied source trees or redundant licence files.
Debian-mandated package copyright metadata is stored separately in the
standard Debian package location.

Some stable application identifiers may retain historical spelling for
settings and upgrade compatibility. The current application and repository
identity remains Calculator.

## 18. Glossary And Further Information
Binary64       IEEE-754 double-precision floating-point representation.
Bit pattern    A fixed number of binary digits interpreted under a width.
Controller     Shared authority for calculator interaction and state.
Domain         The representation and rules for a class of calculation.
Exact rational Integer numerator and denominator without float rounding.
Multiprecision A numeric backend with configurable precision above binary64.
PGO            Profile-guided optimisation used during native builds.
Principal      Conventional selected branch of a multivalued complex
               mathematical function.
Wraparound     Result reduced to the selected fixed bit width.

For code-level reasoning and historical decisions, consult the Calculator
source repository:

  https://github.com/Infiltrator-Projects/Calculator

Its docs/ directory holds the detailed architecture, decisions, numerical
semantics, tool definitions and validation records. Those developer documents
are deliberately NOT copied into the installed Calculator documentation
directory.


