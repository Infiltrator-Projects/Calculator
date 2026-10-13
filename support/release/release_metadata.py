#!/usr/bin/env python3
"""Validate release metadata and decide whether the current run should publish.

VERSION is the only version authority. The Debian changelog, human changelog
and AppStream release list are deliberately redundant consumer metadata, so CI
checks that their newest entries agree before any expensive platform build.
Release publication is triggered by a forward VERSION change, never by commit
message wording.

Manual publication is recovery, not a second release path. An existing tag may
be reconciled only while the workflow is running at that tag's exact source
commit. If the tag was never created because the original publication stopped
early, recovery is allowed only from the commit that actually introduced the
new VERSION. This prevents a later same-version main commit from replacing a
published release's bytes while retaining the old immutable tag identity.
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


def version_tuple(value: str) -> tuple[int, int, int]:
    if not VERSION_RE.fullmatch(value):
        die(f"invalid semantic patch version: {value!r}")
    major, minor, patch = (int(part) for part in value.split("."))
    return major, minor, patch


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


def git_output(*args: str) -> str | None:
    completed = subprocess.run(
        ["git", *args],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    if completed.returncode != 0:
        return None
    value = completed.stdout.strip()
    return value or None


def version_at(revision: str) -> str | None:
    value = git_output("show", f"{revision}:VERSION")
    if value is None:
        return None
    value = value.strip()
    return value if VERSION_RE.fullmatch(value) else None


def previous_version(before: str) -> str | None:
    if not before or set(before) == {"0"}:
        return None
    return version_at(before)


def head_commit() -> str:
    value = git_output("rev-parse", "HEAD^{commit}")
    if value is None:
        die("unable to resolve the checked-out source commit")
    return value


def release_tag_commit(current: str) -> str | None:
    return git_output("rev-parse", f"refs/tags/v{current}^{{commit}}")


def validate_release_identity(current: str, *, manual: bool) -> None:
    """Prove that publication cannot change the source behind a version tag."""
    head = head_commit()
    tagged = release_tag_commit(current)

    if tagged is not None:
        if tagged != head:
            action = "manual reconciliation" if manual else "publication"
            die(
                f"{action} for v{current} must run at tagged source {tagged}, "
                f"not {head}"
            )
        return

    if not manual:
        # A normal forward-version push is the authority that may create the
        # tag for the first time. Existing-tag reuse was rejected above.
        return

    parent_version = version_at("HEAD^")
    if parent_version is None or parent_version == current:
        die(
            "manual publication without an existing tag is allowed only from "
            "the commit that introduced the current VERSION"
        )
    if version_tuple(parent_version) >= version_tuple(current):
        die(
            "manual publication requires a forward VERSION transition "
            f"({parent_version} -> {current})"
        )


def should_publish(event: str, before: str, manual_publish: bool, current: str) -> bool:
    if event == "push":
        previous = previous_version(before)
        if previous is None or previous == current:
            return False
        if version_tuple(current) <= version_tuple(previous):
            die(f"VERSION must move forward for release publication ({previous} -> {current})")
        validate_release_identity(current, manual=False)
        return True

    if event == "workflow_dispatch" and manual_publish:
        validate_release_identity(current, manual=True)
        return True

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
