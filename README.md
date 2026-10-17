<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Calculator

**Project copyright:** © 1993-2026 Shannon Smith

Calculator is a native cross-platform calculator for the software family, with first-class Linux, Windows and iPhone interfaces.

The project is deliberately larger in ambition than a four-function calculator, but the implementation grows in layers. The calculation engine is independent of the graphical interface, while a shared platform-neutral controller defines calculator commands and interaction state once for every native shell. Desktop layout is additionally defined by a shared logical UI contract.

**Version authority:** [`VERSION`](VERSION); CI rejects release-metadata drift before any platform build
**Language:** C++17 shared calculation core, GTK4 Linux shell, native Win32 Windows shell, SwiftUI iPhone shell with Objective-C++ bridge
**Shared foundation:** Common 1.19.46
**Design contract:** shared Design v1
**Licence:** GPL-3.0-or-later

## Engineering ethos

Calculator is a first-principles engineering project. The question is not simply how to reproduce an existing calculator, but what a calculator should be when it is designed today with the strongest available mathematics, numerical methods, human-interface research, platform capabilities and software engineering practice.

## Linux native installation

Calculator's Linux executable has one authoritative home: `/Programs/calculator`. Debian packages, source/package builds and the hardware-native installer establish the native InfiltratorOS filesystem skeleton if it is absent, install no Calculator executable or compatibility symlink under `/usr/bin`, and use an absolute native path in the desktop launcher. `/System/Commands` and `/Programs` are added to future login-shell PATHs by a package-owned profile bridge.

## Current capabilities

Calculator provides Standard, Scientific and Programmer modes, reusable variables and custom functions, structured history, multiple memory slots, arbitrary-precision Scientific calculation, Programmer fixed-width arithmetic, and fourteen advanced tool families across its native platform shells. Detailed ownership, numerical, portability and validation contracts are maintained under [docs/](docs/README.md).

## Release platforms

Releases are multi-platform by default from one versioned source:

- Linux x64: `calculator_<version>_amd64.deb` and `calculator-<version>-native-installer.run`; authoritative executable `/Programs/calculator`.
- Windows x64: `calculator_<version>_windows_x64.exe`, a native standalone Win32 executable.
- iPhone: `calculator_<version>_ios-device-unsigned.ipa` using the standard `Payload/Calculator.app` layout.
- Source: `calculator_<version>_source.tar.gz`, a deterministic source/dependency bundle.

The native Linux installer defaults to the aggressive profile: `-O3`, CPU-native tuning, LTO and measured two-pass PGO. `--system-package-mode` permits InfiltratorOS to run the same installer as root after its package transaction completes.

## Build

Clone recursively so the exact Common revision is available:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Calculator.git
cd Calculator
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Release direction

The project uses `main` as its working branch. `VERSION` is the single version authority; release preflight requires Debian changelog, human changelog and AppStream metadata to agree before publication.

## Licence

Copyright © 1993-2026 Shannon Smith.
Calculator is licensed under GPL-3.0-or-later.
