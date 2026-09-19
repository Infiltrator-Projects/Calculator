<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Security

## Supported source

Current `main` and the current release line are the primary maintained sources unless explicitly documented otherwise.

## Reporting

Do not publish sensitive exploit details in a public issue. Use GitHub private vulnerability reporting/security advisories when available.

Include the affected revision, platform/environment, reproduction, expected and observed behaviour, and known impact.

## Security-sensitive boundaries

Calculator is a local application with no project-owned network service, privileged daemon or mutation of arbitrary system state as part of normal calculation.

Security-sensitive inputs and boundaries include:

- expression and variable text supplied by the user or pasted from another source;
- numeric parsing and conversion at the Calculator/Common boundary;
- fixed-width Programmer parsing, shifts and arithmetic where malformed input must not trigger undefined behaviour;
- the Objective-C++ boundary between Swift and the shared C++ core;
- platform-owned configuration/persistence such as Linux preferences, Windows Registry state and iPhone AppStorage;
- local font/theme discovery and toolkit/platform callbacks; and
- package/release construction, where published artifacts must correspond to the qualified source revision.

Expression input is data, not executable code. Unsupported syntax or out-of-domain operations must fail as calculation errors rather than escaping into host command execution, memory corruption or fabricated results.

Compatibility identifiers and settings locations are treated as migration contracts. A product rename is not justification for silently abandoning existing user state or package upgrade continuity.

## Response and validation

Reproduce the issue at the narrowest owning layer, add deterministic regression coverage where practical, fix the underlying contract rather than only the visible symptom, and validate every affected platform or persistence boundary.

Memory/bounds issues, undefined arithmetic, malformed-input handling and release-integrity failures require the applicable sanitizer, platform or packaging evidence before publication.

## Disclosure

Security fixes should describe user impact and affected versions without publishing unnecessary exploitation detail before users have a corrected release path.
