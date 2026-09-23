#!/usr/bin/env python3
"""
Contract tests for things the C unit tests cannot see.

These guard the assumptions the firmware makes about *other* people's code and
about its own generated artefacts:

  1. LVGL's private LV_SCALE_DEFAULT_LABEL_GAP must stay equal to the
     GAUGE_LABEL_GAP the layout maths mirrors.  An LVGL bump that changes it
     would silently move every numeral on the dial.
  2. tools/gen_needle.py must agree with gauge_needle_size.h, otherwise the
     embedded sprite and the pivot the widget uses drift apart.
  3. Every Montserrat size a theme asks for must be enabled in
     sdkconfig.defaults; a missing font falls back silently and changes layout.
  4. tools/render_preview.py must offer the same presets as the firmware, so
     the host previews stay trustworthy.

Run:  python tests/py/test_contracts.py
"""

from __future__ import annotations

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

GAUGE_DIR = os.path.join(REPO, "firmware", "components", "gauge")
LVGL_SCALE_C = os.path.join(REPO, "firmware", "managed_components", "lvgl__lvgl",
                            "src", "widgets", "scale", "lv_scale.c")

FAILURES: list[str] = []
CHECKS = 0


def check(name: str, condition: bool, detail: str = "") -> None:
    global CHECKS
    CHECKS += 1
    if condition:
        print(f"    PASS  {name}")
    else:
        print(f"    FAIL  {name}" + (f"\n          {detail}" if detail else ""))
        FAILURES.append(name)


def read(path: str) -> str:
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()


def define_int(text: str, name: str) -> int | None:
    """Resolve a simple `#define NAME <int>` (allowing casts and U/L suffixes).

    Returns None when the body is an expression involving other macros, so a
    contract check fails loudly instead of silently comparing a wrong number.
    """
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(.+)$", text, re.M)
    if not m:
        return None
    body = m.group(1).strip()
    body = re.sub(r"uint\d+_t", "", body)     # drop cast types
    body = re.sub(r"(\d)[uUlL]+", r"\1", body)  # drop integer suffixes
    if re.search(r"[A-Za-z_]", body):          # still references something
        return None
    nums = re.findall(r"-?\d+", body)
    return int(nums[-1]) if nums else None


def const_int(text: str, name: str) -> int | None:
    """Resolve a Python `NAME = <int>` at module level."""
    m = re.search(rf"^{re.escape(name)}\s*=\s*(-?\d+)\s*,?\s*$", text, re.M)
    return int(m.group(1)) if m else None


# ---------------------------------------------------------------------------
# 1. LVGL's label gap
# ---------------------------------------------------------------------------
def test_lvgl_label_gap() -> None:
    print("\n  [contract/lvgl]")
    if not os.path.exists(LVGL_SCALE_C):
        check("lvgl source available", False,
              f"{LVGL_SCALE_C} not found - run a build first so managed components are fetched")
        return

    lvgl_src = read(LVGL_SCALE_C)
    lvgl_gap = define_int(lvgl_src, "LV_SCALE_DEFAULT_LABEL_GAP")
    math_h = read(os.path.join(GAUGE_DIR, "include", "gauge_math.h"))
    our_gap = define_int(math_h, "GAUGE_LABEL_GAP")

    check("LVGL defines LV_SCALE_DEFAULT_LABEL_GAP", lvgl_gap is not None)
    check("gauge_math.h defines GAUGE_LABEL_GAP", our_gap is not None)
    if lvgl_gap is None or our_gap is None:
        return

    check(f"GAUGE_LABEL_GAP ({our_gap}) == LV_SCALE_DEFAULT_LABEL_GAP ({lvgl_gap})",
          lvgl_gap == our_gap,
          "Numeral placement is derived from this constant; update gauge_math.h "
          "and re-check the dial if LVGL changed it.")


# ---------------------------------------------------------------------------
# 2. needle generator vs header
# ---------------------------------------------------------------------------
def test_needle_generator() -> None:
    print("\n  [contract/needle]")
    gen = read(os.path.join(REPO, "tools", "gen_needle.py"))
    hdr = read(os.path.join(GAUGE_DIR, "include", "gauge_needle_size.h"))

    def gen_int(name: str) -> int | None:
        m = re.search(rf"^{name}\s*=\s*(\d+)", gen, re.M)
        return int(m.group(1)) if m else None

    pairs = [
        ("SIZE", "GAUGE_NEEDLE_W"),
        ("SIZE", "GAUGE_NEEDLE_H"),
        ("BLADE_LEN", "GAUGE_NEEDLE_TIP_DISTANCE"),
    ]
    for gen_name, hdr_name in pairs:
        g = gen_int(gen_name)
        h = define_int(hdr, hdr_name)
        check(f"gen_needle.{gen_name} ({g}) == {hdr_name} ({h})",
              g is not None and g == h,
              "Regenerate the sprite and update the header together.")

    pivot = define_int(hdr, "GAUGE_NEEDLE_PIVOT_X")
    size = define_int(hdr, "GAUGE_NEEDLE_W")
    check("pivot is the centre of the bitmap",
          pivot is not None and size is not None and pivot == size // 2,
          f"pivot {pivot}, size {size}")


# ---------------------------------------------------------------------------
# 3. fonts used by themes are enabled in sdkconfig.defaults
# ---------------------------------------------------------------------------
def test_fonts_enabled() -> None:
    print("\n  [contract/fonts]")
    theme_src = read(os.path.join(GAUGE_DIR, "gauge_theme.c"))
    sdk = read(os.path.join(REPO, "firmware", "sdkconfig.defaults"))

    used = sorted({int(m) for m in re.findall(r"GAUGE_FONT_(\d+)", theme_src)})
    check("themes reference at least one font", len(used) > 0)

    for size in used:
        enabled = re.search(rf"^CONFIG_LV_FONT_MONTSERRAT_{size}=y\s*$", sdk, re.M)
        check(f"CONFIG_LV_FONT_MONTSERRAT_{size} enabled",
              enabled is not None,
              f"sdkconfig.defaults must enable montserrat_{size}; a missing face "
              "falls back silently and the layout shifts")

    theme_h = read(os.path.join(GAUGE_DIR, "include", "gauge_theme.h"))
    enum_sizes = [int(m) for m in re.findall(r"GAUGE_FONT_(\d+)(?:,|\s*=)", theme_h)]
    for size in used:
        check(f"GAUGE_FONT_{size} is in the gauge_font_t enum", size in enum_sizes)


# ---------------------------------------------------------------------------
# 4. preview presets match the firmware presets
# ---------------------------------------------------------------------------
def test_preview_presets() -> None:
    print("\n  [contract/preview]")
    preview = read(os.path.join(REPO, "tools", "render_preview.py"))
    presets_c = read(os.path.join(GAUGE_DIR, "gauge_presets.c"))

    preview_ids = set(re.findall(r'dict\(id="([a-z0-9_]+)"', preview))
    firmware_ids = set(re.findall(r'^\s*\{\s*"([a-z0-9_]+)"\s*,', presets_c, re.M))

    check("render_preview.py defines presets", len(preview_ids) > 0)
    check("gauge_presets.c defines presets", len(firmware_ids) > 0)
    check(f"preset ids match: {sorted(firmware_ids)}",
          preview_ids == firmware_ids,
          f"preview has {sorted(preview_ids)}, firmware has {sorted(firmware_ids)}")

    # preview geometry constants must track the theme
    theme = read(os.path.join(GAUGE_DIR, "gauge_theme.c"))
    for name, c_name in [("BEZEL_W", "bezel_width"), ("BAND_GAP", "band_gap"),
                         ("BAND_W", "band_width"), ("TICK_MAJOR_LEN", "tick_major_len"),
                         ("TICK_MINOR_LEN", "tick_minor_len"),
                         ("HUB_R", "hub_radius")]:
        py = const_int(preview, name)
        c = re.search(rf"\.{c_name}\s*=\s*(\d+)\s*,", theme)
        c_val = int(c.group(1)) if c else None
        check(f"preview {name} ({py}) == theme .{c_name} ({c_val})",
              py is not None and py == c_val,
              "Keep tools/render_preview.py in step with gauge_theme.c")

    # ...and so must the typography, or the previews mislead about text fit
    m = re.search(r"^(F_LABEL, F_CAPTION, F_VALUE) = (\d+), (\d+), (\d+)$", preview, re.M)
    m2 = re.search(r"^(F_UNIT, F_WORD, F_TAG) = (\d+), (\d+), (\d+)$", preview, re.M)
    check("render_preview.py declares its font sizes", m is not None and m2 is not None)

    if m and m2:
        preview_fonts = {
            "font_label": int(m.group(2)),
            "font_caption": int(m.group(3)),
            "font_value": int(m.group(4)),
            "font_unit": int(m2.group(2)),
            "font_wordmark": int(m2.group(3)),
            "font_tagline": int(m2.group(4)),
        }
        for c_name, size in preview_fonts.items():
            c = re.search(rf"\.{c_name}\s*=\s*GAUGE_FONT_(\d+)\s*,", theme)
            c_val = int(c.group(1)) if c else None
            check(f"preview {c_name} ({size}) == theme GAUGE_FONT_{c_val}",
                  c_val == size,
                  "Text will not fit the same way in the preview and on the panel")


def main() -> int:
    print("round-1.28-gauge contract checks")
    test_lvgl_label_gap()
    test_needle_generator()
    test_fonts_enabled()
    test_preview_presets()

    print("\n-------------------- summary --------------------")
    print(f"  checks: {CHECKS} run, {len(FAILURES)} failed")
    print(f"  result: {'OK' if not FAILURES else 'FAILED'}")
    print("===================================================")
    return 0 if not FAILURES else 1


if __name__ == "__main__":
    raise SystemExit(main())
