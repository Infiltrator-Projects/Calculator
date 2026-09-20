<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# UI Parity Contract

Calculator has one interaction state machine rendered by three native shells. Linux GTK and Windows Win32 consume it directly; iPhone SwiftUI reaches it through a thin Objective-C++ bridge. None is an independent calculator implementation.

## Shared ownership

`src/ui/calculator_ui_contract.hpp` owns:

- Standard, Scientific and Programmer mode names;
- calculator button labels, order, role and command identity;
- the Standard MC/MR/MS/M+/M− memory strip and desktop keypad layouts;
- shared logical desktop sizing/spacing metrics and responsive breakpoints; and
- command-to-insertion mappings and Programmer selector classification.

`src/ui/calculator_ui_controller.*` owns:

- active calculator mode;
- expression, result and status state;
- Scientific DEG/RAD/GRAD, 2nd, HYP and F-E state;
- Programmer radix, width and signedness;
- memory, variables and history through `Session`;
- command enablement; and
- command dispatch and calculator interaction behaviour.

The logical metrics are platform-neutral contract values. Standard mode has a compact 480-unit preferred/minimum desktop height; Scientific and Programmer use the extended 610-unit preferred height with a 520-unit minimum. Both GTK and Win32 consume those mode-aware values rather than inventing platform-local heights. Win32 performs DPI conversion when mapping them to physical coordinates; GTK renders them through its native layout system.

## Platform-shell rule

`src/app/main.cpp` and `src/app/windows_main.cpp` may own platform mechanics: widgets/HWNDs, GTK CSS/GDI rendering, font discovery, DPI handling, keyboard/window integration, accessibility plumbing and host-theme observation/persistence.

They must not define competing calculator button arrays, mode names, calculation state machines or command semantics. A change to shared desktop behaviour is made in the contract/controller first and then rendered by both shells.

## Common ownership

Common owns product-neutral Design v1 semantics, including Day/Night palette values and typography roles. Calculator's desktop theme adapter consumes that contract directly.

Platform code owns only System appearance detection, persistence of the user's theme preference and toolkit-specific rendering.

## iPhone boundary

iPhone is not forced through desktop geometry, but it does use the same Calculator controller. SwiftUI owns touch-native view composition and platform presentation; the Objective-C++ bridge forwards expression edits, mode selection and key commands to the controller and snapshots the resulting state.

For appearance, SwiftUI resolves System light/dark state but retrieves Day/Night semantic values from Common through the bridge. It must not contain a second hard-coded Common palette.

This is semantic parity rather than pixel/layout parity: calculator state and command rules have one authoritative implementation, while native touch composition remains an iPhone concern.

## Regression enforcement

`tests/test_desktop_layout.py` fails if a desktop shell stops consuming the shared contract/controller, reintroduces local calculator layout/state ownership, if iPhone bypasses the shared controller, or if iPhone reintroduces a private Common theme palette.

`calculator-ui-contract` validates the canonical desktop button/layout definition.

`calculator-ui-controller` exercises shared desktop behaviour independently of GTK and Win32, including arithmetic, explicit memory store/update, Scientific angle/2nd/HYP/F-E state, Programmer extended bitwise operations and cursor-aware editing.

Windows CI adds native runtime smoke tests for control creation and interaction. Linux builds/tests verify the GTK adapter. iOS CI builds both the Simulator and unsigned ARM64 device applications against the shared C++ core and Common sources.

## Parity definition

Parity means that shared behaviour has one authoritative owner and each platform renders or adapts that behaviour without redefining it. It does not require identical platform widgets, typography rasterisation, window metrics or touch/desktop interaction mechanics.
