#!/usr/bin/env python3
from pathlib import Path
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
COMMON = ROOT / "src" / "infiltratr-common"
EXPECTED_VERSION = "1.19.35"
EXPECTED_COMMIT = "7cc5de3de0e94ed2cfcff0840bbb5346eb5c9c9f"

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

# Scientific engineering notation must use Common's deterministic integer
# parser for its generated exponent rather than maintaining a second locale/
# exception-based integer parse path.
scientific = (ROOT / "src" / "core" / "scientific.cpp").read_text(encoding="utf-8")
assert "std::stoi(" not in scientific
assert 'infiltratr_parse_i64(' in scientific

design_header = (COMMON / "include" / "infiltratr" / "design.h").read_text(
    encoding="utf-8"
)
for field in (
    "titlebar_rgb",
    "heading_rgb",
    "summary_rgb",
    "kicker_rgb",
    "detail_label_rgb",
    "note_rgb",
    "status_border_rgb",
    "accent_foreground_rgb",
    "accent_hover_rgb",
    "selected_summary_rgb",
    "warning_muted_rgb",
    "warning_border_rgb",
    "success_border_rgb",
):
    assert field in design_header, (
        f"Common 1.19.35 semantic palette field missing from public ABI: {field}"
    )

# Calculator must consume Common through its published surface only. Common
# 1.19.20 deliberately consolidated several internal helpers; importing those
# private headers would couple Calculator to implementation detail rather than
# the immutable public contract.
private_common_headers = (
    "ascii_internal.h",
    "posix_read_internal.h",
)
for base in (ROOT / "src", ROOT / "ios" / "Bridge"):
    for suffix in ("*.c", "*.cc", "*.cpp", "*.h", "*.hpp", "*.m", "*.mm"):
        for path in base.rglob(suffix):
            if COMMON in path.parents:
                continue
            source = path.read_text(encoding="utf-8")
            for private_header in private_common_headers:
                assert private_header not in source, (
                    f"{path} imports private Common implementation header "
                    f"{private_header}"
                )

for required in (
    "infiltratr_parse_double_token",
    "infiltratr_parse_double",
    "infiltratr_parse_u64",
    "infiltratr_parse_i64",
    "infiltratr_parse_u64_range",
    "infiltratr_parse_double_range",
    "infiltratr_i64_add_checked",
    "infiltratr_u64_multiply_checked",
    "infiltratr_ascii_is_space",
    "infiltratr_ascii_is_alpha",
    "infiltratr_ascii_is_alnum",
    "infiltratr_ascii_is_digit",
    "infiltratr_ascii_to_lower",
    "infiltratr_ascii_contains_ci",
    "infiltratr_ascii_equal_ci",
    "infiltratr_utf8_encode_codepoint",
    "infiltratr_theme_mode_key",
    "infiltratr_theme_mode_parse",
    "infiltratr_mkdir_parents",
    "infiltratr_atomic_file_write_bytes",
    "infiltratr_read_text_file_alloc",
    "infiltratr_design_metrics",
    "infiltratr_typography",
):
    assert required in calls, f"Calculator is not consuming Common 1.19.35 {required}"

typography_assets = (
    COMMON / "cmake" / "InfiltratrTypographyAssets.cmake"
).read_text(encoding="utf-8")
assert "InfiltratrTypographyAssets.cmake" in cmake
for variable in (
    "INFILTRATR_MB_CORPO_ARCHIVE_URL",
    "INFILTRATR_MB_CORPO_ARCHIVE_SHA256",
    "INFILTRATR_MB_CORPO_BRAND_REGULAR_FILE",
    "INFILTRATR_MB_CORPO_UI_BOLD_FILE",
    "INFILTRATR_MB_CORPO_UI_REGULAR_FILE",
):
    assert variable in typography_assets, f"Common typography metadata missing {variable}"
    assert variable in cmake, f"Calculator duplicates Common typography metadata: {variable}"

# The iPhone font bundler reads canonical provenance directly from Common's
# design contract rather than carrying another commit/hash table.
ios_font_script = (ROOT / "ios" / "bundle-fonts.sh").read_text(encoding="utf-8")
for key in (
    "typography.assets.source_repository",
    "typography.assets.source_commit",
    "typography.assets.archive_path",
    "typography.assets.archive_sha256",
    "typography.font_files.brand_regular",
    "typography.font_files.ui_bold",
    "typography.font_files.ui_regular",
    "typography.assets.file_sha256.brand_regular",
    "typography.assets.file_sha256.ui_bold",
    "typography.assets.file_sha256.ui_regular",
):
    assert key in ios_font_script, f"iPhone font bundler bypasses Common metadata: {key}"

design_contract = json.loads(
    (COMMON / "design" / "infiltrator-design-v1.json").read_text(encoding="utf-8")
)
for palette_name in ("day", "night"):
    semantic_palette = design_contract["theme"]["palettes"][palette_name]
    for role in (
        "titlebar",
        "heading",
        "summary",
        "kicker",
        "detail_label",
        "note",
        "status_border",
        "accent_hover",
        "selected_summary",
        "warning_muted",
        "warning_border",
        "success_border",
    ):
        assert role in semantic_palette, (
            f"Common 1.19.35 {palette_name} palette missing semantic role: {role}"
        )

canonical_fonts = design_contract["typography"]["font_files"]
ios_project_text = (ROOT / "ios" / "project.yml").read_text(encoding="utf-8")
ios_typography_text = (
    ROOT / "ios" / "Sources" / "CalculatorTypography.swift"
).read_text(encoding="utf-8")
for filename in canonical_fonts.values():
    assert filename in ios_project_text, (
        f"iPhone bundle declaration drifted from Common typography: {filename}"
    )
    assert Path(filename).stem in ios_typography_text, (
        f"iPhone typography registration drifted from Common typography: {filename}"
    )

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
