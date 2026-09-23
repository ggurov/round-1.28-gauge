/*
 * gauge_theme.c - built-in palettes.
 */
#include "gauge_theme.h"

/*
 * GReddy-inspired 1980s/90s Japanese instrument look:
 *   black dial, phosphor-green scale, glowing green rail, magenta warning
 *   sector, red-orange blade needle, brushed-silver bezel.
 *
 * The wordmark shipped with the RPM preset is our own - the palette and
 * layout are a homage, not a copy of anyone's trademark.
 */
const gauge_theme_t gauge_theme_greddy = {
    .face         = {.full = 0x000000},
    .face_inner   = {.full = 0x070B08},
    .bezel        = {.full = 0xE8E8E8},
    .bezel_shadow = {.full = 0x2B2B2B},

    .tick_major   = {.full = 0x2BE06A},
    .tick_minor   = {.full = 0x27C05C},
    .label        = {.full = 0x46F08A},
    .band         = {.full = 0x2BE06A},
    .band_glow    = {.full = 0x0E8B3C},
    .alarm        = {.full = 0xFF2D9E},

    .needle       = {.full = 0xFF3B0A},
    .needle_hub   = {.full = 0x0A0A0A},
    .needle_hub_ring = {.full = 0x333333},

    .value        = {.full = 0x5CFF9E},
    .caption      = {.full = 0x3BE87C},
    .unit         = {.full = 0x27B85E},
    .wordmark     = {.full = 0x46F08A},
    .tagline      = {.full = 0x1E9C4E},

    .font_label   = &lv_font_montserrat_20,
    .font_caption = &lv_font_montserrat_16,
    .font_value   = &lv_font_montserrat_36,
    .font_unit    = &lv_font_montserrat_12,
    .font_wordmark = &lv_font_montserrat_16,
    .font_tagline = &lv_font_montserrat_12,

    .tick_major_len   = 18,
    .tick_minor_len   = 9,
    .tick_major_width = 3,
    .tick_minor_width = 1,
    .band_width       = 5,
    .label_pad_radial = 12,
    .bezel_width      = 4,
    .hub_radius       = 20,
};

/* Amber-on-black motorsport variant; same geometry, different phosphor. */
const gauge_theme_t gauge_theme_amber = {
    .face         = {.full = 0x000000},
    .face_inner   = {.full = 0x0B0800},
    .bezel        = {.full = 0xD0D0D0},
    .bezel_shadow = {.full = 0x2B2B2B},

    .tick_major   = {.full = 0xFFA000},
    .tick_minor   = {.full = 0xB87400},
    .label        = {.full = 0xFFB733},
    .band         = {.full = 0xFFA000},
    .band_glow    = {.full = 0x8B4A00},
    .alarm        = {.full = 0xFF2D2D},

    .needle       = {.full = 0xFF3B0A},
    .needle_hub   = {.full = 0x0A0A0A},
    .needle_hub_ring = {.full = 0x333333},

    .value        = {.full = 0xFFC24D},
    .caption      = {.full = 0xFFA000},
    .unit         = {.full = 0xB87400},
    .wordmark     = {.full = 0xFFB733},
    .tagline      = {.full = 0x8B5A00},

    .font_label   = &lv_font_montserrat_20,
    .font_caption = &lv_font_montserrat_16,
    .font_value   = &lv_font_montserrat_36,
    .font_unit    = &lv_font_montserrat_12,
    .font_wordmark = &lv_font_montserrat_16,
    .font_tagline = &lv_font_montserrat_12,

    .tick_major_len   = 18,
    .tick_minor_len   = 9,
    .tick_major_width = 3,
    .tick_minor_width = 1,
    .band_width       = 5,
    .label_pad_radial = 12,
    .bezel_width      = 4,
    .hub_radius       = 20,
};
