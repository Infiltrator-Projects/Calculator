#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 PACKAGE.deb" >&2
  exit 2
fi

package="$1"
[[ -f "$package" ]] || { echo "missing Debian package: $package" >&2; exit 1; }

sudo dpkg -i "$package"
sudo fc-cache -f

dbus-run-session -- xvfb-run -a bash -s -- <<'INNER'
set -euo pipefail

measure_idle_cpu() {
  label="$1"
  shift
  log="/tmp/calculator-idle-${label}.log"

  # Calculator-owned GTK warnings are product defects.  Keep them fatal while
  # disabling only the absent accessibility bus in this headless environment.
  env GTK_A11Y=none G_DEBUG=fatal-warnings "$@" infiltrator-calc >"$log" 2>&1 &
  pid=$!
  sleep 5

  if ! kill -0 "$pid" 2>/dev/null; then
    cat "$log"
    echo "Linux Calculator exited during ${label} idle CPU smoke test." >&2
    exit 1
  fi

  start_ticks=$(awk '{print $14 + $15}' "/proc/$pid/stat")
  sleep 4
  end_ticks=$(awk '{print $14 + $15}' "/proc/$pid/stat")
  hz=$(getconf CLK_TCK)
  delta=$((end_ticks - start_ticks))
  limit=$((hz * 4 / 10))

  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true

  echo "Linux ${label} idle CPU ticks over 4 seconds: $delta (10% one-core limit: $limit)"
  if (( delta > limit )); then
    cat "$log"
    if [[ "$label" == "gl" ]] &&
       grep -Eq 'DRI3 error: Could not get DRI3 device|Ensure your X server supports DRI3' "$log"; then
      echo "::warning::Hosted X display has no usable DRI3 device; explicit GL startup remains diagnostic only."
    else
      echo "Linux Calculator consumed excessive CPU while idle with ${label} renderer selection." >&2
      exit 1
    fi
  fi
}

measure_idle_cpu default
measure_idle_cpu cairo GSK_RENDERER=cairo
measure_idle_cpu gl GSK_RENDERER=gl
INNER
