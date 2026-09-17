<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Infiltrator Calc

Infiltrator Calc is a native desktop calculator for the Infiltrator software family and the future Infiltrator Mint desktop.

The project is deliberately larger in ambition than a four-function calculator, but the implementation grows in layers. The calculation engine is independent of the graphical interface so it can become a reusable foundation rather than a collection of button callbacks.

**Current source version:** 0.1.1  
**Language:** C++17 application/core with C-based GTK4 presentation APIs  
**Shared foundation:** Infiltratr Common 1.17.0  
**Design contract:** Infiltrator Design v1  
**Licence:** GPL-3.0-or-later

## Initial scope

The first implementation establishes:

- a portable expression evaluator;
- operator precedence and parentheses;
- unary operators;
- powers;
- floating-point numeric literals;
- explicit calculation errors;
- a native GTK4 desktop interface;
- keyboard-first expression entry;
- Infiltrator graphite/silver visual styling;
- MB Corpo font roles with normal desktop fallback; and
- deterministic core regression tests.

The calculation engine must never execute expressions through a shell or external interpreter.

## Planned capability families

The architecture is intended to grow into:

- Standard, Scientific, Programmer and Engineering modes;
- exact integer and wider numeric domains where useful;
- variables, memory and reusable expressions;
- calculation history;
- mathematical and physical constants;
- unit conversion;
- ICT/network calculations;
- storage and filesystem calculations;
- date/time and epoch calculations;
- statistics, graphing and equation solving;
- complex and arbitrary-precision mathematics where justified; and
- Infiltrator-specific engineering calculators.

Features are considered complete only when implementation, tests and documented behaviour agree.

## Shared Infiltrator design

Infiltrator Calc uses Infiltratr Common as its reusable software foundation and follows the canonical Infiltrator Design v1 visual contract. The common design language is a graphite/silver foundation with near-black backgrounds, layered dark panels, restrained silver borders and MB Corpo typography roles.

The calculation core links against `InfiltratrCommon::Portable` from the pinned Common revision. The desktop UI consumes the same named typography roles and palette defined by the shared design contract rather than introducing a project-specific visual system.

## Typography

Infiltrator Calc prefers locally installed MB Corpo fonts when available and falls back automatically to normal system fonts when they are absent.

The project does **not** redistribute proprietary MB Corpo font binaries. The current UI roles are:

- `MB Corpo S Title WEB` for normal interface text and controls;
- `MB Corpo A Title Cond WEB` for product/display titles.

This follows the established Infiltrator desktop applications.

## Build

Clone recursively so the exact Infiltratr Common revision is available:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Infiltrator-Calc.git
cd Infiltrator-Calc
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Debian-family systems the initial development dependencies are the standard C/C++ toolchain, CMake, GTK4 development packages and pkg-config.

## Project structure

```text
src/
├── app/                 GTK desktop application
├── core/                Portable calculation engine
└── infiltratr-common/   Exact shared Common gitlink

tests/                   Core regression tests
docs/                    Maintained engineering/design documentation
```

## Release direction

The project uses `main` as its working branch and follows the Infiltrator release discipline: test the exact source commit, build from that commit, and publish immutable release identities.

The packaged form is a generic Debian package suitable for Infiltrator Repository and, ultimately, Infiltrator Mint.

## Licence

Copyright © 2026 Shannon Smith.

Infiltrator Calc is licensed under the GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`).
