#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Test exact committed source packaging without Python.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
packager="$root/support/release/build_source_bundle.sh"
work=$(mktemp -d /tmp/calculator-source-fixture.XXXXXXXX)
trap 'rm -rf -- "$work"' EXIT
die() { printf 'source-bundle test: %s\n' "$*" >&2; exit 1; }
git_init() {
  mkdir -p "$1"
  git -C "$1" init -q
  git -C "$1" config user.name 'Source fixture'
  git -C "$1" config user.email 'fixture@example.invalid'
  git -C "$1" config commit.gpgsign false
}
git_commit() {
  git -C "$1" add .
  git -C "$1" commit -qm 'Fixture source'
}
scratch="$work/scratch"
mkdir -p "$scratch/calculator-source"
printf 'unrelated caller state' > "$scratch/calculator-source/keep"
leaf="$work/leaf origin"
dependency="$work/dependency origin"
project="$work/project checkout"
git_init "$leaf"
printf 'committed leaf' > "$leaf/leaf.txt"
git_commit "$leaf"
git_init "$dependency"
printf 'committed dependency' > "$dependency/dependency.txt"
git_commit "$dependency"
git -C "$dependency" -c protocol.file.allow=always submodule add -q "$leaf" 'nested leaf'
git_commit "$dependency"
git_init "$project"
printf '1.2.3\n' > "$project/VERSION"
printf 'committed product' > "$project/source.txt"
git_commit "$project"
git -C "$project" -c protocol.file.allow=always submodule add -q "$dependency" src/dependency
git_commit "$project"
revision=$(git -C "$project" rev-parse HEAD)
git -C "$project" -c protocol.file.allow=always submodule update --init --recursive
child="$project/src/dependency"
printf 'uncommitted edit' > "$project/source.txt"
printf 'must not ship' > "$project/private-runner-file"
printf 'uncommitted dependency' > "$child/dependency.txt"
printf 'must not ship' > "$child/private-child-file"
export GITHUB_WORKSPACE="$project" RUNNER_TEMP="$scratch"
first="$work/first.tar.gz"
second="$work/second.tar.gz"
bash "$packager" 1.2.3 "$revision" "$first" >"$work/first.log" 2>&1 &
one=$!
bash "$packager" 1.2.3 "$revision" "$second" >"$work/second.log" 2>&1 &
two=$!
wait "$one" || { cat "$work/first.log" >&2; die 'first parallel archive failed'; }
wait "$two" || { cat "$work/second.log" >&2; die 'second parallel archive failed'; }
cmp "$first" "$second" || die 'archives changed under concurrency'
test "$(tar -xzOf "$first" Calculator-1.2.3/source.txt)" = 'committed product' ||
  die 'uncommitted product code leaked into archive'
test "$(tar -xzOf "$first" Calculator-1.2.3/src/dependency/dependency.txt)" = 'committed dependency' ||
  die 'uncommitted dependency leaked'
test "$(tar -xzOf "$first" 'Calculator-1.2.3/src/dependency/nested leaf/leaf.txt')" = 'committed leaf' ||
  die 'nested pinned Git module not included'
if tar -tzf "$first" | grep -E 'private-|/\.git(/|$)' >/dev/null; then
  die 'private files or Git metadata shipped'
fi
baseline=$(sha256sum "$first" | cut -d' ' -f1)
if bash "$packager" 9.9.9 "$revision" "$first" >"$work/fail.log" 2>&1; then
  die 'wrong committed release version was accepted'
fi
test "$(sha256sum "$first" | cut -d' ' -f1)" = "$baseline" ||
  die 'failed packaging overwrote existing archive'
git -C "$child" checkout -q HEAD^
if bash "$packager" 1.2.3 "$revision" "$first" >"$work/fail.log" 2>&1; then
  die 'mismatched submodule checkout was accepted'
fi
test "$(sha256sum "$first" | cut -d' ' -f1)" = "$baseline" ||
  die 'mismatched submodule overwrote existing archive'
git -C "$project" submodule deinit -f src/dependency >/dev/null
if bash "$packager" 1.2.3 "$revision" "$first" >"$work/fail.log" 2>&1; then
  die 'missing submodule was accepted'
fi
test "$(sha256sum "$first" | cut -d' ' -f1)" = "$baseline" ||
  die 'failed checkout overwrote existing archive'
test "$(cat "$scratch/calculator-source/keep")" = 'unrelated caller state' ||
  die 'packager touched caller sentinel'
test "$(find "$scratch" -mindepth 1 -maxdepth 1 -print | wc -l)" -eq 1 ||
  die 'packager leaked its working directory'
test -z "$(find "$work" -name '*.tmp.*' -print -quit)" ||
  die 'packager leaked a temporary output'
printf 'Calculator committed source bundle: native regression tests passed\n'
