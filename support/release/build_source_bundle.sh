#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Package exact Git objects; local edits and untracked runner files are not inputs.
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 VERSION COMMIT_SHA OUTPUT.tar.gz" >&2
  exit 2
fi

version="$1"
commit_sha="$2"
output="$3"
workspace="${GITHUB_WORKSPACE:-$(pwd)}"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Invalid version' >&2; exit 1; }
[[ "$commit_sha" =~ ^[0-9a-f]{40}$ ]] || { echo 'Expected a full source commit SHA' >&2; exit 1; }
commit_sha="$(git -C "$workspace" rev-parse --verify "$commit_sha^{commit}")"
[[ "$(git -C "$workspace" show "$commit_sha:VERSION")" = "$version" ]] || {
  echo 'Requested version does not match the committed VERSION' >&2
  exit 1
}

work_root="$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/calculator-source.XXXXXX")"
temporary_output=''
cleanup() {
  rm -rf -- "$work_root"
  [[ -z "$temporary_output" ]] || rm -f -- "$temporary_output"
}
trap cleanup EXIT
root="$work_root/Calculator-$version"
mkdir -p "$root" "$(dirname "$output")"

archive_repository() {
  local repository="$1" revision="$2" destination="$3"
  local record metadata relative child expected tree_file
  git -C "$repository" archive --format=tar "$revision" | tar -xf - -C "$destination"
  tree_file="$(mktemp "$work_root/tree.XXXXXX")"
  git -C "$repository" ls-tree -rz "$revision" > "$tree_file"
  while IFS= read -r -d '' record; do
    metadata="${record%%$'\t'*}"
    [[ "$metadata" == '160000 commit '* ]] || continue
    relative="${record#*$'\t'}"
    expected="${metadata##* }"
    child="$repository/$relative"
    [[ -e "$child/.git" ]] &&
      [[ "$(git -C "$child" rev-parse HEAD)" = "$expected" ]] || {
      printf 'Submodule does not match committed gitlink: %s\n' "$child" >&2
      return 1
    }
    mkdir -p "$destination/$relative"
    archive_repository "$child" "$expected" "$destination/$relative"
  done < "$tree_file"
  rm -f -- "$tree_file"
}
archive_repository "$workspace" "$commit_sha" "$root"

epoch="$(git -C "$workspace" show -s --format=%ct "$commit_sha")"
find "$root" -exec touch -h -d "@$epoch" {} +
temporary_output="$(mktemp "$output.tmp.XXXXXX")"
tar --sort=name --mtime="@$epoch" --owner=0 --group=0 --numeric-owner \
  -C "$work_root" -cf - "Calculator-$version" | gzip -n > "$temporary_output"
test -s "$temporary_output"
mv -f -- "$temporary_output" "$output"
temporary_output=''
echo "Deterministic committed source bundle: $output"
