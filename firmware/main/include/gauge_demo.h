/*
 * gauge_demo.h - binds a gauge to a data source and drives it.
 *
 * Today the "data source" is a built-in engine simulator; the seam is
 * gauge_demo_set_value(), which is where CAN / OBD-II / ADC readings will
 * eventually arrive.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "gauge_presets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Creates the gauge on the active LVGL screen and starts the 20 ms driver
 * timer.  Requires bsp_display_init() to have succeeded. */
esp_err_t gauge_demo_start(void);

bool gauge_demo_ready(void);

/* Swap to a different instrument; rebuilds the widget. */
void gauge_demo_select(const gauge_preset_t *preset);

const gauge_preset_t *gauge_demo_current(void);

/* true  -> the internal simulator drives the needle
 * false -> the needle follows gauge_demo_set_value() */
void gauge_demo_set_running(bool running);
bool gauge_demo_is_running(void);

/* Push an engineering value in.  Implicitly stops the simulator. */
void gauge_demo_set_value(float value);

/* Replay the full-scale self-test sweep. */
void gauge_demo_run_selftest(void);

#ifdef __cplusplus
}
#endif
