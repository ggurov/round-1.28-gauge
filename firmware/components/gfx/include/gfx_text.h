/*
 * gfx_text.h - bitmap text over the gfx framebuffer.
 *
 * The fonts are generated from a system TTF by tools/gen_font.py into 8-bit
 * coverage masks.  No font library runs on the device.
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
    uint8_t  first;      /* first character code covered */
    uint8_t  count;      /* number of glyphs */
    uint8_t  line_height;
    uint8_t  ascent;
    const gfx_glyph_t *glyphs;
    const uint8_t *alpha;
} gfx_font_t;

/* Width in pixels of `text` rendered with `font`. */
int gfx_text_width(const char *text, const gfx_font_t *font);

/* Draw with the top-left of the text box at (x, y). */
void gfx_text(int x, int y, const char *text, const gfx_font_t *font, uint16_t colour);

/* Draw with the horizontal centre at `cx`. */
void gfx_text_centered(int cx, int y, const char *text, const gfx_font_t *font,
                       uint16_t colour);

/* Draw right-aligned to `x_right`. */
void gfx_text_right(int x_right, int y, const char *text, const gfx_font_t *font,
                    uint16_t colour);

#ifdef __cplusplus
}
#endif
