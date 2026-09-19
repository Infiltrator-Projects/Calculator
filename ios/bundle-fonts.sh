#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

DESIGN_JSON="${SRCROOT}/../src/infiltratr-common/design/infiltrator-design-v1.json"

common_value() {
    /usr/bin/plutil -extract "$1" raw -o - "${DESIGN_JSON}"
}

SOURCE_REPOSITORY="$(common_value typography.assets.source_repository)"
SOURCE_COMMIT="$(common_value typography.assets.source_commit)"
ARCHIVE_PATH="$(common_value typography.assets.archive_path)"
ARCHIVE_SHA256="$(common_value typography.assets.archive_sha256)"

BRAND_FILE="$(common_value typography.font_files.brand_regular)"
UI_BOLD_FILE="$(common_value typography.font_files.ui_bold)"
UI_REGULAR_FILE="$(common_value typography.font_files.ui_regular)"

BRAND_SHA256="$(common_value typography.assets.file_sha256.brand_regular)"
UI_BOLD_SHA256="$(common_value typography.assets.file_sha256.ui_bold)"
UI_REGULAR_SHA256="$(common_value typography.assets.file_sha256.ui_regular)"

URL="https://raw.githubusercontent.com/${SOURCE_REPOSITORY}/${SOURCE_COMMIT}/${ARCHIVE_PATH}"
ARCHIVE="${DERIVED_FILE_DIR}/calculator-mb-corpo-fonts.tar.xz"
FONT_DIR="${DERIVED_FILE_DIR}/calculator-mb-fonts"
DEST="${TARGET_BUILD_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}"

mkdir -p "${DERIVED_FILE_DIR}" "${DEST}"
curl -fL --retry 3 --retry-delay 2 "${URL}" -o "${ARCHIVE}"

actual="$(shasum -a 256 "${ARCHIVE}" | awk '{print $1}')"
[ "${actual}" = "${ARCHIVE_SHA256}" ] || {
    echo "error: Calculator MB Corpo archive hash mismatch" >&2
    exit 1
}

rm -rf "${FONT_DIR}"
mkdir -p "${FONT_DIR}"
tar -xJf "${ARCHIVE}" -C "${FONT_DIR}"

check_font() {
    file="$1"
    expected="$2"
    [ -f "${FONT_DIR}/${file}" ] || {
        echo "error: Calculator font archive is missing ${file}" >&2
        exit 1
    }
    actual_font="$(shasum -a 256 "${FONT_DIR}/${file}" | awk '{print $1}')"
    [ "${actual_font}" = "${expected}" ] || {
        echo "error: Calculator font hash mismatch: ${file}" >&2
        exit 1
    }
}

check_font "${BRAND_FILE}" "${BRAND_SHA256}"
check_font "${UI_BOLD_FILE}" "${UI_BOLD_SHA256}"
check_font "${UI_REGULAR_FILE}" "${UI_REGULAR_SHA256}"

cp "${FONT_DIR}/${BRAND_FILE}" "${DEST}/"
cp "${FONT_DIR}/${UI_BOLD_FILE}" "${DEST}/"
cp "${FONT_DIR}/${UI_REGULAR_FILE}" "${DEST}/"
