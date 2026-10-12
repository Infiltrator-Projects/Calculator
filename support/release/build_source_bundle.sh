#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 VERSION COMMIT_SHA OUTPUT.tar.gz" >&2
  exit 2
fi

version="$1"
commit_sha="$2"
output="$3"
workspace="${GITHUB_WORKSPACE:-$(pwd)}"
work_root="${RUNNER_TEMP:-/tmp}/calculator-source"
root="$work_root/Calculator-${version}"

rm -rf "$work_root"
mkdir -p "$root" "$(dirname "$output")"
rsync -a \
  --exclude='.git' \
  --exclude='build-*' \
  --exclude='dist' \
  "$workspace/" "$root/"

epoch="$(git -C "$workspace" show -s --format=%ct "$commit_sha")"
find "$root" -exec touch -h -d "@$epoch" {} +
tar --sort=name \
  --mtime="@$epoch" \
  --owner=0 --group=0 --numeric-owner \
  -C "$(dirname "$root")" \
  -cf - "$(basename "$root")" \
  | gzip -n > "$output"

test -s "$output"
echo "Deterministic source bundle: $output"
