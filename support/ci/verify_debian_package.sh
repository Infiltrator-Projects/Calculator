#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 PACKAGE.deb" >&2
  exit 2
fi

package="$1"
[[ -f "$package" ]] || { echo "missing Debian package: $package" >&2; exit 1; }

dpkg-deb --info "$package"
[[ "$(dpkg-deb -f "$package" Package)" == "calculator" ]] || {
  echo "unexpected Debian package identity" >&2
  exit 1
}

listing="$(mktemp)"
trap 'rm -f "$listing"' EXIT
dpkg-deb --fsys-tarfile "$package" | tar -tf - > "$listing"

required=(
  './usr/bin/infiltrator-calc'
  './usr/bin/infiltrator-calc-cli'
  './usr/share/applications/net.ssmith.infiltrator.calc.desktop'
  './usr/share/icons/hicolor/scalable/apps/infiltrator-calc.svg'
  './usr/share/app-install/icons/calculator.svg'
  './usr/share/metainfo/calculator.metainfo.xml'
  './usr/share/fonts/truetype/calculator/mb_corpo_a_cond_regular.ttf'
  './usr/share/fonts/truetype/calculator/mb_corpo_s_bold.ttf'
  './usr/share/fonts/truetype/calculator/mb_corpo_s_regular.ttf'
)
for path in "${required[@]}"; do
  grep -Fqx "$path" "$listing" || {
    echo "Debian package is missing required path: $path" >&2
    exit 1
  }
done

if grep -Eq '^\./usr/include/infiltratr/|^\./usr/lib/.*/libinfiltratr|/cmake/InfiltratrCommon/' "$listing"; then
  echo "Calculator package leaked Common development/runtime payloads." >&2
  cat "$listing" >&2
  exit 1
fi

echo "Debian package contract passed: $package"
