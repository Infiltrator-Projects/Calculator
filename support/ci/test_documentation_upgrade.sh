#!/bin/sh
# Test the actual candidate package against its previous nested manual layout.
set -eu
[ "$(id -u)" -eq 0 ] || { echo 'Run in an isolated root or as root' >&2; exit 1; }
package=$(realpath "$1")
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT HUP INT TERM
old="$work/old"
docs="$old/System/Resources/Documentation/Calculator"
mkdir -p "$old/DEBIAN" "$docs"
printf 'Package: calculator\nVersion: 0.0.0\nArchitecture: amd64\nMaintainer: Fixture <fixture@example.invalid>\nDescription: isolated old manual layout\n' > "$old/DEBIAN/control"
printf '%%PDF-1.4 old manual' > "$docs/Calculator-Manual.pdf"
dpkg-deb --root-owner-group --build "$old" "$work/old.deb" >/dev/null
system="$work/system"
dpkg --root "$system" --unpack "$work/old.deb" >/dev/null
printf 'personal document' > "$system/System/Resources/Documentation/Personal.pdf"
dpkg --root "$system" --unpack "$package" >/dev/null
docs="$system/System/Resources/Documentation"
test ! -e "$docs/Calculator"
test "$(cat "$docs/Personal.pdf")" = 'personal document'
test "$(head -c 5 "$docs/Calculator.pdf")" = '%PDF-'
owner=$(dpkg-query --admindir "$system/var/lib/dpkg" --search /System/Resources/Documentation/Calculator.pdf)
test "$owner" = 'calculator: /System/Resources/Documentation/Calculator.pdf'
echo 'Calculator flat manual upgrade and old folder removal passed'
