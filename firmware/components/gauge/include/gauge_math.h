/*
 * gauge_math.h - the pure arithmetic behind the gauge.
 *
 * Deliberately free of LVGL, ESP-IDF and FreeRTOS so it can be unit-tested on
 * the host with a plain C compiler.  gauge.c is then only responsible for
 * pushing these numbers into LVGL objects.
 */
#pragma once

#include <stddef.h>

#include "gauge_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * lv_scale places a numeral's centre this far inside the end of its major
 * tick, on top of whatever `pad_radial` is set to.  Mirrors LVGL's private
 * LV_SCALE_DEFAULT_LABEL_GAP; asserted against the real header in the tests.
 */
#define GAUGE_LABEL_GAP 15

/* Everything about the scale that the maths needs. */
typedef struct {
    float min;
    float max;
    float major_step;
    int   minor_per_major;   /* minors between two majors; >= 1 */
    int   angle_range;       /* sweep in degrees                  */
    int   rotation;          /* first tick, clockwise from 3 o'clock */
    int   decimals;          /* digits after the point in read-outs  */
} gauge_scale_t;

/* --- angles -------------------------------------------------------------- */

/* Needle angle in degrees, clockwise from 12 o'clock, clamped to the sweep. */
float gauge_math_value_to_angle(const gauge_scale_t *s, float value);

/* Fraction of the sweep covered by `value`, clamped to 0..1. */
float gauge_math_fraction(const gauge_scale_t *s, float value);

/* --- ticks --------------------------------------------------------------- */

/* Number of labelled major ticks, e.g. 9 for 0..8000 in steps of 1000. */
int gauge_math_major_ticks(const gauge_scale_t *s);

/* Total ticks handed to lv_scale_set_total_tick_count(). */
int gauge_math_total_ticks(const gauge_scale_t *s);

/* Value sitting at major tick `index` (0-based). */
float gauge_math_major_value(const gauge_scale_t *s, int index);

/* --- text ---------------------------------------------------------------- */

/* Formats `value` honouring decimals.  Returns the number of chars written. */
int gauge_math_format(const gauge_scale_t *s, float value, char *out, size_t n);

/* Formats the label for major tick `index`.  Returns chars written. */
int gauge_math_major_label(const gauge_scale_t *s, int index, char *out, size_t n);

/* --- geometry ------------------------------------------------------------ */

/*
 * Radius of the lv_scale rail (== the radius ticks grow inward from):
 *   diameter/2 - (bezel + gap + band/2)
 * Integer division on band_width mirrors the C implementation exactly.
 */
int gauge_math_rail_radius(int dial_diameter, int bezel_width, int band_gap, int band_width);

/* Diameter to hand to lv_obj_set_size() for the scale widget. */
int gauge_math_scale_diameter(int rail_radius);

/* Centre radius lv_scale will place numerals at for a given pad_radial. */
int gauge_math_label_radius(int rail_radius, int major_len, int pad_radial, int letter_space);

/* Inverse: the pad_radial that puts numerals at `wanted_radius`. */
int gauge_math_pad_radial_for(int rail_radius, int major_len, int wanted_radius, int letter_space);

/* Default needle length: just inside the major ticks. */
int gauge_math_needle_length(int rail_radius, int major_len);

/*
 * Radius the ticks start at: the inner edge of the rail.  The ticks do not
 * overlap the rail - on a GReddy dial the green band is a clean ring with the
 * ticks hanging inside it.
 */
int gauge_math_tick_base_radius(int rail_radius, int band_width);

/*
 * Outer edge of the warning sector.  It is a separate arc set inboard of the
 * ticks rather than a recolour of them, which is how the old GReddy dials did
 * it: the red zone sits between the scale and the numerals.
 */
int gauge_math_alarm_outer_radius(int tick_base_radius, int major_len, int alarm_gap);

/*
 * Builds the arithmetic view of a config, applying the documented defaults
 * (angle_range 0 -> 270, rotation 0 -> 135, minor_per_major 0 -> 4).
 */
void gauge_math_from_config(const gauge_config_t *cfg, gauge_scale_t *out);

/* --- needle dynamics ----------------------------------------------------- */

/* Exponential-approach coefficient for one tick of `dt` seconds. */
float gauge_math_slew_alpha(float dt, float slew_time);

/* Largest value change allowed in one tick (units per tick). */
float gauge_math_slew_max_step(float range, float dt, float slew_time);

/*
 * One smoothing step.  Approaches `target` exponentially, rate-limited, and
 * snaps when within `epsilon` so the needle actually settles.
 */
float gauge_math_slew_step(float displayed, float target, float alpha,
                           float max_step, float epsilon);

#ifdef __cplusplus
}
#endif
