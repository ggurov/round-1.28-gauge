/*
 * gfx_text.h - bitmap text over the gfx framebuffer.
 *
 * The fonts are generated from a system TTF by tools/gen_font.py into 8-bit
 * coverage masks.  No font library runs on the device.
 *
 * Coordinates are baselines, not box tops.  A glyph's ink sits between the
 * baseline and roughly `cap_height` above it, and the ascent includes room that
 * digits and capitals never use - so centring on the line height puts text
 * visibly low.  Use gfx_text_cap_centered() when you want text to look
 * centred.
 */
#pragma once

#include <stdint.h>

#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  w;          /* ink width  */
    uint8_t  h;          /* ink height */
    int8_t   bearing_x;  /* ink left edge relative to the pen */
    uint8_t  advance;    /* pen advance */
    uint32_t offset;     /* into the font's alpha blob */
    int8_t   bearing_y;  /* ink top relative to the baseline */
} gfx_glyph_t;

typedef struct {
    uint8_t  first;       /* first character code covered */
    uint8_t  count;       /* number of glyphs */
    uint8_t  line_height;
    uint8_t  ascent;
    uint8_t  cap_height;  /* ink height of a digit; use this to centre text */
    const gfx_glyph_t *glyphs;
    const uint8_t *alpha;
} gfx_font_t;

/* Width in pixels of `text` rendered with `font`. */
int gfx_text_width(const char *text, const gfx_font_t *font);

/* Draw with the baseline at `baseline_y` and the pen at `x`. */
void gfx_text(int x, int baseline_y, const char *text, const gfx_font_t *font,
              uint16_t colour);

/* Horizontally centred on `cx`, baseline at `baseline_y`. */
void gfx_text_centered(int cx, int baseline_y, const char *text, const gfx_font_t *font,
                       uint16_t colour);

/* Right-aligned to `x_right`, baseline at `baseline_y`. */
void gfx_text_right(int x_right, int baseline_y, const char *text, const gfx_font_t *font,
                    uint16_t colour);

/*
 * Centred both ways on (cx, cy) using the cap height, so every string in a row
 * sits on the same visual line regardless of which letters it happens to
 * contain.
 */
void gfx_text_cap_centered(int cx, int cy, const char *text, const gfx_font_t *font,
                           uint16_t colour);

#ifdef __cplusplus
}
#endif
