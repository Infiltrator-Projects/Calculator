# Desktop UI parity contract

Infiltrator Calc has one calculator design and one calculator interaction model.

The Linux GTK shell and the native Windows shell are renderers. They are not
independent calculator implementations.

## Ownership

`src/ui/calculator_ui_contract.hpp` owns:

- Standard, Scientific and Programmer mode names;
- every calculator button label, order, role and command;
- the Standard memory strip and all keypad layouts;
- shared desktop window and spacing metrics; and
- command-to-insertion mappings and programmer selector classification.

`src/ui/calculator_ui_controller.*` owns:

- the active mode;
- expression, result and status state;
- DEG/RAD state;
- programmer base, width and signedness;
- memory, variables and history through the shared Session;
- command dispatch and calculator behaviour.

Infiltratr Common continues to own product-neutral design tokens such as the
graphite/silver palette, typography roles, radii and spacing vocabulary.

## Platform shell rule

`src/app/main.cpp` and `src/app/windows_main.cpp` may own only platform
mechanics: GTK widgets/CSS, Win32 HWND/GDI rendering, DPI handling, native
window integration and accessibility plumbing.

Platform shells must not define their own calculator button arrays, mode names,
calculator state machines or command semantics. A change to the calculator
layout or behaviour is made in the shared UI contract/controller first.

## Regression enforcement

`tests/test_desktop_layout.py` fails if either shell stops consuming the
shared contract/controller or reintroduces local calculator layouts/state.

`calculator-ui-contract` validates the canonical button/layout definition.

`calculator-ui-controller` exercises shared behaviour independently of GTK
and Win32, including arithmetic, memory, scientific mode, programmer mode and
cursor-aware editing.

The Windows runtime smoke test remains responsible for native rendering and
interaction integration. Linux build/tests verify the GTK adapter against the
same shared model.

This split is deliberate: parity is defined by shared state, commands, layout
roles and metrics, while each operating system remains free to render those
roles through its native toolkit.
