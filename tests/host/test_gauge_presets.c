/*
 * test_gauge_presets.c - every preset has to be a valid, renderable config.
 *
 * A malformed preset turns into a blank or half-drawn dial on the panel, which
 * is expensive to debug there and cheap to catch here.
 */
#include <stdio.h>
#include <string.h>

#include "gauge_math.h"
#include "gauge_presets.h"
#include "test_framework.h"

static const gauge_theme_t *known_themes[] = {
    &gauge_theme_greddy,
    &gauge_theme_amber,
};

static bool theme_is_known(const gauge_theme_t *t)
{
    for (size_t i = 0; i < sizeof(known_themes) / sizeof(known_themes[0]); i++) {
        if (t == known_themes[i]) {
            return true;
        }
    }
    return false;
}

/* -------------------------------------------------------------------------- */

TF_TEST(gauge_presets, table_is_terminated_and_counted)
{
    TF_NOT_NULL(gauge_presets_all());
    TF_GE(gauge_presets_count(), 1);

    int n = 0;
    for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
        n++;
    }
    TF_EQ_INT(n, gauge_presets_count());
    TF_EQ_INT(gauge_presets_all()[gauge_presets_count()].id == NULL, 1);
}

TF_TEST(gauge_presets, ids_are_unique_and_resolvable)
{
    int n = gauge_presets_count();
    for (int i = 0; i < n; i++) {
        const gauge_preset_t *p = gauge_preset_at(i);
        TF_NOT_NULL(p);
        TF_NOT_NULL(p->id);
        TF_NOT_NULL(p->name);
        TF_NOT_NULL(p->cfg);
        TF_CHECK_MSG(strlen(p->id) > 0, "preset %d has an empty id", i);

        /* lookup by id returns the same entry */
        const gauge_preset_t *found = gauge_preset_find(p->id);
        TF_CHECK_MSG(found == p, "gauge_preset_find(\"%s\") did not round-trip", p->id);

        /* ids must not collide */
        for (int j = i + 1; j < n; j++) {
            TF_CHECK_MSG(strcmp(p->id, gauge_preset_at(j)->id) != 0,
                         "duplicate preset id \"%s\"", p->id);
        }
    }
}

TF_TEST(gauge_presets, lookup_handles_missing_input)
{
    TF_CHECK(gauge_preset_find(NULL) == NULL);
    TF_CHECK(gauge_preset_find("") == NULL);
    TF_CHECK(gauge_preset_find("no-such-gauge") == NULL);
    TF_CHECK(gauge_preset_at(-1) == NULL);
    TF_CHECK(gauge_preset_at(gauge_presets_count()) == NULL);
}

TF_TEST(gauge_presets, named_accessors_are_in_the_table)
{
    const struct { const char *id; const gauge_config_t *cfg; } expected[] = {
        { "rpm",   gauge_preset_rpm()   },
        { "temp",  gauge_preset_temp()  },
        { "boost", gauge_preset_boost() },
        { "volts", gauge_preset_volts() },
    };
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        const gauge_preset_t *p = gauge_preset_find(expected[i].id);
        TF_NOT_NULL(p);
        TF_CHECK_MSG(p && p->cfg == expected[i].cfg,
                     "preset \"%s\" is not the object returned by its accessor",
                     expected[i].id);
    }
}

/* -------------------------------------------------------------------------- */

TF_TEST(gauge_presets, ranges_are_well_formed)
{
    for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
        const gauge_config_t *c = p->cfg;
        TF_CHECK_MSG(c->max > c->min, "%s: max (%g) must exceed min (%g)",
                     p->id, (double)c->max, (double)c->min);
        TF_CHECK_MSG(c->major_step > 0.0f, "%s: major_step must be positive", p->id);
        TF_GE(c->minor_per_major, 1);
        TF_GE(c->decimals, 0);
        TF_LE(c->decimals, 2);
        TF_CHECK_MSG(c->slew_time > 0.0f, "%s: slew_time must be positive", p->id);
        TF_NOT_NULL(c->caption);
        /* unit and the branding lines are optional */
        if (c->unit) {
            TF_CHECK_MSG(strlen(c->unit) < 40, "%s: unit text is unreasonable", p->id);
        }
        TF_NOT_NULL(c->theme);
        TF_CHECK_MSG(theme_is_known(c->theme), "%s: theme is not one of the built-ins", p->id);
    }
}

TF_TEST(gauge_presets, tick_budget_fits_the_label_storage)
{
    for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
        gauge_scale_t s;
        gauge_math_from_config(p->cfg, &s);
        int majors = gauge_math_major_ticks(&s);
        TF_CHECK_MSG(majors <= GAUGE_MAX_TICKS,
                     "%s: %d major ticks exceeds GAUGE_MAX_TICKS (%d)",
                     p->id, majors, GAUGE_MAX_TICKS);
        TF_CHECK_MSG(gauge_math_total_ticks(&s) <= 200,
                     "%s: %d total ticks is unreasonable", p->id,
                     gauge_math_total_ticks(&s));
    }
}

TF_TEST(gauge_presets, explicit_labels_match_the_tick_count)
{
    for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
        const gauge_config_t *c = p->cfg;
        if (!c->tick_labels) {
            continue;
        }
        gauge_scale_t s;
        gauge_math_from_config(c, &s);
        int majors = gauge_math_major_ticks(&s);

        int n = 0;
        while (c->tick_labels[n]) {
            n++;
        }
        TF_CHECK_MSG(n == majors,
                     "%s: %d explicit labels but %d major ticks",
                     p->id, n, majors);
        for (int i = 0; i < n; i++) {
            TF_CHECK_MSG(strlen(c->tick_labels[i]) < GAUGE_LABEL_LEN,
                         "%s: label \"%s\" does not fit in %d bytes",
                         p->id, c->tick_labels[i], GAUGE_LABEL_LEN);
        }
    }
}

TF_TEST(gauge_presets, generated_labels_fit_the_buffer)
{
    for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
        if (p->cfg->tick_labels) {
            continue;
        }
        gauge_scale_t s;
        gauge_math_from_config(p->cfg, &s);
        int majors = gauge_math_major_ticks(&s);
        for (int i = 0; i < majors; i++) {
            char buf[GAUGE_LABEL_LEN];
            int n = gauge_math_major_label(&s, i, buf, sizeof(buf));
            TF_CHECK_MSG(n > 0 && n < GAUGE_LABEL_LEN,
                         "%s: label %d does not fit (%d chars)", p->id, i, n);
        }
    }
}

TF_TEST(gauge_presets, alarm_band_is_either_absent_or_inside_the_range)
{
    for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
        const gauge_config_t *c = p->cfg;
        bool inside = c->alarm_from >= c->min && c->alarm_from <= c->max;
        bool disabled = c->alarm_from > c->max;
        TF_CHECK_MSG(inside || disabled,
                     "%s: alarm_from %g is below min %g",
                     p->id, (double)c->alarm_from, (double)c->min);
    }
}

TF_TEST(gauge_presets, alarm_band_is_wide_enough_to_see)
{
    for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
        const gauge_config_t *c = p->cfg;
        if (c->alarm_from > c->max) {
            continue;
        }
        gauge_scale_t s;
        gauge_math_from_config(c, &s);
        /* at least one major tick's worth of sweep should be red */
        float band = c->max - c->alarm_from;
        TF_CHECK_MSG(band >= c->major_step * 0.5f,
                     "%s: alarm band spans only %g", p->id, (double)band);
    }
}

TF_TEST(gauge_presets, every_preset_renders_a_sane_dial)
{
    for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
        const gauge_config_t *c = p->cfg;
        const gauge_theme_t *t = c->theme;
        int dial = 240;

        gauge_scale_t s;
        gauge_math_from_config(c, &s);

        int rail = gauge_math_rail_radius(dial, t->bezel_width, t->band_gap, t->band_width);
        int needle = gauge_math_needle_length(rail, t->tick_major_len);
        int label_r = gauge_math_label_radius(rail, t->tick_major_len,
                                              t->label_pad_radial, t->label_letter_space);
        int majors = gauge_math_major_ticks(&s);

        char msg[160];
        snprintf(msg, sizeof(msg), "%s: ", p->id);

        TF_CHECK_MSG(rail > 0 && rail < dial / 2, "%srail %d out of range", msg, rail);
        TF_CHECK_MSG(needle > 0, "%sneedle length %d", msg, needle);
        TF_CHECK_MSG(label_r > t->hub_radius + 12,
                     "%snumeral radius %d collides with the hub (%d)",
                     msg, label_r, t->hub_radius);
        TF_CHECK_MSG(label_r < needle,
                     "%snumerals (%d) sit outside the needle tip (%d)",
                     msg, label_r, needle);

        /* the value read-out must fit across the bottom wedge */
        int value_halfwidth = 40;   /* four digits at the value font */
        int value_y = t->hub_radius + 42;
        TF_CHECK_MSG(value_y + 18 < rail - t->tick_major_len,
                     "%svalue read-out collides with the tick band", msg);
        TF_CHECK_MSG(value_halfwidth < needle, "%svalue read-out too wide", msg);

        TF_GE(majors, 2);
    }
}

TF_TEST(gauge_presets, angle_of_alarm_start_is_inside_the_sweep)
{
    for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
        const gauge_config_t *c = p->cfg;
        if (c->alarm_from > c->max) {
            continue;
        }
        gauge_scale_t s;
        gauge_math_from_config(c, &s);
        float a = gauge_math_value_to_angle(&s, c->alarm_from);
        float lo = gauge_math_value_to_angle(&s, c->min);
        float hi = gauge_math_value_to_angle(&s, c->max);
        TF_CHECK_MSG(a >= lo && a <= hi,
                     "%s: alarm angle %g outside sweep %g..%g",
                     p->id, (double)a, (double)lo, (double)hi);
    }
}
