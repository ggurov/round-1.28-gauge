/*
 * gauge_math.c - pure arithmetic for the gauge renderer.
 *
 * No LVGL, no ESP-IDF.  Compiled both into the firmware and into the host
 * unit tests (see tests/host).
 */
#include "gauge_math.h"

#include <math.h>
#include <stdio.h>

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* -------------------------------------------------------------------------- */

float gauge_math_fraction(const gauge_scale_t *s, float value)
{
    float span = s->max - s->min;
    if (span <= 0.0f) {
        return 0.0f;
    }
    return clampf((value - s->min) / span, 0.0f, 1.0f);
}

float gauge_math_value_to_angle(const gauge_scale_t *s, float value)
{
    /*
     * lv_scale angles run clockwise from 3 o'clock; the needle sprite points
     * at 12 o'clock when its rotation is zero, hence the +90.
     */
    return (float)s->rotation + gauge_math_fraction(s, value) * (float)s->angle_range + 90.0f;
}

/* -------------------------------------------------------------------------- */

int gauge_math_major_ticks(const gauge_scale_t *s)
{
    float span = s->max - s->min;
    int steps = 0;
    if (s->major_step > 0.0f && span > 0.0f) {
        steps = (int)lroundf(span / s->major_step);
    }
    if (steps < 1) {
        steps = 1;
    }
    /* label storage in gauge.c is sized for this */
    return clampi(steps + 1, 2, 32);
}

int gauge_math_total_ticks(const gauge_scale_t *s)
{
    int minor = s->minor_per_major > 0 ? s->minor_per_major : 1;
    return (gauge_math_major_ticks(s) - 1) * minor + 1;
}

float gauge_math_major_value(const gauge_scale_t *s, int index)
{
    return s->min + s->major_step * (float)index;
}

/* -------------------------------------------------------------------------- */

int gauge_math_format(const gauge_scale_t *s, float value, char *out, size_t n)
{
    int written;
    if (s->decimals <= 0) {
        written = snprintf(out, n, "%d", (int)lroundf(value));
    } else if (s->decimals == 1) {
        written = snprintf(out, n, "%.1f", (double)value);
    } else {
        written = snprintf(out, n, "%.2f", (double)value);
    }
    return written;
}

int gauge_math_major_label(const gauge_scale_t *s, int index, char *out, size_t n)
{
    return gauge_math_format(s, gauge_math_major_value(s, index), out, n);
}

/* -------------------------------------------------------------------------- */

int gauge_math_rail_radius(int dial_diameter, int bezel_width, int band_gap, int band_width)
{
    return dial_diameter / 2 - (bezel_width + band_gap + band_width / 2);
}

int gauge_math_scale_diameter(int rail_radius)
{
    return rail_radius * 2;
}

int gauge_math_label_radius(int rail_radius, int major_len, int pad_radial, int letter_space)
{
    return rail_radius - major_len - (pad_radial + GAUGE_LABEL_GAP + letter_space);
}

int gauge_math_pad_radial_for(int rail_radius, int major_len, int wanted_radius, int letter_space)
{
    int pad = rail_radius - major_len - GAUGE_LABEL_GAP - letter_space - wanted_radius;
    return pad < 0 ? 0 : pad;
}

int gauge_math_needle_length(int rail_radius, int major_len)
{
    return rail_radius - major_len - 2;
}

int gauge_math_tick_base_radius(int rail_radius, int band_width)
{
    return rail_radius - band_width;
}

int gauge_math_alarm_outer_radius(int tick_base_radius, int major_len, int alarm_gap)
{
    return tick_base_radius - major_len - alarm_gap;
}

void gauge_math_from_config(const gauge_config_t *cfg, gauge_scale_t *out)
{
    out->min = cfg->min;
    out->max = cfg->max;
    out->major_step = cfg->major_step;
    out->minor_per_major = cfg->minor_per_major > 0 ? cfg->minor_per_major : 4;
    out->angle_range = cfg->angle_range > 0 ? cfg->angle_range : 270;
    out->rotation = cfg->rotation != 0 ? cfg->rotation : 135;
    out->decimals = cfg->decimals;
}

/* -------------------------------------------------------------------------- */

/*
 * The exponential is deliberately much faster than the rate limit: the ramp
 * does the travelling and the exponential only rounds off the arrival, which is
 * what a real moving-coil movement looks like.  A time constant of slew_time/12
 * keeps the total settle comfortably inside 1.6x slew_time.
 */
#define SLEW_TAU_DIVISOR 12.0f

float gauge_math_slew_alpha(float dt, float slew_time)
{
    if (slew_time <= 0.0f) {
        return 1.0f;
    }
    float tau = slew_time / SLEW_TAU_DIVISOR;
    return 1.0f - expf(-dt / tau);
}

float gauge_math_slew_max_step(float range, float dt, float slew_time)
{
    if (slew_time <= 0.0f || range <= 0.0f) {
        return 0.0f;
    }
    return range * dt / slew_time;
}

float gauge_math_slew_step(float displayed, float target, float alpha,
                           float max_step, float epsilon)
{
    float err = target - displayed;
    if (fabsf(err) <= epsilon) {
        return target;
    }

    float step = err * alpha;
    if (max_step > 0.0f) {
        if (step > max_step) {
            step = max_step;
        } else if (step < -max_step) {
            step = -max_step;
        }
    }

    float next = displayed + step;
    if (fabsf(target - next) <= epsilon) {
        return target;
    }
    return next;
}
