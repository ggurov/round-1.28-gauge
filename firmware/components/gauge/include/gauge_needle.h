/*
 * gauge_needle.h - the needle sprite, embedded from a generated ARGB8888 blob.
 *
 * tools/gen_needle.py rasterises the blade with 4x supersampling and writes
 * gauge/assets/needle_argb8888.bin.  It ships as white pixels plus an alpha
 * mask so the theme can recolour it at runtime via LVGL's image_recolor.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native geometry baked into the bitmap. */
#define GAUGE_NEEDLE_W            176
#define GAUGE_NEEDLE_H            176
/* Pivot sits in the middle of the bitmap. */
#define GAUGE_NEEDLE_PIVOT_X      (GAUGE_NEEDLE_W / 2)
#define GAUGE_NEEDLE_PIVOT_Y      (GAUGE_NEEDLE_H / 2)
/* Distance in pixels from the pivot to the needle tip. */
#define GAUGE_NEEDLE_TIP_DISTANCE 86

/* Image descriptor backed by the embedded blob.  Binds itself on first call. */
const lv_image_dsc_t *gauge_needle_image(void);

#ifdef __cplusplus
}
#endif
