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
  './Programs/calculator'
  './etc/profile.d/91-infiltrator-calculator-path.sh'
  './usr/share/applications/net.ssmith.calculator.desktop'
  './usr/share/icons/hicolor/scalable/apps/calculator.svg'
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

native_directories=(
  './Programs/'
  './System/Commands/'
  './System/Libraries/'
  './System/Devices/'
  './System/Filesystems/'
  './System/Kernel/Images/'
  './System/Kernel/Modules/'
  './System/Kernel/Firmware/'
  './System/Resources/Calendars/'
  './System/Resources/Clocks/'
  './System/Resources/Fonts/'
  './System/Resources/Locales/'
  './System/Resources/Icons/'
  './System/Resources/Themes/'
  './System/Preferences/'
  './Users/'
  './Volumes/'
  './Temporary/'
)
for path in "${native_directories[@]}"; do
  grep -Fqx "$path" "$listing" || {
    echo "Debian package is missing native filesystem directory: $path" >&2
    exit 1
  }
done

if grep -Eq '^\./usr/bin/(calculator|calculator-cli)$' "$listing"; then
  echo "Calculator must not install executables under /usr/bin." >&2
  exit 1
fi
if grep -Fqx './System/Programs/' "$listing"; then
  echo "The obsolete /System/Programs split must not return." >&2
  exit 1
fi

if grep -Eq '^\./usr/include/infiltratr/|^\./usr/lib/.*/libinfiltratr|/cmake/InfiltratrCommon/' "$listing"; then
  echo "Calculator package leaked Common development/runtime payloads." >&2
  cat "$listing" >&2
  exit 1
fi

echo "Debian package contract passed: $package"
