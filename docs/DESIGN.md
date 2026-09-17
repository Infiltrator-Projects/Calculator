<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Infiltrator Calc Design Direction

The calculator begins with a small expression engine and a deliberately extensible application boundary.

The GUI should remain thin. Calculation semantics, numeric-domain decisions and validation belong in the core. Presentation controls should select operations rather than implement them independently.

The initial numeric representation is `double` only to keep the first application small. The core API is intentionally not designed around textual button operations, so exact integers, arbitrary precision, unit-aware values and symbolic forms can be introduced without replacing the desktop application.

MB Corpo typography is a presentation preference. The application must remain fully usable with the normal system font stack when those proprietary fonts are not installed.
