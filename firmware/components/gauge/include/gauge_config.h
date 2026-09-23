/*
 * gauge_config.h - what makes one instrument different from another.
 *
 * Kept free of LVGL so configs and presets can be unit-tested on the host.
 * gauge.c turns this into widgets; gauge_math.c does the arithmetic.
 */
#pragma once

#include <stdint.h>

#include "gauge_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Upper bound on labelled major ticks; label storage is sized from this. */
#define GAUGE_MAX_TICKS 32
/* Bytes per generated numeral, e.g. "-12.5" plus NUL. */
#define GAUGE_LABEL_LEN 12

typedef struct {
    /* --- text ------------------------------------------------------------ */
    const char *caption;      /* centre caption, e.g. "RPM"                  */
    const char *unit;         /* unit line, e.g. "x1000 r/min"               */
    const char *wordmark;     /* brand text above the hub; may be NULL       */
    const char *tagline;      /* small print under the wordmark; may be NULL */

    /* --- scale ----------------------------------------------------------- */
    float min, max;           /* engineering range                           */
    float major_step;         /* value between two major ticks               */
    int   minor_per_major;    /* minor ticks between two major ticks         */

    /*
     * Optional explicit numerals, NULL-terminated.  Must outlive the gauge
     * (string literals are ideal).  Exactly gauge_math_major_ticks() entries.
     * NULL means generate them from min/major_step/decimals.
     */
    const char *const *tick_labels;

    /* --- warning band ---------------------------------------------------- */
    /* Value at which the redline colour starts; above `max` for no band. */
    float alarm_from;

    /* --- read-out -------------------------------------------------------- */
    int   decimals;           /* digits after the decimal point, 0..2        */

    /*
     * Seconds for a full-scale needle move.  The slew is rate-limited to
     * range/slew_time and rounded off by a much faster exponential, so the
     * needle settles within about 1.5x this value.  Tachometer-ish: 0.30.
     * Damped temperature gauge: 2.0.  0 -> 0.35.
     */
    float slew_time;

    /* --- geometry -------------------------------------------------------- */
    int   angle_range;        /* sweep in degrees; 0 -> 270                  */
    int   rotation;           /* first tick clockwise from 3 o'clock; 0 -> 135 */
    float needle_length;      /* px from centre; 0 -> auto                   */
    float label_radius;       /* px; 0 -> auto                               */

    /* --- look ------------------------------------------------------------ */
    const gauge_theme_t *theme;
} gauge_config_t;

#ifdef __cplusplus
}
#endif
