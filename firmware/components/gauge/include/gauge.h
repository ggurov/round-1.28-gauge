/*
 * gauge.h - the LVGL widget.
 *
 * A gauge owns no data source: callers push engineering values in with
 * gauge_set_value() and the widget slews the needle toward the new target so
 * motion looks like a real moving-coil / stepper instrument instead of a
 * jump-cut.
 *
 * The arithmetic lives in gauge_math.c and the look in gauge_theme.c, both of
 * which are LVGL-free and covered by host unit tests.  This file only turns
 * that data into LVGL objects.
 */
#pragma once

#include "lvgl.h"

#include "gauge_config.h"
#include "gauge_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gauge gauge_t;

/* Creates the gauge as a child of `parent`, filling it.  NULL on failure. */
gauge_t *gauge_create(lv_obj_t *parent, const gauge_config_t *cfg);

/* Destroys the gauge and every LVGL object it owns. */
void gauge_delete(gauge_t *g);

/* Pushes a new reading.  Returns immediately; the needle slews toward it. */
void gauge_set_value(gauge_t *g, float value);

/* Jumps the needle straight to `value`, skipping the smoothing filter. */
void gauge_set_value_immediate(gauge_t *g, float value);

/* Where the needle actually is. */
float gauge_get_displayed_value(const gauge_t *g);

/* Last value requested with gauge_set_value(). */
float gauge_get_target_value(const gauge_t *g);

/* The live config (resolved, including any defaults). */
const gauge_config_t *gauge_get_config(const gauge_t *g);

/* Needle angle in degrees clockwise from 12 o'clock for an arbitrary value. */
float gauge_get_value_angle(const gauge_t *g, float value);

/* Root LVGL object, e.g. for setting a custom background. */
lv_obj_t *gauge_get_obj(gauge_t *g);

/* Sub-objects, for diagnostics and the on-target widget tests. */
lv_obj_t *gauge_get_needle(gauge_t *g);
lv_obj_t *gauge_get_scale(gauge_t *g);
lv_obj_t *gauge_get_hub(gauge_t *g);

/* Current contents of the numeric read-out. */
const char *gauge_get_value_text(gauge_t *g);

/* Number of LVGL widgets the gauge created; handy for tests and diagnostics. */
int gauge_widget_count(const gauge_t *g);

#ifdef __cplusplus
}
#endif
