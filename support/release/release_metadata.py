#!/usr/bin/env python3
"""Validate release metadata and decide whether the current run should publish.

VERSION is the only version authority. The Debian changelog, human changelog
and AppStream release list are deliberately redundant consumer metadata, so CI
checks that their newest entries agree before any expensive platform build.
Release publication is triggered by an actual VERSION change, never by commit
message wording. An explicit manual publish may be used to reconcile a partial
or interrupted release without manufacturing a new source version.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import NoReturn

ROOT = Path(__file__).resolve().parents[2]
VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
DEBIAN_RE = re.compile(r"^infiltrator-calculator \(([^)]+)\)")
CHANGELOG_RE = re.compile(r"^## ([0-9]+\.[0-9]+\.[0-9]+)\b", re.MULTILINE)


def die(message: str) -> NoReturn:
    print(f"release metadata error: {message}", file=sys.stderr)
    raise SystemExit(1)


def read_text(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def version() -> str:
    value = read_text("VERSION").strip()
    if not VERSION_RE.fullmatch(value):
        die(f"VERSION is not a strict semantic patch version: {value!r}")
    return value


def validate_metadata(expected: str) -> None:
    debian_first = read_text("debian/changelog").splitlines()[0]
    match = DEBIAN_RE.match(debian_first)
    if not match or match.group(1) != expected:
        die(
            "debian/changelog newest version does not match VERSION "
            f"({match.group(1) if match else 'unparseable'} != {expected})"
        )

    changelog_match = CHANGELOG_RE.search(read_text("CHANGELOG.md"))
    if not changelog_match or changelog_match.group(1) != expected:
        die(
            "CHANGELOG.md newest release does not match VERSION "
            f"({changelog_match.group(1) if changelog_match else 'missing'} != {expected})"
        )

    try:
        root = ET.fromstring(read_text("debian/infiltrator-calculator.metainfo.xml"))
    except ET.ParseError as exc:
        die(f"AppStream XML is not well formed: {exc}")
    release = root.find("./releases/release")
    appstream_version = release.get("version") if release is not None else None
    if appstream_version != expected:
        die(
            "AppStream newest release does not match VERSION "
            f"({appstream_version or 'missing'} != {expected})"
        )

    # README formerly duplicated a literal source version and drifted. Keep the
    # authority explicit instead of adding another value that humans must bump.
    if "**Current source version:**" in read_text("README.md"):
        die("README.md must refer to VERSION as the authority, not duplicate it")


def previous_version(before: str) -> str | None:
    if not before or set(before) == {"0"}:
        return None
    completed = subprocess.run(
        ["git", "show", f"{before}:VERSION"],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    if completed.returncode != 0:
        return None
    value = completed.stdout.strip()
    return value if VERSION_RE.fullmatch(value) else None


def should_publish(event: str, before: str, manual_publish: bool, current: str) -> bool:
    if event == "push":
        previous = previous_version(before)
        return previous is not None and previous != current
    if event == "workflow_dispatch":
        return manual_publish
    return False


def write_outputs(path: Path, current: str, publish: bool) -> None:
    with path.open("a", encoding="utf-8") as stream:
        stream.write(f"version={current}\n")
        stream.write(f"release={'true' if publish else 'false'}\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--event", default="")
    parser.add_argument("--before", default="")
    parser.add_argument("--manual-publish", action="store_true")
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()

    current = version()
    validate_metadata(current)
    publish = should_publish(args.event, args.before, args.manual_publish, current)

    print(f"Calculator version: {current}")
    print(f"Publish release: {'yes' if publish else 'no'}")
    if args.github_output:
        write_outputs(args.github_output, current, publish)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
