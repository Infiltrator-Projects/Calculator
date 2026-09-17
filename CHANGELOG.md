# Changelog

## 0.1.6

- Reworked the calculator shell around the canonical Infiltrator Design v1 graphite/silver tokens.
- Increased control size and spacing and separated number, operator, utility, clear and primary-action treatments.
- Replaced the old cycling MODE button with direct Standard, Scientific and Programmer mode selection.
- Added visible selected states for Programmer base, integer width and signedness controls.
- Added a Windows x64 build/test path alongside the Linux Debian build.
- Release publication now requires both Linux and Windows to succeed and publishes Debian, Windows executable and portable Windows runtime assets together.

## 0.1.5

- Fixed Programmer shift counts so they are interpreted as decimal quantities regardless of the selected number base.
- Kept 8/16/32/64-bit width controls distinct from numeric keypad input.

## 0.1.4

- Added Programmer mode with binary, octal, decimal and hexadecimal bases.
- Added 8/16/32/64-bit widths, signed/unsigned display, bitwise operations and shifts.
- Added Programmer-mode regression tests.

## 0.1.3

- Added reusable calculation variables with `name=expression` assignment.
- Added a shared calculation-session layer for variables, memory and bounded history.
- Added a desktop calculation-history window with clear-history support.
- Added regression coverage for variable evaluation, session memory and history retention.
- Kept the existing Standard and Scientific calculator behaviour intact.
- Finalised the release regression test include and release verification path.
