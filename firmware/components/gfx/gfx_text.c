/*
 * gfx_text.c - draws generated alpha-mask glyphs into the framebuffer.
 *
 * Coverage is used as a blend factor, so the edges of the generated type stay
 * smooth instead of turning into stair-steps.
 */
#include "gfx_text.h"

#include <string.h>

#include "gfx_font_data.h"

static const gfx_glyph_t *glyph_for(const gfx_font_t *font, char c)
{
    const unsigned idx = (unsigned char)c - font->first;
    if (idx >= font->count) {
        return NULL;
    }
    return &font->glyphs[idx];
}

int gfx_text_width(const char *text, const gfx_font_t *font)
{
    if (!text || !font) {
        return 0;
    }
    int pen = 0;
    for (const char *p = text; *p; p++) {
        const gfx_glyph_t *g = glyph_for(font, *p);
        pen += g ? g->advance : font->line_height / 3;
    }
    return pen;
}

void gfx_text(int x, int baseline_y, const char *text, const gfx_font_t *font, uint16_t colour)
{
    if (!text || !font) {
        return;
    }

    int pen = x;

    for (const char *p = text; *p; p++) {
        const gfx_glyph_t *g = glyph_for(font, *p);
        if (!g) {
            pen += font->line_height / 3;
            continue;
        }
        if (g->w == 0 || g->h == 0) {
            pen += g->advance;
            continue;
        }

        const int gx = pen + g->bearing_x;
        const int gy = baseline_y + g->bearing_y;
        const uint8_t *src = &font->alpha[g->offset];

        for (int row = 0; row < g->h; row++) {
            const int py = gy + row;
            if ((unsigned)py >= GFX_H) {
                src += g->w;
                continue;
            }
            uint16_t *dst = &gfx_framebuffer()[py * GFX_W];
            for (int col = 0; col < g->w; col++) {
                const uint8_t a = src[col];
                if (a == 0) {
                    continue;
                }
                const int px = gx + col;
                if ((unsigned)px >= GFX_W) {
                    continue;
                }
                dst[px] = (a >= 250) ? colour : gfx_blend(dst[px], colour, a);
            }
            src += g->w;
        }

        pen += g->advance;
    }
}

void gfx_text_centered(int cx, int baseline_y, const char *text, const gfx_font_t *font,
                       uint16_t colour)
{
    gfx_text(cx - gfx_text_width(text, font) / 2, baseline_y, text, font, colour);
}

void gfx_text_right(int x_right, int baseline_y, const char *text, const gfx_font_t *font,
                    uint16_t colour)
{
    gfx_text(x_right - gfx_text_width(text, font), baseline_y, text, font, colour);
}

void gfx_text_cap_centered(int cx, int cy, const char *text, const gfx_font_t *font,
                           uint16_t colour)
{
    if (!font) {
        return;
    }
    /* put the middle of the cap band on cy: baseline = cy + cap/2 */
    const int baseline = cy + font->cap_height / 2;
    gfx_text(cx - gfx_text_width(text, font) / 2, baseline, text, font, colour);
}
