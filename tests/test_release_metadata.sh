#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Isolated Git-history regression for native Calculator release metadata.
set -euo pipefail
repo=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
script="$repo/support/release/release_metadata.sh"
fixture=$(mktemp -d)
trap 'rm -rf -- "$fixture"' EXIT
mkdir -p "$fixture/debian"
git -C "$fixture" init -q
git -C "$fixture" config user.name 'Release fixture'
git -C "$fixture" config user.email 'fixture@example.invalid'
git -C "$fixture" config commit.gpgsign false
commit() {
  local version=$1 extra=${2:-}
  printf '%s\n' "$version" > "$fixture/VERSION"
  printf 'calculator (%s) unstable; urgency=medium\n' "$version" > "$fixture/debian/changelog"
  printf '## %s\n\nFixture release notes\n' "$version" > "$fixture/CHANGELOG.md"
  printf '<component><releases><release version="%s" /></releases></component>\n' "$version" > "$fixture/debian/calculator.metainfo.xml"
  printf 'Source from VERSION only\n' > "$fixture/README.md"
  printf '%s\n' "$extra" > "$fixture/source"
  git -C "$fixture" add .
  git -C "$fixture" commit -qm Fixture
  git -C "$fixture" rev-parse HEAD
}
check_yes() {
  CALCULATOR_METADATA_ROOT="$fixture" bash "$script" "$@" > "$fixture/result"
  grep -Fq 'Publish release: true' "$fixture/result"
}
check_no() {
  CALCULATOR_METADATA_ROOT="$fixture" bash "$script" "$@" > "$fixture/result"
  grep -Fq 'Publish release: false' "$fixture/result"
}
check_reject() {
  if CALCULATOR_METADATA_ROOT="$fixture" bash "$script" "$@" > "$fixture/result" 2>&1; then
    echo 'Unsafe release identity was accepted' >&2
    cat "$fixture/result" >&2
    exit 1
  fi
}
old=$(commit 1.0.0)
git -C "$fixture" tag v1.0.0
introduced=$(commit 1.0.1 'first candidate')
check_yes --event push --before "$old"
corrected=$(commit 1.0.1 'corrected before publication')
check_yes --event push --before "$introduced"
check_yes --event workflow_dispatch --manual-publish
check_no --event pull_request --before "$old"
git -C "$fixture" tag v1.0.1
check_yes --event push --before "$introduced"
check_yes --event workflow_dispatch --manual-publish
commit 1.0.1 'maintenance after publication' >/dev/null
check_no --event push --before "$corrected"
check_reject --event workflow_dispatch --manual-publish
prior=$(git -C "$fixture" rev-parse HEAD)
commit 0.9.0 rollback >/dev/null
check_reject --event push --before "$prior"
prior=$(git -C "$fixture" rev-parse HEAD)
commit 1.0.1 'attempted old identity reuse' >/dev/null
check_reject --event push --before "$prior"
printf '<component><releases><release version="invalid" /></releases></component>\n' > "$fixture/debian/calculator.metainfo.xml"
check_reject --event pull_request
echo 'Native Calculator release identity regression passed.'
