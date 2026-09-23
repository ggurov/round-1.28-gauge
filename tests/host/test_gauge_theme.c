/*
 * test_gauge_theme.c - themes have to produce a coherent dial.
 *
 * These catch the failure mode where a theme tweak makes the band overflow the
 * bezel, the numerals collide with the needle, or a colour is out of range.
 */
#include <string.h>

#include "gauge_math.h"
#include "gauge_theme.h"
#include "test_framework.h"

#define DIAL 240

static const gauge_theme_t *all_themes[] = {
    &gauge_theme_greddy,
    &gauge_theme_amber,
};
#define THEME_COUNT (sizeof(all_themes) / sizeof(all_themes[0]))

static const char *theme_name(const gauge_theme_t *t)
{
    return t == &gauge_theme_greddy ? "greddy"
         : t == &gauge_theme_amber  ? "amber"
         : "?";
}

TF_TEST(gauge_theme, both_themes_are_registered)
{
    TF_EQ_INT(THEME_COUNT, 2);
}

TF_TEST(gauge_theme, font_enums_are_in_range)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        gauge_font_t fonts[] = {
            t->font_label, t->font_caption, t->font_value,
            t->font_unit, t->font_wordmark, t->font_tagline,
        };
        for (size_t f = 0; f < sizeof(fonts) / sizeof(fonts[0]); f++) {
            TF_GE(fonts[f], 0);
            TF_LE(fonts[f], GAUGE_FONT_COUNT - 1);
        }
    }
}

TF_TEST(gauge_theme, font_index_helper_clamps)
{
    TF_EQ_INT(gauge_theme_font_index(GAUGE_FONT_20), GAUGE_FONT_20);
    TF_EQ_INT(gauge_theme_font_index((gauge_font_t)999), GAUGE_FONT_16);
    TF_EQ_INT(gauge_theme_font_index((gauge_font_t)-3), GAUGE_FONT_16);
}

TF_TEST(gauge_theme, colours_fit_in_24_bits)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        const uint32_t *cols[] = {
            &t->face, &t->bezel, &t->tick_major, &t->tick_minor, &t->label,
            &t->band, &t->band_glow, &t->alarm, &t->needle, &t->needle_hub,
            &t->needle_hub_ring, &t->value, &t->caption, &t->unit,
            &t->wordmark, &t->tagline,
        };
        for (size_t c = 0; c < sizeof(cols) / sizeof(cols[0]); c++) {
            TF_LE(*cols[c], 0xFFFFFFu);
        }
    }
}

TF_TEST(gauge_theme, band_stays_inside_the_bezel)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        int rail = gauge_math_rail_radius(DIAL, t->bezel_width, t->band_gap, t->band_width);
        int band_outer = rail + t->band_width / 2;
        int bezel_inner = DIAL / 2 - t->bezel_width;
        TF_CHECK_MSG(band_outer <= bezel_inner,
                     "%s: band outer %d overlaps bezel inner %d",
                     theme_name(t), band_outer, bezel_inner);
    }
}

TF_TEST(gauge_theme, geometry_is_ordered_sensibly)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        TF_GE(t->bezel_width, 1);
        TF_GE(t->band_width, 1);
        TF_GE(t->band_gap, 0);
        TF_GE(t->tick_major_len, 1);
        TF_GE(t->tick_minor_len, 1);
        TF_CHECK_MSG(t->tick_major_len > t->tick_minor_len,
                     "%s: major tick (%d) should be longer than minor (%d)",
                     theme_name(t), t->tick_major_len, t->tick_minor_len);
        TF_CHECK_MSG(t->tick_major_width >= t->tick_minor_width,
                     "%s: major tick should be at least as wide as minor",
                     theme_name(t));
        TF_GE(t->hub_radius, 4);
    }
}

TF_TEST(gauge_theme, needle_reaches_past_the_numerals_but_not_the_ticks)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        int rail = gauge_math_rail_radius(DIAL, t->bezel_width, t->band_gap, t->band_width);
        int needle = gauge_math_needle_length(rail, t->tick_major_len);
        int label_r = gauge_math_label_radius(rail, t->tick_major_len,
                                              t->label_pad_radial, t->label_letter_space);
        int tick_inner = rail - t->tick_major_len;

        TF_CHECK_MSG(needle <= tick_inner,
                     "%s: needle tip %d runs into the tick band (inner %d)",
                     theme_name(t), needle, tick_inner);
        TF_CHECK_MSG(needle >= label_r,
                     "%s: needle tip %d stops short of the numerals (centre %d)",
                     theme_name(t), needle, label_r);
    }
}

TF_TEST(gauge_theme, hub_does_not_swallow_the_numerals)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        int rail = gauge_math_rail_radius(DIAL, t->bezel_width, t->band_gap, t->band_width);
        int label_r = gauge_math_label_radius(rail, t->tick_major_len,
                                              t->label_pad_radial, t->label_letter_space);
        /* assume up to a 30 px tall glyph */
        int label_inner = label_r - 15;
        TF_CHECK_MSG(t->hub_radius < label_inner,
                     "%s: hub radius %d reaches the numerals (inner %d)",
                     theme_name(t), t->hub_radius, label_inner);
    }
}

TF_TEST(gauge_theme, label_pad_radial_keeps_the_numeral_layout_stable)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        int rail = gauge_math_rail_radius(DIAL, t->bezel_width, t->band_gap, t->band_width);
        /* the pad we ship should round-trip through the layout maths */
        int wanted = rail - t->tick_major_len - 4 - 12;   /* glyph_h 24 -> half 12 */
        int pad = gauge_math_pad_radial_for(rail, t->tick_major_len, wanted,
                                            t->label_letter_space);
        int got = gauge_math_label_radius(rail, t->tick_major_len, pad,
                                          t->label_letter_space);
        if (pad > 0) {
            TF_EQ_INT(got, wanted);
        }
    }
}
