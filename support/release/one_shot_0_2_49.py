#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[2]
version = root / "VERSION"
if version.read_text(encoding="utf-8").strip() != "0.2.48":
    raise SystemExit("Calculator VERSION moved; refusing stale one-shot release advance")
version.write_text("0.2.49\n", encoding="utf-8")

debian = root / "debian/changelog"
debian_text = debian.read_text(encoding="utf-8")
if not debian_text.startswith("calculator (0.2.48) "):
    raise SystemExit("Unexpected Debian changelog head")
debian_entry = """calculator (0.2.49) unstable; urgency=medium

  * Publish the post-0.2.48 native filesystem implementation as a new immutable release.
  * Keep Calculator exclusively at /Programs/calculator with no /usr/bin compatibility entry.
  * Keep standalone Debian, source and aggressive-native installation aligned with the InfiltratorOS skeleton and native PATH contract.

 -- Shannon Smith <infiltratr@yandex.com>  Sat, 17 Oct 2026 16:08:00 +1100

"""
debian.write_text(debian_entry + debian_text, encoding="utf-8")

changelog = root / "CHANGELOG.md"
changelog_text = changelog.read_text(encoding="utf-8")
anchor = "## 0.2.48 - 2026-10-17\n"
if anchor not in changelog_text or "## 0.2.49 - 2026-10-17" in changelog_text:
    raise SystemExit("Unexpected human changelog state")
changelog_entry = """## 0.2.49 - 2026-10-17

- Publish the product changes made after the immutable 0.2.48 release instead of mutating that release identity.
- Keep `/Programs/calculator` as Calculator's sole Linux executable location with no `/usr/bin` compatibility entry.
- Keep CMake/source installation, Debian packaging, desktop launch, package qualification and the aggressive hardware-native installer on the same native filesystem contract.

"""
changelog.write_text(changelog_text.replace(anchor, changelog_entry + anchor, 1), encoding="utf-8")

metainfo = root / "debian/calculator.metainfo.xml"
metainfo_text = metainfo.read_text(encoding="utf-8")
releases = "  <releases>\n"
if releases not in metainfo_text or 'release version="0.2.49"' in metainfo_text:
    raise SystemExit("Unexpected AppStream release state")
release = '    <release version="0.2.49" date="2026-10-17"><description><p>Publish the post-0.2.48 native filesystem implementation with Calculator exclusively under /Programs and all standalone/native installation paths aligned to that contract.</p></description></release>\n'
metainfo.write_text(metainfo_text.replace(releases, releases + release, 1), encoding="utf-8")
