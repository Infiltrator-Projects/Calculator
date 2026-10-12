#!/usr/bin/env python3
"""Validate release metadata and decide whether the current push is a release.

VERSION is the only version authority.  The Debian changelog, human changelog
and AppStream release list are deliberately redundant consumer metadata, so CI
checks that their newest entries agree before any expensive platform build.
Release publication is triggered by an actual VERSION change, never by commit
message wording.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
DEBIAN_RE = re.compile(r"^infiltrator-calculator \(([^)]+)\)")
CHANGELOG_RE = re.compile(r"^## ([0-9]+\.[0-9]+\.[0-9]+)\b", re.MULTILINE)


def die(message: str) -> "NoReturn":
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


def git_output(*args: str) -> str:
    completed = subprocess.run(
        ["git", *args],
        cwd=ROOT,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return completed.stdout.strip()


def tag_exists(tag: str) -> bool:
    completed = subprocess.run(
        ["git", "rev-parse", "--quiet", "--verify", f"refs/tags/{tag}"],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return completed.returncode == 0


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


def should_release(event: str, before: str, manual_publish: bool, current: str) -> bool:
    requested = False
    if event == "push":
        previous = previous_version(before)
        requested = previous is not None and previous != current
    elif event == "workflow_dispatch":
        requested = manual_publish

    if not requested:
        return False

    tag = f"v{current}"
    if tag_exists(tag):
        print(f"{tag} already exists; publication is already complete")
        return False
    return True


def write_outputs(path: Path, current: str, release: bool) -> None:
    with path.open("a", encoding="utf-8") as stream:
        stream.write(f"version={current}\n")
        stream.write(f"release={'true' if release else 'false'}\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--event", default="")
    parser.add_argument("--before", default="")
    parser.add_argument("--manual-publish", action="store_true")
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()

    current = version()
    validate_metadata(current)
    release = should_release(args.event, args.before, args.manual_publish, current)

    print(f"Calculator version: {current}")
    print(f"Publish release: {'yes' if release else 'no'}")
    if args.github_output:
        write_outputs(args.github_output, current, release)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
