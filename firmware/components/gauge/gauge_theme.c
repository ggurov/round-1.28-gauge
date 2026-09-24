/*
 * gauge_theme.c - built-in palettes.
 *
 * Colours are 0xRRGGBB and line up with tools/render_preview.py so the
 * host-rendered previews and the real panel agree.
 */
#include "gauge_theme.h"

const gauge_theme_t gauge_theme_greddy = {
    .face            = 0x000000,
    .bezel           = 0xE8E8E8,

    .tick_major      = 0x2BE06A,
    .tick_minor      = 0x27C05C,
    .label           = 0x46F08A,
    .band            = 0x2BE06A,
    .band_glow       = 0x0E8B3C,
    .alarm           = 0xFF1A1A,

    .needle          = 0xFF3B0A,
    .needle_hub      = 0x0A0A0A,
    .needle_hub_ring = 0x333333,

    .value           = 0x5CFF9E,
    .caption         = 0x3BE87C,
    .unit            = 0x27B85E,
    .wordmark        = 0x46F08A,
    .tagline         = 0x1E9C4E,
    .status          = 0x7C7C7C,

    .font_label      = GAUGE_FONT_20,
    .font_caption    = GAUGE_FONT_16,
    .font_value      = GAUGE_FONT_30,
    .font_unit       = GAUGE_FONT_12,
    .font_wordmark   = GAUGE_FONT_14,
    .font_tagline    = GAUGE_FONT_10,

    .band_gap        = 3,
    .band_width      = 5,
    .alarm_gap       = 1,
    .alarm_width     = 4,
    .tick_gap        = 1,
    .tick_major_len  = 15,
    .tick_minor_len  = 7,
    .tick_major_width = 4,      /* half-width of the wedge */
    .tick_minor_width = 1,
    .bezel_width     = 4,
    .hub_radius      = 18,
    .label_pad_radial = 3,
    .label_letter_space = 0,
};

const gauge_theme_t gauge_theme_amber = {
    .face            = 0x000000,
    .bezel           = 0xD0D0D0,

    .tick_major      = 0xFFA000,
    .tick_minor      = 0xB87400,
    .label           = 0xFFB733,
    .band            = 0xFFA000,
    .band_glow       = 0x8B4A00,
    .alarm           = 0xFF2D2D,

    .needle          = 0xFF3B0A,
    .needle_hub      = 0x0A0A0A,
    .needle_hub_ring = 0x333333,

    .value           = 0xFFC24D,
    .caption         = 0xFFA000,
    .unit            = 0xB87400,
    .wordmark        = 0xFFB733,
    .tagline         = 0x8B5A00,
    .status          = 0x8A7A5A,

    .font_label      = GAUGE_FONT_20,
    .font_caption    = GAUGE_FONT_16,
    .font_value      = GAUGE_FONT_30,
    .font_unit       = GAUGE_FONT_12,
    .font_wordmark   = GAUGE_FONT_14,
    .font_tagline    = GAUGE_FONT_10,

    .band_gap        = 3,
    .band_width      = 5,
    .alarm_gap       = 1,
    .alarm_width     = 4,
    .tick_gap        = 1,
    .tick_major_len  = 15,
    .tick_minor_len  = 7,
    .tick_major_width = 4,
    .tick_minor_width = 1,
    .bezel_width     = 4,
    .hub_radius      = 18,
    .label_pad_radial = 3,
    .label_letter_space = 0,
};

int gauge_theme_font_index(gauge_font_t f)
{
    if ((int)f < 0 || f >= GAUGE_FONT_COUNT) {
        return (int)GAUGE_FONT_16;
    }
    return (int)f;
}
