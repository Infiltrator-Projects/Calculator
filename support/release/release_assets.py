#!/usr/bin/env python3
"""Validate the release payload once and emit its manifest/checksums."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--dist", type=Path, default=Path("dist"))
    args = parser.parse_args()

    version = args.version
    dist = args.dist
    assets = [
        dist / "linux" / f"calculator_{version}_amd64.deb",
        dist / "windows" / f"calculator_{version}_windows_x64.exe",
        dist / "ios" / f"calculator_{version}_ios-device-unsigned.ipa",
        dist / "source" / f"calculator_{version}_source.tar.gz",
        dist / "native" / f"calculator-{version}-native-installer.run",
        dist / "LICENSE.txt",
    ]

    for path in assets:
        if not path.is_file() or path.stat().st_size == 0:
            raise SystemExit(f"missing or empty release asset: {path}")

    native = assets[4]
    if not os.access(native, os.X_OK):
        raise SystemExit(f"native installer is not executable: {native}")

    checksum_path = dist / "SHA256SUMS.txt"
    with checksum_path.open("w", encoding="utf-8", newline="\n") as stream:
        for path in assets:
            stream.write(f"{sha256(path)}  {path.name}\n")

    manifest_path = dist / "release-assets.txt"
    with manifest_path.open("w", encoding="utf-8", newline="\n") as stream:
        for path in assets:
            stream.write(f"{path.as_posix()}\n")
        stream.write(f"{checksum_path.as_posix()}\n")

    print(f"Validated {len(assets)} release assets plus checksum manifest.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
