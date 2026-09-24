/*
 * app_gauge.c - drives the gauge and pushes frames to the panel.
 *
 * A 20 ms task slews the needle and redraws the dial; a full 240x240 RGB565
 * frame is 115 KB, which is about 23 ms of SPI at 40 MHz, so the frame rate
 * settles around 25-30 fps.  The render itself is a couple of milliseconds.
 *
 * Real data will arrive through app_gauge_set_value(); today the source is a
 * small engine simulator with a self-test sweep on boot.
 */
#include "app_gauge.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gauge_render.h"
#include "gfx.h"

static const char *TAG = "gauge";

#define TICK_MS 20

/* Self-test sweep timings (ms from start). */
#define SELFTEST_TOP_MS 1150
#define SELFTEST_END_MS 1750

/* Repeating driving script, as fractions of the gauge's range so the same
 * script looks sensible on a tach, a boost gauge or a temperature gauge. */
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

static gauge_render_t       *s_gauge;
static const gauge_preset_t *s_preset;
static TaskHandle_t          s_task;
static bool                  s_running;      /* task alive */
static bool                  s_demo = true;
static bool                  s_visible = true;

static uint32_t s_elapsed_ms;
static size_t   s_step;
static uint32_t s_step_ms;
static float    s_jitter;
static bool     s_selftest;
static float    s_manual;

/* -------------------------------------------------------------------------- */

static void restart_selftest(void)
{
    s_elapsed_ms = 0;
    s_selftest = true;
    s_step = 0;
    s_step_ms = 0;
}

static void gauge_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(TICK_MS);

    for (;;) {
        vTaskDelay(period);
        if (!s_gauge || !s_visible) {
            continue;
        }

        const gauge_config_t *cfg = gauge_render_config(s_gauge);

        if (!s_demo) {
            gauge_render_set_value(s_gauge, s_manual);
        } else if (s_selftest) {
            s_elapsed_ms += TICK_MS;
            if (s_elapsed_ms < 500) {
                gauge_render_set_value(s_gauge, cfg->min);
            } else if (s_elapsed_ms < SELFTEST_TOP_MS) {
                gauge_render_set_value(s_gauge, cfg->max);
            } else if (s_elapsed_ms < SELFTEST_END_MS) {
                gauge_render_set_value(s_gauge, cfg->min);
            } else {
                s_selftest = false;
                s_step = 0;
                s_step_ms = 0;
            }
        } else {
            s_step_ms += TICK_MS;
            if (s_step_ms >= k_script[s_step].ms) {
                s_step_ms = 0;
                s_step = (s_step + 1) % SCRIPT_LEN;
            }
            const float range = cfg->max - cfg->min;
            float value = cfg->min + k_script[s_step].frac * range;

            /* a little smoothed noise so the needle never looks generated */
            const float r = ((float)(rand() % 2001) / 1000.0f) - 1.0f;
            s_jitter += (r - s_jitter) * 0.25f;
            value += s_jitter * 0.012f * range;

            gauge_render_set_value(s_gauge, value);
        }

        gauge_render_tick(s_gauge, (float)TICK_MS / 1000.0f);
        gfx_flush();
    }
}

static void ensure_task(void)
{
    if (!s_task) {
        xTaskCreatePinnedToCore(gauge_task, "gauge", 4096, NULL, 5, &s_task, 1);
    }
}

/* -------------------------------------------------------------------------- */

void app_gauge_start(void)
{
    if (!s_preset) {
        s_preset = gauge_preset_find("rpm");
    }
    if (!s_gauge) {
        s_gauge = gauge_render_create(s_preset->cfg);
        if (!s_gauge) {
            ESP_LOGE(TAG, "gauge_render_create failed for '%s'", s_preset->id);
            return;
        }
        ESP_LOGI(TAG, "gauge: %s", s_preset->name);
    }
    s_visible = true;
    s_running = true;
    restart_selftest();
    ensure_task();
}

void app_gauge_stop(void)
{
    s_visible = false;
}

void app_gauge_select(const gauge_preset_t *preset)
{
    if (!preset || !preset->cfg) {
        return;
    }
    s_preset = preset;
    if (s_gauge) {
        gauge_render_destroy(s_gauge);
        s_gauge = NULL;
    }
    s_gauge = gauge_render_create(preset->cfg);
    if (!s_gauge) {
        ESP_LOGE(TAG, "gauge_render_create failed for '%s'", preset->id);
        return;
    }
    s_visible = true;
    ESP_LOGI(TAG, "gauge: %s", preset->name);
    restart_selftest();
    ensure_task();
}

const gauge_preset_t *app_gauge_current(void)
{
    return s_preset;
}

void app_gauge_set_demo(bool on)
{
    s_demo = on;
    if (on) {
        restart_selftest();
    }
}

bool app_gauge_is_demo(void)
{
    return s_demo;
}

void app_gauge_set_value(float value)
{
    s_manual = value;
    s_demo = false;
}

void app_gauge_sweep(void)
{
    s_demo = true;
    restart_selftest();
}
