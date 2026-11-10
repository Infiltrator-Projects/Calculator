#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Native release identity gate: one VERSION authority, immutable Git tags.
set -euo pipefail
root=${CALCULATOR_METADATA_ROOT:-$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)}
fail() { printf 'release metadata error: %s\n' "$1" >&2; exit 1; }
event= before= output= manual=0
while (($#)); do
  case "$1" in
    --event) (($# >= 2)) || fail 'missing event'; event=$2; shift 2 ;;
    --before) (($# >= 2)) || fail 'missing before'; before=$2; shift 2 ;;
    --github-output) (($# >= 2)) || fail 'missing output file'; output=$2; shift 2 ;;
    --manual-publish) manual=1; shift ;;
    *) fail "unknown argument $1" ;;
  esac
done
command -v git >/dev/null || fail 'git is required'
command -v dpkg >/dev/null || fail 'dpkg is required'
command -v xmlstarlet >/dev/null || fail 'xmlstarlet is required for strict XML validation'
strict_version() { [[ "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; }
current=$(tr -d '\r\n' < "$root/VERSION") || fail 'VERSION cannot be read'
strict_version "$current" || fail "VERSION is not a strict semantic patch version: $current"
first=$(head -n 1 "$root/debian/changelog") || fail 'Debian changelog missing'
debian_regex="^calculator[[:space:]]+\\(([^)]+)\\)"
[[ "$first" =~ $debian_regex ]] && [[ "${BASH_REMATCH[1]}" == "$current" ]] ||
  fail 'debian/changelog newest version does not match VERSION'
changelog=$(sed -nE 's/^## ([0-9]+\.[0-9]+\.[0-9]+)([[:space:]]|$).*/\1/p' "$root/CHANGELOG.md" | head -n 1) || fail 'human changelog missing'
[[ "$changelog" == "$current" ]] || fail 'CHANGELOG.md newest release does not match VERSION'
xml="$root/debian/calculator.metainfo.xml"
xmlstarlet val -e "$xml" >/dev/null 2>&1 || fail 'AppStream XML is not well formed'
appstream=$(xmlstarlet sel -t -v '/component/releases/release[1]/@version' "$xml") || fail 'AppStream XML cannot be read'
[[ "$appstream" == "$current" ]] || fail 'AppStream newest release does not match VERSION'
! grep -Fq '**Current source version:**' "$root/README.md" || fail 'README duplicates the VERSION authority'
git_output() { git -C "$root" "$@" 2>/dev/null || :; }
version_at() {
  local result
  result=$(git_output show "$1:VERSION")
  if strict_version "$result"; then printf '%s\n' "$result"; fi
}
head_commit=$(git_output rev-parse 'HEAD^{commit}')
[[ "$head_commit" =~ ^[0-9a-f]{40}$ ]] || fail 'unable to resolve checked-out source commit'
release_tag=$(git_output rev-parse -q --verify "refs/tags/v$current^{commit}")
release_identity() {
  local action=$1 introduced previous
  if [[ -n "$release_tag" ]]; then
    [[ "$release_tag" == "$head_commit" ]] ||
      fail "$action for v$current must run at tagged source $release_tag, not $head_commit"
    return 0
  fi
  introduced=$(git_output log -1 --format=%H HEAD -- VERSION)
  [[ -n "$introduced" ]] || fail 'untagged publication requires a committed VERSION transition'
  previous=$(version_at "$introduced^")
  [[ -n "$previous" ]] && dpkg --compare-versions "$previous" lt "$current" ||
    fail 'untagged publication requires a committed forward VERSION transition'
}
publish=false
if [[ "$event" == push ]]; then
  previous=
  if [[ -n "$before" && ! "$before" =~ ^0+$ ]]; then previous=$(version_at "$before"); fi
  if [[ -n "$previous" ]] && dpkg --compare-versions "$current" lt "$previous"; then
    fail "VERSION must move forward ($previous -> $current)"
  fi
  if [[ -n "$release_tag" && "$release_tag" != "$head_commit" && "$previous" == "$current" ]]; then
    publish=false
  else
    release_identity publication
    publish=true
  fi
elif [[ "$event" == workflow_dispatch && $manual == 1 ]]; then
  release_identity 'manual reconciliation'
  publish=true
fi
printf 'Calculator version: %s\nPublish release: %s\n' "$current" "$publish"
if [[ -n "$output" ]]; then
  printf 'version=%s\nrelease=%s\n' "$current" "$publish" >> "$output"
fi
