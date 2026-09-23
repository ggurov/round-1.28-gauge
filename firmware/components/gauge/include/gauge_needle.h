/*
 * gauge_needle.h - the needle sprite as an LVGL image descriptor.
 *
 * tools/gen_needle.py rasterises the blade with 4x supersampling and writes
 * gauge/assets/needle_argb8888.bin.  It ships as white pixels plus an alpha
 * mask so the theme can recolour it via LVGL's image_recolor.
 */
#pragma once

#include "lvgl.h"

#include "gauge_needle_size.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Descriptor backed by the embedded blob, bound on first call.  Returns NULL
 * if the blob size does not match GAUGE_NEEDLE_BYTES, which means the asset
 * needs regenerating with `python tools/gen_needle.py`.
 */
const lv_image_dsc_t *gauge_needle_image(void);

#ifdef __cplusplus
}
#endif
