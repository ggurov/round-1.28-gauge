#!/usr/bin/env python3
"""
Rasterise the gauge needle into an ARGB8888 blob for the firmware.

The needle is drawn white with an alpha mask so the theme can tint it at
runtime via LVGL's `image_recolor`.  Output is written straight to
firmware/components/gauge/assets/needle_argb8888.bin, which the gauge
component embeds with ESP-IDF's EMBED_FILES.

Geometry (bitmap coordinates, +y is DOWN, origin at the top-left):
    the pivot sits at the centre of the square bitmap
    the blade points straight UP from the pivot
    a short tail points down so the hub has something to cover

Run:  python tools/gen_needle.py
"""

from __future__ import annotations

import os
import struct
import sys
import zlib

# --------------------------------------------------------------------------
# Keep these in sync with gauge_needle.h
# --------------------------------------------------------------------------
SIZE = 176                 # bitmap is SIZE x SIZE
PIVOT = SIZE // 2          # 88
BLADE_LEN = 86             # pivot -> tip
TAIL_LEN = 14              # pivot -> tail end
BASE_HALF_W = 6.0
TIP_HALF_W = 0.9
TAIL_HALF_W = 4.5
SUPERSAMPLE = 4

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN_OUT = os.path.join(REPO, "firmware", "components", "gauge", "assets",
                       "needle_argb8888.bin")
PREVIEW_OUT = os.path.join(REPO, "tools", "preview", "needle.png")

# Blade + tail as one polygon, in pivot-relative coordinates (y up is negative).
POLY = [
    (-BASE_HALF_W, 0.0),
    (-TIP_HALF_W, -float(BLADE_LEN)),
    (TIP_HALF_W, -float(BLADE_LEN)),
    (BASE_HALF_W, 0.0),
    (TAIL_HALF_W, float(TAIL_LEN)),
    (-TAIL_HALF_W, float(TAIL_LEN)),
]


def point_in_poly(x: float, y: float, poly) -> bool:
    """Even-odd ray casting."""
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y):
            x_cross = (xj - xi) * (y - yi) / (yj - yi) + xi
            if x < x_cross:
                inside = not inside
        j = i
    return inside


def rasterise() -> bytearray:
    """Return SIZE*SIZE*4 bytes of little-endian BGRA, white with coverage alpha."""
    # Only walk the needle's bounding box; everything else stays transparent.
    min_x = int(min(p[0] for p in POLY)) - 2
    max_x = int(max(p[0] for p in POLY)) + 2
    min_y = int(min(p[1] for p in POLY)) - 2
    max_y = int(max(p[1] for p in POLY)) + 2

    buf = bytearray(SIZE * SIZE * 4)

    inv = 1.0 / SUPERSAMPLE
    samples = SUPERSAMPLE * SUPERSAMPLE

    for py in range(min_y, max_y + 1):
        by = PIVOT + py
        if not (0 <= by < SIZE):
            continue
        for px in range(min_x, max_x + 1):
            bx = PIVOT + px
            if not (0 <= bx < SIZE):
                continue

            hits = 0
            for sy in range(SUPERSAMPLE):
                fy = py + (sy + 0.5) * inv
                for sx in range(SUPERSAMPLE):
                    fx = px + (sx + 0.5) * inv
                    if point_in_poly(fx, fy, POLY):
                        hits += 1
            if hits == 0:
                continue

            alpha = int(round(255.0 * hits / samples))
            off = (by * SIZE + bx) * 4
            buf[off + 0] = 0xFF   # B
            buf[off + 1] = 0xFF   # G
            buf[off + 2] = 0xFF   # R
            buf[off + 3] = alpha  # A

    return buf


# --------------------------------------------------------------------------
# tiny dependency-free PNG writer, for the human-readable preview
# --------------------------------------------------------------------------
def write_png(path: str, width: int, height: int, rgb: bytearray) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter: none
        raw += rgb[y * width * 3:(y + 1) * width * 3]

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as fh:
        fh.write(png)


def write_preview(buf: bytearray, path: str, zoom: int = 2) -> None:
    """Composite the needle over a dark grey background so edges are visible."""
    w = SIZE * zoom
    h = SIZE * zoom
    rgb = bytearray(w * h * 3)
    for y in range(h):
        for x in range(w):
            sx, sy = x // zoom, y // zoom
            off = (sy * SIZE + sx) * 4
            a = buf[off + 3] / 255.0
            # orange blade over a 32/32/36 background
            r = int(0x20 * (1 - a) + 0xFF * a)
            g = int(0x20 * (1 - a) + 0x3B * a)
            b = int(0x24 * (1 - a) + 0x0A * a)
            o = (y * w + x) * 3
            rgb[o], rgb[o + 1], rgb[o + 2] = r, g, b
    write_png(path, w, h, rgb)


def main() -> int:
    buf = rasterise()

    os.makedirs(os.path.dirname(BIN_OUT), exist_ok=True)
    with open(BIN_OUT, "wb") as fh:
        fh.write(buf)

    opaque = sum(1 for i in range(3, len(buf), 4) if buf[i] > 0)
    print(f"needle_asset  : {BIN_OUT}")
    print(f"  format      : ARGB8888, {SIZE}x{SIZE}, {len(buf)} bytes")
    print(f"  pivot       : ({PIVOT}, {PIVOT})")
    print(f"  tip distance: {BLADE_LEN} px")
    print(f"  covered px  : {opaque}")

    try:
        write_preview(buf, PREVIEW_OUT)
        print(f"preview       : {PREVIEW_OUT}")
    except Exception as exc:  # noqa: BLE001
        print(f"preview       : skipped ({exc})", file=sys.stderr)

    expected = SIZE * SIZE * 4
    if len(buf) != expected:
        print(f"ERROR: expected {expected} bytes, got {len(buf)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
