#!/usr/bin/env python3
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
COMMON = ROOT / "src" / "infiltratr-common"
EXPECTED_VERSION = "1.19.6"
EXPECTED_COMMIT = "4964786ebf1e66dfdb9309c3813dcb17bce19eb5"

assert (COMMON / "VERSION").read_text(encoding="utf-8").strip() == EXPECTED_VERSION

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
assert f'set(INFILTRATR_COMMON_EXPECTED_VERSION "{EXPECTED_VERSION}")' in cmake
assert f'set(INFILTRATR_COMMON_EXPECTED_COMMIT "{EXPECTED_COMMIT}")' in cmake

# A repository build must contain and checkout the immutable Common release
# commit, not merely another revision carrying the same VERSION text.
if (ROOT / ".git").exists():
    actual = subprocess.check_output(
        ["git", "-C", str(COMMON), "rev-parse", "HEAD"],
        text=True,
    ).strip()
    assert actual == EXPECTED_COMMIT, (
        f"Common checkout mismatch: expected {EXPECTED_COMMIT}, got {actual}"
    )
    gitlink = subprocess.check_output(
        ["git", "-C", str(ROOT), "rev-parse", "HEAD:src/infiltratr-common"],
        text=True,
    ).strip()
    assert gitlink == EXPECTED_COMMIT, (
        f"Common gitlink mismatch: expected {EXPECTED_COMMIT}, got {gitlink}"
    )

# Every Common function called by Calculator must exist in the pinned public
# header surface. This catches consumers drifting onto private or removed APIs.
public_headers = "\n".join(
    path.read_text(encoding="utf-8")
    for path in sorted((COMMON / "include" / "infiltratr").glob("*.h"))
)
call_pattern = re.compile(r"\b(infiltratr_[a-z0-9_]+)\s*\(")
calls = set()
for base in (ROOT / "src", ROOT / "ios" / "Bridge"):
    for suffix in ("*.c", "*.cc", "*.cpp", "*.h", "*.hpp", "*.m", "*.mm"):
        for path in base.rglob(suffix):
            if COMMON in path.parents:
                continue
            calls.update(call_pattern.findall(path.read_text(encoding="utf-8")))

assert calls, "Calculator no longer exercises Common public APIs"
missing = sorted(
    function for function in calls
    if not re.search(rf"\b{re.escape(function)}\s*\(", public_headers)
)
assert not missing, f"Calculator calls APIs absent from Common public headers: {missing}"

# iPhone compiles Common directly, so derive the canonical Portable source list
# from Common's own CMake instead of duplicating that private list in this test.
common_cmake = (COMMON / "CMakeLists.txt").read_text(encoding="utf-8")
match = re.search(
    r"set\(INFILTRATR_COMMON_PORTABLE_SOURCES\s+(.*?)\)",
    common_cmake,
    re.S,
)
assert match, "Unable to locate Common Portable source list"
portable_sources = re.findall(r"src/[A-Za-z0-9_.-]+\.c", match.group(1))

ios_project = (ROOT / "ios" / "project.yml").read_text(encoding="utf-8")
missing_ios = [
    source for source in portable_sources
    if f"../src/infiltratr-common/{source}" not in ios_project
]
assert not missing_ios, (
    f"iPhone omits Common Portable sources: {missing_ios}"
)

print(
    f"Common {EXPECTED_VERSION} integration contract passed "
    f"at {EXPECTED_COMMIT[:12]}."
)
