#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The same immutable verified font inventory as native InfiltratorOS.
set -eu

REVISION="66c17fe9ea3ea9678b4028ffea1862a37d488f79"
BASE="https://raw.githubusercontent.com/Infiltrator-Projects/InfiltratorOS/${REVISION}/assets/fonts"
WORK="${DERIVED_FILE_DIR}/calculator-extended-fonts"
DEST="${TARGET_BUILD_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}"
mkdir -p "${WORK}" "${DEST}"
MANIFEST="${WORK}/SHA256SUMS"
curl --fail --location --retry 3 --silent --show-error \
    "${BASE}/SHA256SUMS" --output "${MANIFEST}"

[ "$(wc -l < "${MANIFEST}" | tr -d ' ')" -eq 23 ] || {
    echo "error: canonical 23-face MB/Daimler manifest is incomplete" >&2
    exit 1
}

for filename in \
    mb_corpo_s_text_regular.ttf \
    mb_corpo_s_text_bold.ttf \
    mb_corpo_a_title_cond_regular.ttf \
    mb_corpo_s_text_cond_bold.ttf \
    mb_corpo_a_screen_2_cond_regular.ttf
do
    checksum="$(awk -v font="${filename}" '$2 == font { print $1 }' "${MANIFEST}")"
    [ "${#checksum}" -eq 64 ] || {
        echo "error: missing SHA-256 for ${filename}" >&2
        exit 1
    }
    case "${checksum}" in *[!0-9a-f]*)
        echo "error: malformed SHA-256 for ${filename}" >&2; exit 1 ;;
    esac
    staged="${WORK}/${filename}"
    curl --fail --location --retry 3 --silent --show-error \
        "${BASE}/${filename}" --output "${staged}"
    actual="$(shasum -a 256 "${staged}" | awk '{print $1}')"
    [ "${actual}" = "${checksum}" ] || {
        echo "error: verified MB Corpo font mismatch: ${filename}" >&2
        exit 1
    }
    cp "${staged}" "${DEST}/${filename}"
done
