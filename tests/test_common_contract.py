#!/usr/bin/env python3
from pathlib import Path
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
COMMON = ROOT / "src" / "infiltratr-common"
EXPECTED_VERSION = "1.19.57"
EXPECTED_COMMIT = "0cfa4a36cf84034fd77d5c17a59990f068d5c1fb"

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
        f"Common {EXPECTED_VERSION} semantic palette field missing from public ABI: {field}"
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
    assert required in calls, f"Calculator is not consuming Common {EXPECTED_VERSION} {required}"

# The old three-face archive remains available for existing Common consumers.
# Calculator's extended Windows/iPhone faces must instead match the exact
# SHA-256-qualified 23-face source revision of pinned Common's installer.
extended_installer = (
    COMMON / "cmake" / "InfiltratrEnsureExtendedFonts.sh"
).read_text(encoding="utf-8")
revision_match = re.search(
    r'^SOURCE_COMMIT="([a-f0-9]{40})"$',
    extended_installer, re.MULTILINE,
)
assert revision_match is not None, "Common extended font source is not pinned"
revision = revision_match.group(1)
assert f'set(CALCULATOR_EXTENDED_FONTS_COMMIT "{revision}")' in cmake
assert "EXPECTED_HASH" in cmake and "CALCULATOR_FONT_MANIFEST_COUNT EQUAL 23" in cmake

ios_font_script = (ROOT / "ios" / "bundle-fonts.sh").read_text(encoding="utf-8")
assert f'REVISION="{revision}"' in ios_font_script
assert "shasum -a 256" in ios_font_script
assert 'wc -l < "${MANIFEST}"' in ios_font_script

selected_fonts = (
    "mb_corpo_s_text_regular.ttf",
    "mb_corpo_s_text_bold.ttf",
    "mb_corpo_a_title_cond_regular.ttf",
    "mb_corpo_s_text_cond_bold.ttf",
    "mb_corpo_a_screen_2_cond_regular.ttf",
)
ios_project = (ROOT / "ios" / "project.yml").read_text(encoding="utf-8")
for filename in selected_fonts:
    assert filename in cmake, f"Missing verified Windows role: {filename}"
    assert filename in ios_font_script, f"Missing verified iPhone role: {filename}"
    assert filename in ios_project, f"iPhone bundle omits {filename}"


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
            f"Common {EXPECTED_VERSION} {palette_name} palette missing semantic role: {role}"
        )

# The historic Common three-face JSON stays available for old consumers,
# but the 0.2.53 app intentionally bundles the verified extended role selection.
ios_project_text = (ROOT / "ios" / "project.yml").read_text(encoding="utf-8")
ios_typography_text = (
    ROOT / "ios" / "Sources" / "CalculatorTypography.swift"
).read_text(encoding="utf-8")
for filename in selected_fonts:
    assert filename in ios_project_text, (
        f"iPhone bundle declaration omits extended typography: {filename}"
    )
    assert Path(filename).stem in ios_typography_text, (
        f"iPhone font registration omits extended typography: {filename}"
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
