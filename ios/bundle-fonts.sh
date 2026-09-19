#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

SOURCE_COMMIT=aa161e7342112beab8feb7669f072c870f742765
URL="https://raw.githubusercontent.com/Infiltrator-Projects/MBLINK/${SOURCE_COMMIT}/assets/fonts/mb-corpo-fonts.tar.xz"
ARCHIVE="${DERIVED_FILE_DIR}/calculator-mb-corpo-fonts.tar.xz"
FONT_DIR="${DERIVED_FILE_DIR}/calculator-mb-fonts"
DEST="${TARGET_BUILD_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}"

mkdir -p "${DERIVED_FILE_DIR}" "${DEST}"
curl -fL --retry 3 --retry-delay 2 "${URL}" -o "${ARCHIVE}"

actual="$(shasum -a 256 "${ARCHIVE}" | awk '{print $1}')"
[ "${actual}" = "bdb6063f838a7fab22b4d6b412170640c69511df53aa3dfa9a4ea8431c9d8274" ] || {
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

check_font mb_corpo_a_cond_regular.ttf c8bcd7e1a7d71169b38491d9b7c1ffe7ba7b46e888f0c1219931343a47bc0e05
check_font mb_corpo_s_bold.ttf d37ea986e2344d83390f94f170e6272b56efd00bfec808afe8314c4ca45d43b4
check_font mb_corpo_s_regular.ttf 94ede6629443c03d4362dcef425fb3ff520be5d654370021a34e81286804465c

cp "${FONT_DIR}/mb_corpo_a_cond_regular.ttf" "${DEST}/"
cp "${FONT_DIR}/mb_corpo_s_bold.ttf" "${DEST}/"
cp "${FONT_DIR}/mb_corpo_s_regular.ttf" "${DEST}/"
