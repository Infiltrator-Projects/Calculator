#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
: "${EXPECTED_SHA:?}" "${VERSION:?}" "${RUNNER_TEMP:?}"
test "$(git rev-parse HEAD)" = "$EXPECTED_SHA"
test "$(tr -d '\r\n' < VERSION)" = "$VERSION"
tag="v$VERSION"
mapfile -t assets < dist/release-assets.txt
mkdir -p release-assets
for asset in "${assets[@]}"; do cp -- "$asset" release-assets/; done
expected_assets="$(printf '%s\n' "${assets[@]##*/}" | sort)"
test "$(find release-assets -maxdepth 1 -type f -printf '%f\n' | sort)" = "$expected_assets"
(cd release-assets && sha256sum -c SHA256SUMS.txt)
git fetch origin --tags
if git rev-parse -q --verify "refs/tags/$tag^{commit}" >/dev/null; then
    test "$(git rev-parse "$tag^{commit}")" = "$EXPECTED_SHA"
fi
# Creation may time out after succeeding remotely. Re-read durable state before
# retrying. Public assets are compared and never uploaded or edited in place.
work="$(mktemp -d "$RUNNER_TEMP/calculator-release.XXXXXX")"
trap 'rm -rf -- "$work"' EXIT
state="$work/state"
for attempt in 1 2 3; do
    if gh release view "$tag" --json isDraft,isPrerelease,targetCommitish > "$state"; then
        break
    fi
    if gh release create "$tag" --target "$EXPECTED_SHA" --draft \
        --title "Calculator $VERSION" --generate-notes; then
        gh release view "$tag" --json isDraft,isPrerelease,targetCommitish > "$state"
        break
    fi
    test "$attempt" != 3
    sleep "$attempt"
done
test "$(jq -r .isPrerelease "$state")" = false
test "$(jq -r .targetCommitish "$state")" = "$EXPECTED_SHA"
draft="$(jq -r .isDraft "$state")"
if [[ "$draft" == true ]]; then
    gh release upload "$tag" release-assets/* --clobber
fi
verify_remote() {
    local destination
    destination="$(mktemp -d "$work/download.XXXXXX")"
    gh release download "$tag" --dir "$destination"
    diff -qr release-assets "$destination"
    rm -rf "$destination"
}
verify_remote
# Publish an annotated immutable tag only after staged bytes are verified.
if ! git rev-parse -q --verify "refs/tags/$tag^{commit}" >/dev/null; then
    git config user.name github-actions[bot]
    git config user.email 41898282+github-actions[bot]@users.noreply.github.com
    git tag --annotate "$tag" "$EXPECTED_SHA" --message "Calculator $VERSION"
    git push origin "refs/tags/$tag"
fi
if [[ "$draft" == true ]]; then
    gh release edit "$tag" --draft=false --latest
fi
verify_remote
git fetch origin "refs/tags/$tag:refs/tags/$tag"
test "$(git rev-parse "$tag^{commit}")" = "$EXPECTED_SHA"
echo "Release $tag is published from exact qualified bytes for $EXPECTED_SHA."
