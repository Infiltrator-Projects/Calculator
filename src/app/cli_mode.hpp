/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

namespace calculator::app {

/** Run Calculator's explicit terminal mode.
 *
 * This is an alternate operating mode of the single Calculator executable,
 * selected by `calculator /cli`. It is deliberately not exposed as a second
 * installed command or application identity.
 *
 * @param argc Number of arguments following `/cli`.
 * @param argv Arguments following `/cli`.
 * @return Process exit status.
 */
int run_cli_mode(int argc, char** argv);

} // namespace calculator::app
