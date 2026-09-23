/*
 * gauge.h - a config-driven, smoothly animated automotive gauge widget.
 *
 * The gauge owns no data source: callers push engineering values in with
 * gauge_set_value() and the widget slews the needle toward the new target so
 * motion looks like a real moving-coil / stepper instrument instead of a
 * jump-cut.
 *
 * Everything that distinguishes one gauge from another (range, tick spacing,
 * units, warning band, palette, typography) lives in gauge_config_t, so
 * turning the tachometer into a temperature or boost gauge is a matter of
 * swapping the config - see gauge_presets.h.
 */
#pragma once

#include "lvgl.h"
#include "gauge_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAUGE_MAX_TICKS  24   /* major ticks we are willing to label */
#define GAUGE_LABEL_LEN  8    /* bytes per auto-generated numeral    */

typedef struct gauge gauge_t;

typedef struct {
    /* --- text ------------------------------------------------------------ */
    const char *caption;      /* centre caption, e.g. "RPM"                  */
    const char *unit;         /* unit line, e.g. "x1000 r/min"               */
    const char *wordmark;     /* struck-through brand text; may be NULL      */
    const char *tagline;      /* small print under the wordmark; may be NULL */

    /* --- scale ----------------------------------------------------------- */
    float min, max;           /* engineering range                           */
    float major_step;         /* value between two major ticks               */
    int   minor_per_major;    /* minor ticks between two major ticks         */

    /* Optional explicit numerals.  NULL-terminated, must stay alive for as
     * long as the gauge does (string literals are ideal).  When NULL the
     * numerals are generated from min/major_step. */
    const char *const *tick_labels;

    /* --- warning band ---------------------------------------------------- */
    /* Value at which the redline / warning colour starts.  Set above `max`
     * (or to NAN) for no band. */
    float alarm_from;

    /* --- read-out -------------------------------------------------------- */
    int decimals;             /* digits after the decimal point, 0..2        */

    /* --- needle dynamics ------------------------------------------------- */
    /* Seconds for a full-scale needle move.  Small values feel like a
     * stepper tachometer (~0.35), large values like a damped temperature
     * gauge (~2.5).  0 -> 0.35. */
    float slew_time;

    /* --- geometry -------------------------------------------------------- */
    int   angle_range;        /* sweep in degrees; 0 -> 270                  */
    int   rotation;           /* first tick, clockwise from 3 o'clock; 0 -> 135 */
    float needle_length;      /* needle length in px from centre; 0 -> auto  */
    float label_radius;       /* numeral centre radius in px; 0 -> auto      */

    /* --- look ------------------------------------------------------------ */
    const gauge_theme_t *theme;
} gauge_config_t;

/* Creates the gauge as a child of `parent` and fills it (LV_PCT(100)).
 * Returns NULL on allocation failure. */
gauge_t *gauge_create(lv_obj_t *parent, const gauge_config_t *cfg);

/* Destroys the gauge and every LVGL object it owns. */
void gauge_delete(gauge_t *g);

/* Pushes a new reading.  Returns immediately; the needle slews toward it. */
void gauge_set_value(gauge_t *g, float value);

/* Jumps the needle straight to `value`, skipping the smoothing filter. */
void gauge_set_value_immediate(gauge_t *g, float value);

/* Current *displayed* value (i.e. where the needle actually is). */
float gauge_get_displayed_value(const gauge_t *g);

/* Last value requested with gauge_set_value(). */
float gauge_get_target_value(const gauge_t *g);

/* Renders `value` to a needle angle in degrees, clockwise from 12 o'clock. */
float gauge_value_to_angle(const gauge_t *g, float value);

/* Root LVGL object, e.g. for setting a custom background. */
lv_obj_t *gauge_get_obj(gauge_t *g);

#ifdef __cplusplus
}
#endif
