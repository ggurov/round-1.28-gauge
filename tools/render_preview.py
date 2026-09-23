#!/usr/bin/env python3
"""
Render host-side mock-ups of every gauge preset.

Mirrors the geometry the firmware builds from LVGL primitives, so the design
can be reviewed - and the arithmetic sanity-checked - without flashing the
board.  Keep these constants in step with:

  firmware/components/gauge/gauge.c            (layout maths)
  firmware/components/gauge/gauge_theme.c      (palette, tick geometry)
  firmware/components/gauge/gauge_presets.c    (ranges, captions)

Run:  python tools/render_preview.py
"""

from __future__ import annotations

import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_needle import POLY, PIVOT, SIZE as NEEDLE_SIZE  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(REPO, "tools", "preview")

SS = 3                      # supersample factor
DIAL = 240                  # panel is 240x240

# ---- geometry, mirrors gauge.c / gauge_theme_greddy ----------------------
BEZEL_W = 4
BAND_GAP = 3
BAND_W = 5
TICK_MAJOR_LEN = 16
TICK_MINOR_LEN = 8
TICK_MAJOR_W = 3
TICK_MINOR_W = 1
LABEL_PAD_RADIAL = 3
LABEL_ROTATE = 0            # numerals stay upright

HUB_R = 18
Y_WORDMARK = -(HUB_R + 26)
Y_TAGLINE = -(HUB_R + 10)
Y_CAPTION = HUB_R + 16
Y_VALUE = HUB_R + 42
Y_UNIT = HUB_R + 68

F_LABEL, F_CAPTION, F_VALUE = 20, 16, 30
F_UNIT, F_WORD, F_TAG = 12, 14, 10

# LVGL's lv_scale places the numeral centre at:
#   radius - major_len - (pad_radial + LV_SCALE_DEFAULT_LABEL_GAP)
LV_SCALE_DEFAULT_LABEL_GAP = 15

RAIL_R = DIAL // 2 - (BEZEL_W + BAND_GAP + BAND_W // 2)
SCALE_D = 2 * RAIL_R
LABEL_R = RAIL_R - TICK_MAJOR_LEN - LV_SCALE_DEFAULT_LABEL_GAP - LABEL_PAD_RADIAL
NEEDLE_LEN = RAIL_R - TICK_MAJOR_LEN - 2

# ---- palettes, mirrors gauge_theme.c -------------------------------------
GREDDY = dict(
    face=(0x00, 0x00, 0x00), bezel=(0xE8, 0xE8, 0xE8),
    tick_major=(0x2B, 0xE0, 0x6A), tick_minor=(0x27, 0xC0, 0x5C),
    label=(0x46, 0xF0, 0x8A), band=(0x2B, 0xE0, 0x6A),
    band_glow=(0x0E, 0x8B, 0x3C), alarm=(0xFF, 0x2D, 0x9E),
    needle=(0xFF, 0x3B, 0x0A), hub=(0x0A, 0x0A, 0x0A), hub_ring=(0x33, 0x33, 0x33),
    value=(0x5C, 0xFF, 0x9E), caption=(0x3B, 0xE8, 0x7C), unit=(0x27, 0xB8, 0x5E),
    wordmark=(0x46, 0xF0, 0x8A), tagline=(0x1E, 0x9C, 0x4E),
)

AMBER = dict(
    face=(0x00, 0x00, 0x00), bezel=(0xD0, 0xD0, 0xD0),
    tick_major=(0xFF, 0xA0, 0x00), tick_minor=(0xB8, 0x74, 0x00),
    label=(0xFF, 0xB7, 0x33), band=(0xFF, 0xA0, 0x00),
    band_glow=(0x8B, 0x4A, 0x00), alarm=(0xFF, 0x2D, 0x2D),
    needle=(0xFF, 0x3B, 0x0A), hub=(0x0A, 0x0A, 0x0A), hub_ring=(0x33, 0x33, 0x33),
    value=(0xFF, 0xC2, 0x4D), caption=(0xFF, 0xA0, 0x00), unit=(0xB8, 0x74, 0x00),
    wordmark=(0xFF, 0xB7, 0x33), tagline=(0x8B, 0x5A, 0x00),
)

# ---- presets, mirrors gauge_presets.c ------------------------------------
PRESETS = [
    dict(id="rpm", caption="RPM", unit="x1000 r/min", wordmark="R-GAUGE",
         tagline="TUNING SYSTEM", lo=0, hi=8000, major=1000, minor=4,
         alarm=6500, decimals=0, theme=GREDDY, show=4200,
         labels=["0", "1", "2", "3", "4", "5", "6", "7", "8"]),
    dict(id="temp", caption="CL TEMP", unit="DEG C", wordmark="R-GAUGE",
         tagline="TUNING SYSTEM", lo=50, hi=150, major=10, minor=2,
         alarm=115, decimals=0, theme=GREDDY, show=92),
    dict(id="boost", caption="BOOST", unit="BAR", wordmark="R-GAUGE",
         tagline="TUNING SYSTEM", lo=-1.0, hi=2.0, major=0.5, minor=5,
         alarm=1.75, decimals=1, theme=GREDDY, show=0.9),
    dict(id="volts", caption="VOLTS", unit="V DC", wordmark="R-GAUGE",
         tagline="TUNING SYSTEM", lo=8, hi=16, major=1, minor=2,
         alarm=None, decimals=1, theme=AMBER, show=13.8),
]

ROT = 135.0     # first tick, clockwise from 3 o'clock
SWEEP = 270.0


def pick_font(size: int, bold: bool = True):
    names = (["seguisb.ttf", "segoeuib.ttf", "arialbd.ttf", "verdanab.ttf", "calibrib.ttf"]
             if bold else
             ["segoeui.ttf", "arial.ttf", "verdana.ttf", "calibri.ttf"])
    for n in names:
        p = os.path.join(r"C:\Windows\Fonts", n)
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def polar(cx, cy, r, deg):
    a = math.radians(deg)
    return cx + r * math.cos(a), cy + r * math.sin(a)


def text_centered(d, xy, text, font, fill, letter_space=0):
    cx, cy = xy
    if letter_space:
        widths = [d.textlength(c, font=font) for c in text]
        total = sum(widths) + letter_space * (len(text) - 1)
        x = cx - total / 2
        for ch, w in zip(text, widths):
            d.text((x, cy), ch, font=font, fill=fill, anchor="lm")
            x += w + letter_space
    else:
        d.text((cx, cy), text, font=font, fill=fill, anchor="mm")


def fmt(value: float, decimals: int) -> str:
    return f"{value:.{decimals}f}"


def angle_of(value: float, p: dict) -> float:
    frac = (value - p["lo"]) / (p["hi"] - p["lo"])
    frac = min(1.0, max(0.0, frac))
    return ROT + SWEEP * frac


def render(p: dict) -> Image.Image:
    th = p["theme"]
    S = DIAL * SS
    img = Image.new("RGB", (S, S), (14, 14, 16))
    d = ImageDraw.Draw(img)

    cx = cy = S / 2
    rail = RAIL_R * SS
    band_w = BAND_W * SS
    end = ROT + SWEEP

    r_outer = DIAL / 2 * SS
    d.ellipse([cx - r_outer, cy - r_outer, cx + r_outer, cy + r_outer],
              fill=th["face"], outline=th["bezel"], width=BEZEL_W * SS)

    # glow: stacked translucent arcs behind the band
    glow_w = BAND_W * 4 * SS
    for i in range(6, 0, -1):
        w = int(glow_w * i / 6)
        overlay = Image.new("RGB", (S, S), (0, 0, 0))
        od = ImageDraw.Draw(overlay)
        od.arc([cx - rail, cy - rail, cx + rail, cy + rail], ROT, end,
               fill=th["band_glow"], width=w)
        mask = Image.new("L", (S, S), 0)
        ImageDraw.Draw(mask).arc([cx - rail, cy - rail, cx + rail, cy + rail],
                                 ROT, end, fill=int(90 / i), width=w)
        img = Image.composite(overlay, img, mask)
    d = ImageDraw.Draw(img)

    d.arc([cx - rail, cy - rail, cx + rail, cy + rail], ROT, end,
          fill=th["band"], width=band_w)

    majors = int(round((p["hi"] - p["lo"]) / p["major"]))
    total_ticks = majors * p["minor"] + 1

    alarm_deg = None
    if p["alarm"] is not None and p["alarm"] <= p["hi"]:
        alarm_deg = angle_of(p["alarm"], p)
        d.arc([cx - rail, cy - rail, cx + rail, cy + rail], alarm_deg, end,
              fill=th["alarm"], width=band_w)

    for i in range(total_ticks):
        deg = ROT + SWEEP * i / (total_ticks - 1)
        is_major = (i % p["minor"]) == 0
        ln = (TICK_MAJOR_LEN if is_major else TICK_MINOR_LEN) * SS
        w = max(1, int((TICK_MAJOR_W if is_major else TICK_MINOR_W) * SS))
        col = th["alarm"] if (is_major and alarm_deg is not None and deg >= alarm_deg) \
            else (th["tick_major"] if is_major else th["tick_minor"])
        d.line([polar(cx, cy, rail, deg), polar(cx, cy, rail - ln, deg)],
               fill=col, width=w)

    f_label = pick_font(int(F_LABEL * SS))
    labels = p.get("labels")
    for i in range(majors + 1):
        deg = ROT + SWEEP * i / majors
        txt = labels[i] if labels else fmt(p["lo"] + p["major"] * i, p["decimals"])
        text_centered(d, polar(cx, cy, LABEL_R * SS, deg), txt, f_label, th["label"])

    # needle
    scale = NEEDLE_LEN / 86.0
    nw = int(round(NEEDLE_SIZE * scale * SS))
    nimg = Image.new("RGBA", (NEEDLE_SIZE, NEEDLE_SIZE), (0, 0, 0, 0))
    ImageDraw.Draw(nimg).polygon([(PIVOT + x, PIVOT + y) for x, y in POLY],
                                 fill=th["needle"] + (255,))
    nimg = nimg.resize((nw, nw), Image.LANCZOS)
    theta = angle_of(p["show"], p)
    pivot_px = PIVOT * nw / NEEDLE_SIZE
    nimg = nimg.rotate(-(theta - 270.0), resample=Image.BICUBIC,
                       center=(pivot_px, pivot_px))
    img.paste(nimg, (int(round(cx - pivot_px)), int(round(cy - pivot_px))), nimg)

    hr = HUB_R * SS
    d.ellipse([cx - hr, cy - hr, cx + hr, cy + hr], fill=th["hub"],
              outline=th["hub_ring"], width=2 * SS)

    f_word = pick_font(int(F_WORD * SS))
    f_tag = pick_font(int(F_TAG * SS), bold=False)
    f_cap = pick_font(int(F_CAPTION * SS))
    f_val = pick_font(int(F_VALUE * SS))
    f_unit = pick_font(int(F_UNIT * SS), bold=False)

    text_centered(d, (cx, cy + Y_WORDMARK * SS), p["wordmark"], f_word, th["wordmark"], 2 * SS)
    text_centered(d, (cx, cy + Y_TAGLINE * SS), p["tagline"], f_tag, th["tagline"], SS)
    text_centered(d, (cx, cy + Y_CAPTION * SS), p["caption"], f_cap, th["caption"], SS)
    text_centered(d, (cx, cy + Y_VALUE * SS), fmt(p["show"], p["decimals"]), f_val, th["value"])
    text_centered(d, (cx, cy + Y_UNIT * SS), p["unit"], f_unit, th["unit"], SS)

    return img.resize((DIAL, DIAL), Image.LANCZOS)


def main() -> int:
    os.makedirs(OUT_DIR, exist_ok=True)
    sheet = Image.new("RGB", (DIAL * len(PRESETS), DIAL), (14, 14, 16))

    for i, p in enumerate(PRESETS):
        img = render(p)
        path = os.path.join(OUT_DIR, f"dial_{p['id']}.png")
        img.save(path)
        img.resize((DIAL * 2, DIAL * 2), Image.NEAREST).save(
            os.path.join(OUT_DIR, f"dial_{p['id']}_2x.png"))
        sheet.paste(img, (DIAL * i, 0))
        print(f"preview : {path}")

    sheet_path = os.path.join(OUT_DIR, "dial_all.png")
    sheet.resize((DIAL * len(PRESETS) * 2, DIAL * 2), Image.NEAREST).save(sheet_path)
    print(f"preview : {sheet_path}")

    print()
    print(f"  rail radius    : {RAIL_R} px   (lv_scale widget {SCALE_D}x{SCALE_D})")
    print(f"  needle tip     : {NEEDLE_LEN} px")
    print(f"  numeral centre : {LABEL_R} px   "
          f"(band {LABEL_R - 12} .. {LABEL_R + 12})")
    print(f"  major ticks    : {RAIL_R - TICK_MAJOR_LEN} .. {RAIL_R} px")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
