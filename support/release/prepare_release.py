#!/usr/bin/env python3
"""Prepare Calculator's redundant release metadata from one command.

VERSION remains the version authority. This helper exists solely to make the
human edit atomic in intent: it validates every expected old marker, builds all
new file contents in memory, then replaces the four release metadata files.
The independent release_metadata.py preflight remains authoritative in CI and
will reject an interrupted or manually inconsistent update.
"""

from __future__ import annotations

import argparse
import os
import re
import tempfile
from datetime import date
from email.utils import format_datetime
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[2]
VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
DEBIAN_RE = re.compile(r"^infiltrator-calculator \(([^)]+)\)")
CHANGELOG_RE = re.compile(r"^## ([0-9]+\.[0-9]+\.[0-9]+)\b", re.MULTILINE)
UNRELEASED_MARKER = "## Unreleased\n\nNo unreleased changes.\n"
APPSTREAM_MARKER = "  <releases>\n"


def semantic_tuple(value: str) -> tuple[int, int, int]:
    if not VERSION_RE.fullmatch(value):
        raise ValueError(f"not a strict semantic patch version: {value!r}")
    return tuple(int(part) for part in value.split("."))  # type: ignore[return-value]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def validate_current(current: str) -> None:
    debian = read("debian/changelog")
    debian_match = DEBIAN_RE.match(debian.splitlines()[0] if debian else "")
    if not debian_match or debian_match.group(1) != current:
        raise ValueError("debian/changelog is not aligned with the current VERSION")

    changelog = read("CHANGELOG.md")
    changelog_match = CHANGELOG_RE.search(changelog)
    if not changelog_match or changelog_match.group(1) != current:
        raise ValueError("CHANGELOG.md is not aligned with the current VERSION")
    if UNRELEASED_MARKER not in changelog:
        raise ValueError("CHANGELOG.md has no canonical Unreleased marker")

    appstream = read("debian/infiltrator-calculator.metainfo.xml")
    first_release = re.search(r'<release version="([^"]+)"', appstream)
    if not first_release or first_release.group(1) != current:
        raise ValueError("AppStream metadata is not aligned with the current VERSION")
    if APPSTREAM_MARKER not in appstream:
        raise ValueError("AppStream metadata has no canonical releases marker")


def build_updates(
    new_version: str,
    release_date: str,
    debian_date: str,
    summary: str,
    bullets: list[str],
) -> dict[str, str]:
    current = read("VERSION").strip()
    semantic_tuple(current)
    if semantic_tuple(new_version) <= semantic_tuple(current):
        raise ValueError(f"new version {new_version} must be newer than {current}")
    try:
        date.fromisoformat(release_date)
    except ValueError as exc:
        raise ValueError("release date must be YYYY-MM-DD") from exc
    if not summary.strip():
        raise ValueError("release summary must not be empty")
    cleaned_bullets = [bullet.strip() for bullet in bullets if bullet.strip()]
    if not cleaned_bullets:
        raise ValueError("at least one non-empty release bullet is required")

    validate_current(current)

    changelog = read("CHANGELOG.md")
    changelog_entry = "\n## " + new_version + " — " + release_date + "\n\n"
    changelog_entry += "".join(f"- {bullet}\n" for bullet in cleaned_bullets)
    changelog_new = changelog.replace(
        UNRELEASED_MARKER,
        UNRELEASED_MARKER + changelog_entry,
        1,
    )

    debian = read("debian/changelog")
    debian_entry = f"infiltrator-calculator ({new_version}) unstable; urgency=medium\n\n"
    debian_entry += "".join(f"  * {bullet}\n" for bullet in cleaned_bullets)
    debian_entry += (
        "\n -- Shannon Smith <infiltratr@yandex.com>  "
        f"{debian_date}\n\n"
    )

    appstream = read("debian/infiltrator-calculator.metainfo.xml")
    escaped_summary = (
        summary.strip()
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
    )
    appstream_entry = (
        f'    <release version="{new_version}" date="{release_date}">\n'
        "      <description>\n"
        f"        <p>{escaped_summary}</p>\n"
        "      </description>\n"
        "    </release>\n"
    )
    appstream_new = appstream.replace(
        APPSTREAM_MARKER,
        APPSTREAM_MARKER + appstream_entry,
        1,
    )

    return {
        "VERSION": new_version + "\n",
        "CHANGELOG.md": changelog_new,
        "debian/changelog": debian_entry + debian,
        "debian/infiltrator-calculator.metainfo.xml": appstream_new,
    }


def replace_files(updates: dict[str, str]) -> None:
    staged: list[tuple[Path, Path]] = []
    try:
        for relative, content in updates.items():
            destination = ROOT / relative
            fd, temporary_name = tempfile.mkstemp(
                prefix=destination.name + ".",
                suffix=".tmp",
                dir=destination.parent,
                text=True,
            )
            temporary = Path(temporary_name)
            with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as stream:
                stream.write(content)
                stream.flush()
                os.fsync(stream.fileno())
            staged.append((temporary, destination))

        for temporary, destination in staged:
            os.replace(temporary, destination)
    finally:
        for temporary, _ in staged:
            temporary.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Advance VERSION, Debian, CHANGELOG and AppStream metadata together."
    )
    parser.add_argument("--version", required=True, help="new X.Y.Z version")
    parser.add_argument(
        "--date",
        default=date.today().isoformat(),
        help="release date in YYYY-MM-DD (default: today)",
    )
    parser.add_argument(
        "--debian-date",
        default=format_datetime(datetime.now().astimezone()),
        help="RFC 2822 Debian changelog timestamp",
    )
    parser.add_argument("--summary", required=True, help="one-line AppStream summary")
    parser.add_argument(
        "--bullet",
        action="append",
        default=[],
        help="release note bullet; repeat for multiple bullets",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="validate and show affected files without writing",
    )
    args = parser.parse_args()

    try:
        updates = build_updates(
            args.version,
            args.date,
            args.debian_date,
            args.summary,
            args.bullet,
        )
    except (OSError, ValueError) as exc:
        parser.error(str(exc))

    print(f"Prepare Calculator {args.version}:")
    for relative in updates:
        print(f"  {relative}")
    if args.dry_run:
        print("Dry run only; no files changed.")
        return 0

    replace_files(updates)
    print("Release metadata prepared. Run release_metadata.py or CI preflight to verify.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
