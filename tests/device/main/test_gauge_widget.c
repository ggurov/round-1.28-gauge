/*
 * test_gauge_widget.c - exercises the real LVGL object tree on the target.
 *
 * Host tests cover the arithmetic; these cover the parts that only exist once
 * LVGL is involved: widget construction, needle rotation, the read-out label,
 * timer-driven slew, and that deleting a gauge actually frees everything.
 */
#include <math.h>
#include <string.h>

#include "gauge.h"
#include "gauge_math.h"
#include "gauge_presets.h"
#include "lvgl_test_env.h"
#include "unity.h"

#define DIAL 240

/* -------------------------------------------------------------------------- */

TEST_CASE("gauge: every preset builds, renders and tears down", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    TEST_ASSERT_NOT_NULL(scr);

    for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
        gauge_t *g = gauge_create(scr, p->cfg);
        TEST_ASSERT_NOT_NULL_MESSAGE(g, p->id);
        TEST_ASSERT_NOT_NULL(gauge_get_obj(g));
        TEST_ASSERT_NOT_NULL(gauge_get_scale(g));
        TEST_ASSERT_NOT_NULL(gauge_get_needle(g));
        TEST_ASSERT_NOT_NULL(gauge_get_hub(g));
        lvgl_test_env_render();
        gauge_delete(g);
    }
    lvgl_test_env_render();
}

TEST_CASE("gauge: root fills the display", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    gauge_t *g = gauge_create(scr, gauge_preset_rpm());
    TEST_ASSERT_NOT_NULL(g);

    lv_obj_t *root = gauge_get_obj(g);
    TEST_ASSERT_EQUAL_INT(DIAL, lv_obj_get_width(root));
    TEST_ASSERT_EQUAL_INT(DIAL, lv_obj_get_height(root));
    gauge_delete(g);
}

TEST_CASE("gauge: rejects a null config", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    TEST_ASSERT_NULL(gauge_create(scr, NULL));

    gauge_config_t bad = *gauge_preset_rpm();
    bad.theme = NULL;
    TEST_ASSERT_NULL(gauge_create(scr, &bad));
}

TEST_CASE("gauge: config defaults are resolved on the live object", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    gauge_config_t cfg = *gauge_preset_rpm();
    cfg.angle_range = 0;       /* -> 270 */
    cfg.rotation = 0;          /* -> 135 */
    cfg.minor_per_major = 0;   /* -> 4   */

    gauge_t *g = gauge_create(scr, &cfg);
    TEST_ASSERT_NOT_NULL(g);

    const gauge_config_t *live = gauge_get_config(g);
    TEST_ASSERT_NOT_NULL(live);
    TEST_ASSERT_EQUAL_INT(270, live->angle_range);
    TEST_ASSERT_EQUAL_INT(135, live->rotation);
    TEST_ASSERT_EQUAL_INT(4, live->minor_per_major);
    gauge_delete(g);
}

/* -------------------------------------------------------------------------- */

TEST_CASE("gauge: needle angle tracks the value", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    gauge_t *g = gauge_create(scr, gauge_preset_rpm());
    TEST_ASSERT_NOT_NULL(g);

    lv_obj_t *needle = gauge_get_needle(g);
    const gauge_config_t *c = gauge_get_config(g);

    gauge_set_value_immediate(g, c->min);
    TEST_ASSERT_EQUAL_INT32((int32_t)lroundf(gauge_get_value_angle(g, c->min) * 10.0f),
                            lv_image_get_rotation(needle));

    gauge_set_value_immediate(g, c->max);
    TEST_ASSERT_EQUAL_INT32((int32_t)lroundf(gauge_get_value_angle(g, c->max) * 10.0f),
                            lv_image_get_rotation(needle));

    gauge_set_value_immediate(g, 4000.0f);
    /* 4000 rpm sits at 12 o'clock */
    TEST_ASSERT_EQUAL_INT32(3600, lv_image_get_rotation(needle));
    gauge_delete(g);
}

TEST_CASE("gauge: needle uses the generated sprite and a centred pivot", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    gauge_t *g = gauge_create(scr, gauge_preset_rpm());
    TEST_ASSERT_NOT_NULL(g);

    lv_obj_t *needle = gauge_get_needle(g);
    lv_image_dsc_t *dsc = (lv_image_dsc_t *)lv_image_get_src(needle);
    TEST_ASSERT_NOT_NULL(dsc);
    TEST_ASSERT_EQUAL_INT(2400, lv_image_get_rotation(needle));  /* 0 rpm -> 240 deg */

    /* the pivot must land on the dial centre: pos + pivot == 120,120 */
    lv_point_t pivot;
    lv_image_get_pivot(needle, &pivot);
    TEST_ASSERT_EQUAL_INT32(lv_image_get_src_width(needle) / 2, pivot.x);
    TEST_ASSERT_EQUAL_INT32(lv_image_get_src_height(needle) / 2, pivot.y);
    TEST_ASSERT_EQUAL_INT32(DIAL / 2, lv_obj_get_x(needle) + pivot.x);
    TEST_ASSERT_EQUAL_INT32(DIAL / 2, lv_obj_get_y(needle) + pivot.y);

    gauge_delete(g);
}

/* -------------------------------------------------------------------------- */

TEST_CASE("gauge: values are clamped to the configured range", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    gauge_t *g = gauge_create(scr, gauge_preset_rpm());
    const gauge_config_t *c = gauge_get_config(g);

    gauge_set_value(g, -5000.0f);
    TEST_ASSERT_EQUAL_FLOAT(c->min, gauge_get_target_value(g));

    gauge_set_value(g, 99999.0f);
    TEST_ASSERT_EQUAL_FLOAT(c->max, gauge_get_target_value(g));
    gauge_delete(g);
}

TEST_CASE("gauge: immediate set updates displayed value and read-out", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    gauge_t *g = gauge_create(scr, gauge_preset_rpm());
    TEST_ASSERT_NOT_NULL(g);

    gauge_set_value_immediate(g, 4250.0f);
    TEST_ASSERT_EQUAL_FLOAT(4250.0f, gauge_get_displayed_value(g));
    TEST_ASSERT_EQUAL_STRING("4250", gauge_get_value_text(g));
    gauge_delete(g);
}

TEST_CASE("gauge: read-out honours each preset's decimals", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();

    gauge_t *rpm = gauge_create(scr, gauge_preset_rpm());
    gauge_set_value_immediate(rpm, 4250.0f);
    TEST_ASSERT_EQUAL_STRING("4250", gauge_get_value_text(rpm));
    gauge_delete(rpm);

    gauge_t *temp = gauge_create(scr, gauge_preset_temp());
    gauge_set_value_immediate(temp, 92.4f);
    TEST_ASSERT_EQUAL_STRING("92", gauge_get_value_text(temp));
    gauge_delete(temp);

    gauge_t *boost = gauge_create(scr, gauge_preset_boost());
    gauge_set_value_immediate(boost, 0.85f);
    TEST_ASSERT_EQUAL_STRING("0.8", gauge_get_value_text(boost));  /* rounds */
    gauge_delete(boost);

    gauge_t *volts = gauge_create(scr, gauge_preset_volts());
    gauge_set_value_immediate(volts, 13.84f);
    TEST_ASSERT_EQUAL_STRING("13.8", gauge_get_value_text(volts));
    gauge_delete(volts);
}

/* -------------------------------------------------------------------------- */

TEST_CASE("gauge: the needle slews toward its target instead of jumping", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    gauge_t *g = gauge_create(scr, gauge_preset_rpm());
    const gauge_config_t *c = gauge_get_config(g);

    gauge_set_value_immediate(g, c->min);
    gauge_set_value(g, c->max);
    TEST_ASSERT_EQUAL_FLOAT(c->min, gauge_get_displayed_value(g));

    /* a handful of 16 ms ticks should move it, but not all the way */
    for (int i = 0; i < 4; i++) {
        lvgl_test_env_render();
    }
    float partway = gauge_get_displayed_value(g);
    TEST_ASSERT_GREATER_THAN_FLOAT((double)c->min, (double)partway);
    TEST_ASSERT_LESS_THAN_FLOAT((double)c->max, (double)partway);

    /* and it must settle */
    for (int i = 0; i < 400; i++) {
        lvgl_test_env_render();
    }
    TEST_ASSERT_EQUAL_FLOAT(c->max, gauge_get_displayed_value(g));
    gauge_delete(g);
}

TEST_CASE("gauge: the needle never leaves the dial while slewing", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    gauge_t *g = gauge_create(scr, gauge_preset_rpm());
    const gauge_config_t *c = gauge_get_config(g);
    lv_obj_t *needle = gauge_get_needle(g);

    float lo = gauge_get_value_angle(g, c->min);
    float hi = gauge_get_value_angle(g, c->max);

    gauge_set_value_immediate(g, c->min);
    gauge_set_value(g, c->max);
    for (int i = 0; i < 200; i++) {
        lvgl_test_env_render();
        float angle = (float)lv_image_get_rotation(needle) / 10.0f;
        TEST_ASSERT_GREATER_OR_EQUAL_FLOAT((double)lo, (double)angle);
        TEST_ASSERT_LESS_OR_EQUAL_FLOAT((double)hi, (double)angle);
    }
    gauge_delete(g);
}

/* -------------------------------------------------------------------------- */

TEST_CASE("gauge: create and delete does not leak LVGL memory", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();

    /* warm up so one-off allocations are not counted */
    gauge_t *warm = gauge_create(scr, gauge_preset_rpm());
    TEST_ASSERT_NOT_NULL(warm);
    lvgl_test_env_render();
    gauge_delete(warm);

    long before = lvgl_test_env_mem_used();
    TEST_ASSERT_TRUE(before >= 0);

    for (int i = 0; i < 8; i++) {
        gauge_t *g = gauge_create(scr, gauge_preset_rpm());
        TEST_ASSERT_NOT_NULL(g);
        lvgl_test_env_render();
        gauge_delete(g);
    }
    lvgl_test_env_render();

    long after = lvgl_test_env_mem_used();
    TEST_ASSERT_EQUAL_INT((int)before, (int)after);
}

TEST_CASE("gauge: switching preset on the same screen works repeatedly", "[gauge]")
{
    lv_obj_t *scr = lvgl_test_env_fresh_screen();
    gauge_t *g = NULL;

    for (int round = 0; round < 3; round++) {
        for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
            if (g) {
                gauge_delete(g);
            }
            g = gauge_create(scr, p->cfg);
            TEST_ASSERT_NOT_NULL_MESSAGE(g, p->id);
            gauge_set_value_immediate(g, p->cfg->min + (p->cfg->max - p->cfg->min) * 0.5f);
            lvgl_test_env_render();
        }
    }
    gauge_delete(g);
}
