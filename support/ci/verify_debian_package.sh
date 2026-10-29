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
control_dir="$(mktemp -d)"
trap 'rm -f "$listing"; rm -rf "$control_dir"' EXIT
dpkg-deb --fsys-tarfile "$package" | tar -tf - > "$listing"
dpkg-deb -e "$package" "$control_dir"

required=(
  './Programs/calculator'
  './etc/profile.d/91-infiltrator-calculator-path.sh'
  './System/Commands/calculator-ensure-fonts'
  './usr/share/applications/net.ssmith.calculator.desktop'
  './usr/share/icons/hicolor/scalable/apps/calculator.svg'
  './usr/share/app-install/icons/calculator.svg'
  './System/Resources/Icons/calculator.svg'
  './usr/share/metainfo/calculator.metainfo.xml'
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
  './System/Resources/Documentation/'
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

if grep -Eq '^\./usr/lib/calculator/|^\./usr/share/doc/calculator/(BUILD-INFO|LICENSE)$' "$listing"; then
  echo "Calculator package retains deprecated application-owned Linux paths." >&2
  exit 1
fi
# Application documentation may contain the licence, but no build reports,
# compiler settings, source manuals or generated developer artefacts.
unexpected_docs="$(grep -E '^\./System/Resources/Documentation/Calculator/' "$listing" |
  grep -Ev '^\./System/Resources/Documentation/Calculator/(|LICENSE)
  echo "Calculator must not install executables under /usr/bin." >&2
  exit 1
fi
if grep -Fqx './System/Programs/' "$listing"; then
  echo "The obsolete /System/Programs split must not return." >&2
  exit 1
fi
if grep -Eq '(mb_corpo_|daimler_).*\.ttf$|^\./System/Resources/Fonts/[^/]+\.ttf$' "$listing"; then
  echo "Calculator must not package duplicate MB Corpo font binaries." >&2
  exit 1
fi

grep -Fq '/System/Commands/calculator-ensure-fonts' "$control_dir/postinst" || {
  echo "Calculator postinst must establish the verified 23-face native font cache." >&2
  exit 1
}
if grep -Eq '(rm|rmdir).*System/Resources/Fonts' "$control_dir"/*; then
  echo "Calculator uninstall scripts must never remove shared native fonts." >&2
  exit 1
fi

if grep -Eq '^\./usr/include/infiltratr/|^\./usr/lib/.*/libinfiltratr|/cmake/InfiltratrCommon/' "$listing"; then
  echo "Calculator package leaked Common development/runtime payloads." >&2
  cat "$listing" >&2
  exit 1
fi

echo "Debian package contract passed: $package"
 || true)"
if [[ -n "$unexpected_docs" ]]; then
  echo "Calculator package leaked developer/build documents:" >&2
  printf '%s\n' "$unexpected_docs" >&2
  exit 1
fi
if grep -Eq '^\./usr/bin/(calculator|calculator-cli)
  echo "Calculator must not install executables under /usr/bin." >&2
  exit 1
fi
if grep -Fqx './System/Programs/' "$listing"; then
  echo "The obsolete /System/Programs split must not return." >&2
  exit 1
fi
if grep -Eq '(mb_corpo_|daimler_).*\.ttf$|^\./System/Resources/Fonts/[^/]+\.ttf$' "$listing"; then
  echo "Calculator must not package duplicate MB Corpo font binaries." >&2
  exit 1
fi

grep -Fq '/System/Commands/calculator-ensure-fonts' "$control_dir/postinst" || {
  echo "Calculator postinst must establish the verified 23-face native font cache." >&2
  exit 1
}
if grep -Eq '(rm|rmdir).*System/Resources/Fonts' "$control_dir"/*; then
  echo "Calculator uninstall scripts must never remove shared native fonts." >&2
  exit 1
fi

if grep -Eq '^\./usr/include/infiltratr/|^\./usr/lib/.*/libinfiltratr|/cmake/InfiltratrCommon/' "$listing"; then
  echo "Calculator package leaked Common development/runtime payloads." >&2
  cat "$listing" >&2
  exit 1
fi

echo "Debian package contract passed: $package"
 "$listing"; then
  echo "Calculator must not install executables under /usr/bin." >&2
  exit 1
fi
if grep -Fqx './System/Programs/' "$listing"; then
  echo "The obsolete /System/Programs split must not return." >&2
  exit 1
fi
if grep -Eq '(mb_corpo_|daimler_).*\.ttf$|^\./System/Resources/Fonts/[^/]+\.ttf$' "$listing"; then
  echo "Calculator must not package duplicate MB Corpo font binaries." >&2
  exit 1
fi

grep -Fq '/System/Commands/calculator-ensure-fonts' "$control_dir/postinst" || {
  echo "Calculator postinst must establish the verified 23-face native font cache." >&2
  exit 1
}
if grep -Eq '(rm|rmdir).*System/Resources/Fonts' "$control_dir"/*; then
  echo "Calculator uninstall scripts must never remove shared native fonts." >&2
  exit 1
fi

if grep -Eq '^\./usr/include/infiltratr/|^\./usr/lib/.*/libinfiltratr|/cmake/InfiltratrCommon/' "$listing"; then
  echo "Calculator package leaked Common development/runtime payloads." >&2
  cat "$listing" >&2
  exit 1
fi

echo "Debian package contract passed: $package"
