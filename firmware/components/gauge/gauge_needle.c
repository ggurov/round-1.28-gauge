/*
 * gauge_needle.c - binds the embedded needle blob to an lv_image_dsc_t.
 *
 * The pixel data is deliberately kept out of a generated .c file: the raw
 * bitmap is embedded with ESP-IDF's EMBED_FILES and wrapped in a descriptor at
 * runtime, which keeps the repository small and the compile fast.
 */
#include "gauge_needle.h"

#include <string.h>

/* The linker symbol name is derived from the path inside the component, with
 * every non-alphanumeric character replaced by '_'.  If this ever fails to
 * resolve, build once and read the real name out of the link error / nm. */
extern const uint8_t needle_blob_start[] asm("_binary_assets_needle_argb8888_bin_start");
extern const uint8_t needle_blob_end[] asm("_binary_assets_needle_argb8888_bin_end");

static lv_image_dsc_t s_needle_dsc;
static bool s_bound;

const lv_image_dsc_t *gauge_needle_image(void)
{
    if (!s_bound) {
        const size_t blob_len = (size_t)(needle_blob_end - needle_blob_start);
        const size_t expect = (size_t)GAUGE_NEEDLE_W * (size_t)GAUGE_NEEDLE_H * 4u;

        if (blob_len != expect) {
            /* Regenerate with `python tools/gen_needle.py`. */
            return NULL;
        }

        memset(&s_needle_dsc, 0, sizeof(s_needle_dsc));
        s_needle_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        s_needle_dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
        s_needle_dsc.header.flags = 0;
        s_needle_dsc.header.w = GAUGE_NEEDLE_W;
        s_needle_dsc.header.h = GAUGE_NEEDLE_H;
        s_needle_dsc.header.stride = GAUGE_NEEDLE_W * 4;
        s_needle_dsc.data_size = (uint32_t)blob_len;
        s_needle_dsc.data = needle_blob_start;
        s_bound = true;
    }
    return &s_needle_dsc;
}
