<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# UI Parity Contract

Calculator has one interaction state machine rendered by three native shells. Linux GTK and Windows Win32 consume it directly; iPhone SwiftUI reaches it through a thin Objective-C++ bridge. None is an independent calculator implementation.

## Shared ownership

`src/ui/calculator_ui_contract.hpp` owns:

- Standard, Scientific and Programmer mode names;
- calculator button labels, order, role and command identity;
- the Standard MC/MR/MS/M+/M− memory strip and desktop keypad layouts, including distinct CE (current entry) and C (whole calculation) actions;
- shared logical desktop sizing/spacing metrics and responsive breakpoints; and
- command-to-insertion mappings and Programmer selector classification.

`src/ui/calculator_ui_controller.*` owns:

- active calculator mode;
- expression, result and status state;
- Scientific DEG/RAD/GRAD, 2nd, HYP and F-E state;
- Programmer radix, width and signedness;
- memory, variables and history through `Session`, including a newest-first multi-memory stack with indexed recall/delete, per-entry history deletion and a shared unlimited-or-bounded retention preference;
- command enablement;
- Scientific live result preview and case-tolerant built-in identifier/function completion; and
- command dispatch and calculator interaction behaviour, including Standard CE semantics that discard only the current operand while preserving the pending expression and repeated Equals semantics that reuse the previous top-level operation.

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

Windows CI adds native runtime smoke tests for control creation and interaction. Linux builds/tests verify the GTK adapter. Linux responsive layout is resize-event-driven through GtkWindow size-property notifications; it must not use a permanent frame-clock callback while the calculator is idle. Programmer Bases/Bits is a native interactive surface on both desktop shells rather than a platform-specific informational dialog; the initial empty Programmer state represents the zero bit-pattern consistently, and XNOR, arithmetic-right shift, modulo and byte-order reversal are dispatched through the shared Controller on Linux, Windows and iPhone. iOS CI builds both the Simulator and unsigned ARM64 device applications against the shared C++ core and Common sources.

Scientific Tab completion is a shared controller operation. GTK and Win32 only forward the key and restore the returned cursor; candidate discovery comes from the Scientific function catalogue, canonical constants and Session-owned variables/functions. Built-in functions/constants match completion prefixes case-insensitively, while user-owned identifiers retain exact spelling. Scientific live preview follows the same side-effect-free Session path on every shell, so typing an assignment or function definition cannot commit it before Equals. Mode, Scientific angle unit, Programmer radix/width/signedness, Scientific precision, display preferences and history retention are durable shared semantic state; native shells persist those changes immediately through the same Controller-owned document rather than maintaining platform-specific semantic copies.

Scientific calculation precision is one Controller setting with a 16..1000 digit range. GTK exposes the full range through Preferences; Win32 and iPhone expose native precision presets while still calling the same setter. Reusable variables, custom functions, precision and presentation preferences serialize through one versioned Controller document; Linux/XDG, Windows/Registry and iPhone/UserDefaults are storage adapters only.

## Parity definition

Parity means that shared behaviour has one authoritative owner and each platform renders or adapts that behaviour without redefining it. It does not require identical platform widgets, typography rasterisation, window metrics or touch/desktop interaction mechanics.


## Advanced Tools parity

All three release shells expose the same fourteen-item Tools catalogue from `calculator::tools::catalog()` and call `calculator::tools::evaluate()` for results. Platform code must not reproduce engineering formulae, unit factors, CIDR arithmetic, statistics, root finding, exact arithmetic, complex arithmetic, financial calculations or number-utility logic.

Linux uses a GTK workbench and Cairo graph rendering. Windows uses a native Win32 workbench and GDI graph rendering. iPhone uses a SwiftUI sheet and Canvas graph rendering through the Objective-C++ bridge. Graph data curves and tool-result text use Common's semantic text role so they resolve light on Night surfaces and dark on Day surfaces; graph axes retain the subdued status-border role. Shared tool output uses LF internally, while the Win32 shell normalizes it to CRLF before assigning multiline EDIT text so structured results remain line-separated. These presentations may differ in layout, but tool names, prompts/examples, evaluation semantics, text results and graph samples originate in the shared core.
