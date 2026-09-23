/*
 * gauge_needle_size.h - geometry baked into the generated needle sprite.
 *
 * Split out from gauge_needle.h so it can be included by host tests without
 * pulling in LVGL.  tools/gen_needle.py must agree with these numbers; the
 * asset test in tests/host checks the generated blob against them.
 */
#pragma once

#define GAUGE_NEEDLE_W            176
#define GAUGE_NEEDLE_H            176
/* Centre of the bitmap; the C tests assert these equal W/2 and H/2. */
#define GAUGE_NEEDLE_PIVOT_X      88
#define GAUGE_NEEDLE_PIVOT_Y      88
/* Distance in pixels from the pivot to the needle tip. */
#define GAUGE_NEEDLE_TIP_DISTANCE 86
/* Bytes per pixel, LV_COLOR_FORMAT_ARGB8888. */
#define GAUGE_NEEDLE_BPP          4
/* Exact size of assets/needle_argb8888.bin. */
#define GAUGE_NEEDLE_BYTES        (GAUGE_NEEDLE_W * GAUGE_NEEDLE_H * GAUGE_NEEDLE_BPP)
