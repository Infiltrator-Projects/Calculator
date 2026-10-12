#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 VERSION SOURCE.tar.gz OUTPUT.run" >&2
  exit 2
fi

version="$1"
source_archive="$2"
installer="$3"

test -s "$source_archive"
mkdir -p build-installer-tools "$(dirname "$installer")"

g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -static \
  support/tools/native_installer.cpp \
  -o build-installer-tools/calculator-native-installer

g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
  support/tools/build_native_installer.cpp \
  -o build-installer-tools/build-native-installer

build-installer-tools/calculator-native-installer --help >/dev/null
build-installer-tools/build-native-installer --help >/dev/null
build-installer-tools/build-native-installer \
  build-installer-tools/calculator-native-installer \
  "$source_archive" \
  "$installer"

chmod +x "$installer"
test -x "$installer"
"$installer" --help >/dev/null
echo "Native installer: $installer (Calculator $version)"
