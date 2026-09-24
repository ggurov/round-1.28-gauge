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

TF_TEST(gauge_theme, ticks_hang_inside_the_rail)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        const int rail = gauge_math_rail_radius(DIAL, t->bezel_width, t->band_gap, t->band_width);
        const int tick_base = gauge_math_tick_base_radius(rail, t->band_width);

        TF_CHECK_MSG(tick_base <= rail - t->band_width,
                     "%s: ticks start at %d, inside the rail ending at %d",
                     theme_name(t), tick_base, rail - t->band_width);
        TF_GE(tick_base, 20);
    }
}

TF_TEST(gauge_theme, warning_sector_is_inboard_of_the_ticks)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        const int rail = gauge_math_rail_radius(DIAL, t->bezel_width, t->band_gap, t->band_width);
        const int tick_base = gauge_math_tick_base_radius(rail, t->band_width);
        const int tick_inner = tick_base - t->tick_major_len;
        const int alarm_out = gauge_math_alarm_outer_radius(tick_base, t->tick_major_len,
                                                            t->alarm_gap);
        const int alarm_in = alarm_out - t->alarm_width;

        TF_CHECK_MSG(alarm_out < tick_inner,
                     "%s: warning sector at %d overlaps the ticks (inner end %d)",
                     theme_name(t), alarm_out, tick_inner);
        TF_CHECK_MSG(alarm_in > 0, "%s: warning sector has no room", theme_name(t));
    }
}

TF_TEST(gauge_theme, needle_stops_short_of_the_warning_sector)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        const int rail = gauge_math_rail_radius(DIAL, t->bezel_width, t->band_gap, t->band_width);
        const int tick_base = gauge_math_tick_base_radius(rail, t->band_width);
        const int alarm_out = gauge_math_alarm_outer_radius(tick_base, t->tick_major_len,
                                                            t->alarm_gap);
        const int alarm_in = alarm_out - t->alarm_width;
        const int needle = alarm_in - 2;

        TF_GE(needle, 20);
        TF_CHECK_MSG(needle <= alarm_in,
                     "%s: needle tip %d reaches into the warning sector", theme_name(t), needle);
    }
}

TF_TEST(gauge_theme, numerals_fit_inside_the_warning_sector)
{
    for (size_t i = 0; i < THEME_COUNT; i++) {
        const gauge_theme_t *t = all_themes[i];
        const int rail = gauge_math_rail_radius(DIAL, t->bezel_width, t->band_gap, t->band_width);
        const int tick_base = gauge_math_tick_base_radius(rail, t->band_width);
        const int alarm_out = gauge_math_alarm_outer_radius(tick_base, t->tick_major_len,
                                                            t->alarm_gap);
        const int alarm_in = alarm_out - t->alarm_width;
        const int cap = 16;   /* gfx_font_label cap height */
        const int label_r = alarm_in - 3 - cap / 2;

        TF_CHECK_MSG(label_r + cap / 2 < alarm_in,
                     "%s: numerals reach the warning sector", theme_name(t));
        TF_CHECK_MSG(label_r - cap / 2 > t->hub_radius,
                     "%s: numerals collide with the hub (%d vs %d)",
                     theme_name(t), label_r - cap / 2, t->hub_radius);
    }
}
