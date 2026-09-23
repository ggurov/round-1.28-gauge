/*
 * test_gauge_math.c - the arithmetic the dial is built from.
 *
 * These are the numbers that decide where every tick, numeral and needle ends
 * up, so they are worth pinning down precisely.
 */
#include <string.h>

#include "gauge_math.h"
#include "test_framework.h"

/* Mirrors gauge_preset_rpm() */
static const gauge_scale_t RPM = {
    .min = 0.0f, .max = 8000.0f, .major_step = 1000.0f,
    .minor_per_major = 4, .angle_range = 270, .rotation = 135, .decimals = 0,
};

/* Mirrors gauge_preset_temp() */
static const gauge_scale_t TEMP = {
    .min = 50.0f, .max = 150.0f, .major_step = 10.0f,
    .minor_per_major = 2, .angle_range = 270, .rotation = 135, .decimals = 0,
};

/* Mirrors gauge_preset_boost() */
static const gauge_scale_t BOOST = {
    .min = -1.0f, .max = 2.0f, .major_step = 0.5f,
    .minor_per_major = 5, .angle_range = 270, .rotation = 135, .decimals = 1,
};

/* -------------------------------------------------------------------------- */

TF_TEST(gauge_math, fraction_clamps_to_range)
{
    TF_NEAR(gauge_math_fraction(&RPM, -500.0f), 0.0, 1e-6);
    TF_NEAR(gauge_math_fraction(&RPM, 0.0f), 0.0, 1e-6);
    TF_NEAR(gauge_math_fraction(&RPM, 4000.0f), 0.5, 1e-6);
    TF_NEAR(gauge_math_fraction(&RPM, 8000.0f), 1.0, 1e-6);
    TF_NEAR(gauge_math_fraction(&RPM, 99999.0f), 1.0, 1e-6);
}

TF_TEST(gauge_math, fraction_handles_negative_range)
{
    TF_NEAR(gauge_math_fraction(&BOOST, -1.0f), 0.0, 1e-6);
    TF_NEAR(gauge_math_fraction(&BOOST, 0.5f), 0.5, 1e-6);
    TF_NEAR(gauge_math_fraction(&BOOST, 2.0f), 1.0, 1e-6);
}

TF_TEST(gauge_math, fraction_of_degenerate_range_is_zero)
{
    gauge_scale_t z = { .min = 5.0f, .max = 5.0f, .major_step = 1.0f };
    TF_NEAR(gauge_math_fraction(&z, 5.0f), 0.0, 1e-6);
    TF_NEAR(gauge_math_fraction(&z, 9.0f), 0.0, 1e-6);
}

TF_TEST(gauge_math, value_to_angle_endpoints)
{
    /* rotation 135 + sweep 270, then +90 to convert to "clockwise from 12". */
    TF_NEAR(gauge_math_value_to_angle(&RPM, 0.0f), 225.0, 1e-3);
    TF_NEAR(gauge_math_value_to_angle(&RPM, 8000.0f), 495.0, 1e-3);
}

TF_TEST(gauge_math, value_to_angle_midpoint)
{
    /* 4000 rpm sits at 12 o'clock, i.e. 360 deg in this convention. */
    TF_NEAR(gauge_math_value_to_angle(&RPM, 4000.0f), 360.0, 1e-3);
    /* rotation 135, so a quarter of the sweep lands at 135+67.5+90 = 292.5 */
    TF_NEAR(gauge_math_value_to_angle(&RPM, 2000.0f), 292.5, 1e-3);
    TF_NEAR(gauge_math_value_to_angle(&RPM, 6000.0f), 427.5, 1e-3);
}

TF_TEST(gauge_math, value_to_angle_is_monotonic)
{
    float prev = -1e9f;
    for (int i = 0; i <= 100; i++) {
        float v = 8000.0f * (float)i / 100.0f;
        float a = gauge_math_value_to_angle(&RPM, v);
        TF_GE(a, prev);
        prev = a;
    }
}

TF_TEST(gauge_math, value_to_angle_sweeps_exactly_angle_range)
{
    float span = gauge_math_value_to_angle(&RPM, RPM.max) - gauge_math_value_to_angle(&RPM, RPM.min);
    TF_NEAR(span, 270.0, 1e-3);
}

TF_TEST(gauge_math, value_to_angle_respects_custom_geometry)
{
    gauge_scale_t s = {
        .min = 0.0f, .max = 100.0f, .major_step = 10.0f,
        .angle_range = 180, .rotation = 180, .decimals = 0,
    };
    TF_NEAR(gauge_math_value_to_angle(&s, 0.0f), 270.0, 1e-3);   /* 9 o'clock */
    TF_NEAR(gauge_math_value_to_angle(&s, 100.0f), 450.0, 1e-3); /* 9 -> 3 */
}

/* -------------------------------------------------------------------------- */

TF_TEST(gauge_math, major_ticks_counts_labels)
{
    TF_EQ_INT(gauge_math_major_ticks(&RPM), 9);    /* 0..8   */
    TF_EQ_INT(gauge_math_major_ticks(&TEMP), 11);  /* 50..150 */
    TF_EQ_INT(gauge_math_major_ticks(&BOOST), 7);  /* -1..2  */
}

TF_TEST(gauge_math, total_ticks_includes_minors)
{
    TF_EQ_INT(gauge_math_total_ticks(&RPM), (9 - 1) * 4 + 1);   /* 33 */
    TF_EQ_INT(gauge_math_total_ticks(&TEMP), (11 - 1) * 2 + 1); /* 21 */
    TF_EQ_INT(gauge_math_total_ticks(&BOOST), (7 - 1) * 5 + 1); /* 31 */
}

TF_TEST(gauge_math, total_ticks_is_odd_so_majors_land_on_both_ends)
{
    /* major_tick_every only works out if the tick count is majors*minor+1 */
    const gauge_scale_t *all[] = { &RPM, &TEMP, &BOOST };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        int total = gauge_math_total_ticks(all[i]);
        TF_EQ_INT((total - 1) % all[i]->minor_per_major, 0);
        TF_EQ_INT((total - 1) / all[i]->minor_per_major + 1, gauge_math_major_ticks(all[i]));
    }
}

TF_TEST(gauge_math, major_ticks_is_clamped)
{
    gauge_scale_t tiny = { .min = 0.0f, .max = 1.0f, .major_step = 0.001f };
    TF_LE(gauge_math_major_ticks(&tiny), GAUGE_MAX_TICKS);
    gauge_scale_t zero = { .min = 0.0f, .max = 100.0f, .major_step = 0.0f };
    TF_GE(gauge_math_major_ticks(&zero), 2);
}

TF_TEST(gauge_math, major_value_steps_across_range)
{
    TF_NEAR(gauge_math_major_value(&TEMP, 0), 50.0, 1e-6);
    TF_NEAR(gauge_math_major_value(&TEMP, 10), 150.0, 1e-6);
    TF_NEAR(gauge_math_major_value(&BOOST, 0), -1.0, 1e-6);
    TF_NEAR(gauge_math_major_value(&BOOST, 6), 2.0, 1e-6);
}

/* -------------------------------------------------------------------------- */

TF_TEST(gauge_math, format_honours_decimals)
{
    char buf[GAUGE_LABEL_LEN];

    gauge_math_format(&RPM, 4000.0f, buf, sizeof(buf));
    TF_STR_EQ(buf, "4000");

    gauge_math_format(&RPM, 4250.6f, buf, sizeof(buf));
    TF_STR_EQ(buf, "4251");   /* rounds, does not truncate */

    gauge_math_format(&BOOST, 0.0f, buf, sizeof(buf));
    TF_STR_EQ(buf, "0.0");

    gauge_math_format(&BOOST, -0.5f, buf, sizeof(buf));
    TF_STR_EQ(buf, "-0.5");
}

TF_TEST(gauge_math, major_label_is_the_raw_value)
{
    char buf[GAUGE_LABEL_LEN];

    /*
     * The generator always labels with the engineering value.  A gauge that
     * wants "0".."8" on the dial for a 0..8000 rpm range supplies those through
     * gauge_config_t::tick_labels instead (see gauge_presets.c), so this must
     * NOT be shortened to "8".
     */
    gauge_math_major_label(&RPM, 0, buf, sizeof(buf));
    TF_STR_EQ(buf, "0");
    gauge_math_major_label(&RPM, 8, buf, sizeof(buf));
    TF_STR_EQ(buf, "8000");

    gauge_math_major_label(&TEMP, 0, buf, sizeof(buf));
    TF_STR_EQ(buf, "50");
    gauge_math_major_label(&TEMP, 10, buf, sizeof(buf));
    TF_STR_EQ(buf, "150");

    gauge_math_major_label(&BOOST, 0, buf, sizeof(buf));
    TF_STR_EQ(buf, "-1.0");
    gauge_math_major_label(&BOOST, 3, buf, sizeof(buf));
    TF_STR_EQ(buf, "0.5");
}

TF_TEST(gauge_math, every_generated_label_fits_the_buffer)
{
    const gauge_scale_t *all[] = { &RPM, &TEMP, &BOOST };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        int majors = gauge_math_major_ticks(all[i]);
        for (int m = 0; m < majors; m++) {
            char buf[GAUGE_LABEL_LEN];
            int n = gauge_math_major_label(all[i], m, buf, sizeof(buf));
            TF_GE(n, 1);
            /* snprintf returns what it *would* have written; it must fit */
            TF_LE(n, GAUGE_LABEL_LEN - 1);
        }
    }
}

/* -------------------------------------------------------------------------- */

TF_TEST(gauge_math, rail_radius_math)
{
    /* 240 dial, 4 px bezel, 3 px gap, 5 px band -> 120 - (4+3+2) = 111 */
    TF_EQ_INT(gauge_math_rail_radius(240, 4, 3, 5), 111);
    TF_EQ_INT(gauge_math_scale_diameter(111), 222);
}

TF_TEST(gauge_math, rail_radius_leaves_room_for_the_band)
{
    int r = gauge_math_rail_radius(240, 4, 3, 5);
    /* rail + half the band must stay inside the bezel */
    TF_LE(r + 5 / 2, 240 / 2 - 4);
}

TF_TEST(gauge_math, label_radius_matches_lvgl_layout)
{
    /* radius - major_len - (pad_radial + LV_SCALE_DEFAULT_LABEL_GAP + spacing) */
    TF_EQ_INT(gauge_math_label_radius(111, 16, 3, 0), 111 - 16 - 3 - GAUGE_LABEL_GAP);
    TF_EQ_INT(gauge_math_label_radius(111, 16, 3, 0), 77);
}

TF_TEST(gauge_math, pad_radial_is_the_inverse_of_label_radius)
{
    TF_EQ_INT(gauge_math_pad_radial_for(111, 16, 77, 0), 3);
    /*
     * pad_radial clamps at zero, so the largest reachable numeral radius is
     * rail - major_len - GAUGE_LABEL_GAP == 80 here.  Stay inside that.
     */
    for (int wanted = 40; wanted <= 80; wanted += 5) {
        int pad = gauge_math_pad_radial_for(111, 16, wanted, 0);
        TF_EQ_INT(gauge_math_label_radius(111, 16, pad, 0), wanted);
    }
}

TF_TEST(gauge_math, pad_radial_never_goes_negative)
{
    TF_EQ_INT(gauge_math_pad_radial_for(111, 16, 500, 0), 0);
}

TF_TEST(gauge_math, needle_length_stops_inside_the_major_ticks)
{
    int rail = gauge_math_rail_radius(240, 4, 3, 5);
    int len = gauge_math_needle_length(rail, 16);
    TF_EQ_INT(len, rail - 16 - 2);
    /* tip must not reach the tick band */
    TF_LE(len, rail - 16);
}

TF_TEST(gauge_math, needle_clears_the_numerals_and_hub)
{
    int rail = gauge_math_rail_radius(240, 4, 3, 5);
    int needle = gauge_math_needle_length(rail, 16);
    int label_r = gauge_math_label_radius(rail, 16, 3, 0);
    int label_outer = label_r + 12;   /* half a 20 px glyph */

    TF_GE(needle, label_outer);       /* tip reaches past the numerals */
    TF_GE(label_r - 12, 18);          /* numerals clear the 18 px hub  */
}

/* -------------------------------------------------------------------------- */

TF_TEST(gauge_math, slew_alpha_is_a_sane_fraction)
{
    TF_GE(gauge_math_slew_alpha(0.016f, 0.35f), 0.0);
    TF_LE(gauge_math_slew_alpha(0.016f, 0.35f), 1.0);
    /* a longer time constant means a smaller step */
    TF_LE(gauge_math_slew_alpha(0.016f, 2.0f), gauge_math_slew_alpha(0.016f, 0.3f));
    /* zero slew time means jump straight there */
    TF_NEAR(gauge_math_slew_alpha(0.016f, 0.0f), 1.0, 1e-6);
}

TF_TEST(gauge_math, slew_max_step_caps_full_scale_time)
{
    /* 8000 units in 0.30 s, sampled every 16 ms -> 426.7 units per tick */
    TF_NEAR(gauge_math_slew_max_step(8000.0f, 0.016f, 0.30f), 8000.0 * 0.016 / 0.30, 1e-3);
}

TF_TEST(gauge_math, slew_step_is_rate_limited)
{
    float alpha = gauge_math_slew_alpha(0.016f, 0.30f);
    float max_step = gauge_math_slew_max_step(8000.0f, 0.016f, 0.30f);
    float next = gauge_math_slew_step(0.0f, 8000.0f, alpha, max_step, 0.8f);
    TF_NEAR(next, max_step, 1e-3);   /* the cap binds, not the exponential */
    TF_LE(next - 0.0f, max_step + 1e-3);
}

TF_TEST(gauge_math, slew_step_never_overshoots)
{
    float alpha = gauge_math_slew_alpha(0.016f, 0.30f);
    float max_step = gauge_math_slew_max_step(8000.0f, 0.016f, 0.30f);
    float v = 0.0f;
    for (int i = 0; i < 100000; i++) {
        v = gauge_math_slew_step(v, 8000.0f, alpha, max_step, 0.8f);
        TF_GE(v, 0.0);
        TF_LE(v, 8000.0);
        if (v == 8000.0f) {
            break;
        }
    }
    TF_NEAR(v, 8000.0, 1e-3);
}

TF_TEST(gauge_math, slew_step_converges_within_the_configured_time)
{
    const float slew_time = 0.30f;
    const float dt = 0.016f;
    float alpha = gauge_math_slew_alpha(dt, slew_time);
    float max_step = gauge_math_slew_max_step(8000.0f, dt, slew_time);
    float epsilon = 8000.0f * 1e-4f;

    float v = 0.0f;
    int ticks = 0;
    while (v < 8000.0f && ticks < 10000) {
        v = gauge_math_slew_step(v, 8000.0f, alpha, max_step, epsilon);
        ticks++;
    }
    /*
     * A full-scale move should take about slew_time: the rate limit covers the
     * bulk of it and the exponential only adds the final settle.
     */
    float seconds = (float)ticks * dt;
    TF_CHECK_MSG(seconds <= slew_time * 1.6f,
                 "full-scale move took %.3f s, wanted <= %.3f s",
                 (double)seconds, (double)(slew_time * 1.6f));
    TF_GE(seconds, slew_time * 0.8f);
}

TF_TEST(gauge_math, slew_step_snaps_to_target_inside_epsilon)
{
    float next = gauge_math_slew_step(100.0f, 100.5f, 0.2f, 50.0f, 1.0f);
    TF_NEAR(next, 100.5, 1e-6);   /* within epsilon -> exact target */
}

TF_TEST(gauge_math, slew_step_handles_downward_moves)
{
    float alpha = gauge_math_slew_alpha(0.016f, 0.30f);
    float max_step = gauge_math_slew_max_step(8000.0f, 0.016f, 0.30f);
    float next = gauge_math_slew_step(8000.0f, 0.0f, alpha, max_step, 0.8f);
    TF_NEAR(next, 8000.0f - max_step, 1e-3);
}

/* -------------------------------------------------------------------------- */

TF_TEST(gauge_math, from_config_applies_documented_defaults)
{
    gauge_config_t cfg = {
        .min = 0.0f, .max = 100.0f, .major_step = 10.0f,
        .angle_range = 0, .rotation = 0, .minor_per_major = 0, .decimals = 2,
    };
    gauge_scale_t s;
    gauge_math_from_config(&cfg, &s);

    TF_EQ_INT(s.angle_range, 270);
    TF_EQ_INT(s.rotation, 135);
    TF_EQ_INT(s.minor_per_major, 4);
    TF_EQ_INT(s.decimals, 2);
    TF_NEAR(s.min, 0.0, 1e-9);
    TF_NEAR(s.max, 100.0, 1e-9);
}

TF_TEST(gauge_math, from_config_preserves_explicit_values)
{
    gauge_config_t cfg = {
        .min = 50.0f, .max = 150.0f, .major_step = 10.0f,
        .angle_range = 180, .rotation = 90, .minor_per_major = 2, .decimals = 0,
    };
    gauge_scale_t s;
    gauge_math_from_config(&cfg, &s);

    TF_EQ_INT(s.angle_range, 180);
    TF_EQ_INT(s.rotation, 90);
    TF_EQ_INT(s.minor_per_major, 2);
}
