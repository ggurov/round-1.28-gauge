/*
 * gauge_demo.c - drives a gauge from a data source.
 *
 * The "data source" is currently a small engine simulator so the demo has
 * something believable to show: a self-test sweep on boot, then a repeating
 * driving script with throttle blips, gear changes and a bit of needle
 * jitter.  Real data will arrive through gauge_demo_set_value().
 */
#include "gauge_demo.h"

#include <math.h>
#include <stdlib.h>

#include "bsp.h"
#include "esp_log.h"
#include "gauge.h"
#include "gauge_presets.h"

static const char *TAG = "demo";

#define DEMO_PERIOD_MS 20

/* Self-test sweep timings (ms from start). */
#define SELFTEST_MID_MS   500
#define SELFTEST_TOP_MS   1150
#define SELFTEST_END_MS   1750

/* Repeating driving script.  Values are fractions of the gauge's range so the
 * same script looks sensible on a tach, a boost gauge or a temperature gauge. */
typedef struct {
    uint16_t ms;
    float    frac;
} demo_step_t;

static const demo_step_t k_script[] = {
    { 1400, 0.11f }, {  600, 0.40f }, {  900, 0.375f }, {  500, 0.22f },
    {  700, 0.52f }, { 1000, 0.65f }, {  600, 0.575f }, {  900, 0.82f },
    {  700, 1.00f }, {  500, 0.65f }, {  800, 0.475f }, {  600, 0.30f },
    { 1200, 0.105f }, { 1000, 0.70f }, {  700, 0.375f }, { 1500, 0.115f },
};
#define SCRIPT_LEN (sizeof(k_script) / sizeof(k_script[0]))

static gauge_t              *s_gauge;
static const gauge_preset_t *s_preset;
static lv_timer_t           *s_timer;

static bool     s_running = true;   /* simulator active */
static float    s_manual;           /* value when not simulating */

static uint32_t s_elapsed_ms;
static size_t   s_step;
static uint32_t s_step_ms;
static float    s_jitter;
static bool     s_selftest;

/* -------------------------------------------------------------------------- */

static float range_of(const gauge_config_t *c)
{
    float r = c->max - c->min;
    return r > 0.0f ? r : 1.0f;
}

static void demo_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_gauge) {
        return;
    }

    s_elapsed_ms += DEMO_PERIOD_MS;

    if (!s_running) {
        gauge_set_value(s_gauge, s_manual);
        return;
    }

    /* --- boot self-test: pin the needle low, full scale, then back --- */
    if (s_selftest) {
        const gauge_preset_t *p = s_preset;
        const gauge_config_t *cfg = p->cfg;
        if (s_elapsed_ms < SELFTEST_MID_MS) {
            gauge_set_value(s_gauge, cfg->min);
        } else if (s_elapsed_ms < SELFTEST_TOP_MS) {
            gauge_set_value(s_gauge, cfg->max);
        } else if (s_elapsed_ms < SELFTEST_END_MS) {
            gauge_set_value(s_gauge, cfg->min);
        } else {
            s_selftest = false;
            s_step = 0;
            s_step_ms = 0;
        }
        return;
    }

    /* --- driving script --- */
    s_step_ms += DEMO_PERIOD_MS;
    if (s_step_ms >= k_script[s_step].ms) {
        s_step_ms = 0;
        s_step = (s_step + 1) % SCRIPT_LEN;
    }

    const gauge_config_t *cfg = s_preset->cfg;
    float range = range_of(cfg);
    float value = cfg->min + k_script[s_step].frac * range;

    /* Smooth pseudo-random jitter so the needle never looks computer-generated. */
    float r = ((float)(rand() % 2001) / 1000.0f) - 1.0f;
    s_jitter += (r - s_jitter) * 0.25f;
    value += s_jitter * 0.012f * range;

    gauge_set_value(s_gauge, value);
}

/* -------------------------------------------------------------------------- */

static esp_err_t rebuild_gauge(void)
{
    if (!bsp_display_get()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!bsp_lvgl_lock(-1)) {
        return ESP_ERR_TIMEOUT;
    }

    if (s_gauge) {
        gauge_delete(s_gauge);
        s_gauge = NULL;
    }

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_gauge = gauge_create(scr, s_preset->cfg);

    bsp_lvgl_unlock();

    if (!s_gauge) {
        ESP_LOGE(TAG, "gauge_create failed for preset '%s'", s_preset->id);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "gauge: %s", s_preset->name);
    return ESP_OK;
}

static void restart_selftest(void)
{
    s_elapsed_ms = 0;
    s_selftest = true;
    s_step = 0;
    s_step_ms = 0;
}

esp_err_t gauge_demo_start(void)
{
    if (!s_preset) {
        s_preset = gauge_preset_find("rpm");
    }

    esp_err_t err = rebuild_gauge();
    if (err != ESP_OK) {
        return err;
    }

    restart_selftest();
    gauge_set_value_immediate(s_gauge, s_preset->cfg->min);

    if (!bsp_lvgl_lock(-1)) {
        return ESP_ERR_TIMEOUT;
    }
    s_timer = lv_timer_create(demo_timer_cb, DEMO_PERIOD_MS, NULL);
    bsp_lvgl_unlock();

    return s_timer ? ESP_OK : ESP_ERR_NO_MEM;
}

bool gauge_demo_ready(void)
{
    return s_gauge != NULL;
}

void gauge_demo_select(const gauge_preset_t *preset)
{
    if (!preset || !preset->cfg) {
        return;
    }
    s_preset = preset;
    if (rebuild_gauge() != ESP_OK) {
        return;
    }
    restart_selftest();
    gauge_set_value_immediate(s_gauge, preset->cfg->min);
}

const gauge_preset_t *gauge_demo_current(void)
{
    return s_preset;
}

void gauge_demo_set_running(bool running)
{
    s_running = running;
    if (running) {
        restart_selftest();
    }
}

bool gauge_demo_is_running(void)
{
    return s_running;
}

void gauge_demo_set_value(float value)
{
    s_manual = value;
    s_running = false;
}

void gauge_demo_run_selftest(void)
{
    s_running = true;
    restart_selftest();
}
