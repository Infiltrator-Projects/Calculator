<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Infiltrator Calc Architecture

Infiltrator Calc separates calculation semantics from the desktop interface.

The portable calculation core owns parsing, evaluation, numeric behaviour, units, variables, constants, history models and error semantics. The GTK application owns presentation, keyboard interaction, clipboard integration and desktop integration.

Infiltratr Common is consumed as an exact gitlink. Common provides reusable low-level facilities; calculator-specific mathematical semantics remain owned by this repository.

## Typography

The application prefers locally installed MB Corpo faces and falls back to the host desktop font stack when they are unavailable. Proprietary MB Corpo binaries are not redistributed by this repository.

## Design rule

A calculation must be represented as data and evaluated by the calculator engine. The GUI must never become an alternate calculation implementation.
