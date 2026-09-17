<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Initial Implementation

The first implementation is intentionally small enough to qualify quickly.

Current engine semantics:

- decimal numeric literals accepted by the C library parser;
- `+`, `-`, `*`, `/` and `^` operators;
- parentheses;
- unary plus and minus;
- explicit division-by-zero and invalid-input errors;
- finite-result rejection for non-finite operations.

This is a foundation, not the final numeric model. The roadmap tracks expansion into integer domains, scientific functions, units, programmer calculations and advanced mathematics.
