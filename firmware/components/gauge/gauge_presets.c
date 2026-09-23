/*
 * gauge_presets.c - the gauge catalogue.
 */
#include "gauge_presets.h"
#include "gauge_theme.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/* Tachometer: 0-8 x1000 r/min, 250 r/min per minor tick, redline at 6500      */
/* -------------------------------------------------------------------------- */
static const char *const rpm_labels[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", NULL
};

static const gauge_config_t s_rpm = {
    .caption      = "RPM",
    .unit         = "x1000 r/min",
    .wordmark     = "R-GAUGE",
    .tagline      = "PRECISION INSTRUMENT",
    .min          = 0.0f,
    .max          = 8000.0f,
    .major_step   = 1000.0f,
    .minor_per_major = 4,
    .tick_labels  = rpm_labels,
    .alarm_from   = 6500.0f,
    .decimals     = 0,
    .slew_time    = 0.30f,
    .theme        = &gauge_theme_greddy,
};

/* -------------------------------------------------------------------------- */
/* Coolant temperature: 50-150 deg C, the classic "CL TEMP" instrument         */
/* -------------------------------------------------------------------------- */
static const gauge_config_t s_temp = {
    .caption      = "CL TEMP",
    .unit         = "DEG C",
    .wordmark     = "R-GAUGE",
    .tagline      = "PRECISION INSTRUMENT",
    .min          = 50.0f,
    .max          = 150.0f,
    .major_step   = 10.0f,
    .minor_per_major = 2,
    .tick_labels  = NULL,
    .alarm_from   = 115.0f,
    .decimals     = 0,
    .slew_time    = 2.0f,
    .theme        = &gauge_theme_greddy,
};

/* -------------------------------------------------------------------------- */
/* Boost: -1.0 .. 2.0 bar, 0.1 bar per minor tick                              */
/* -------------------------------------------------------------------------- */
static const gauge_config_t s_boost = {
    .caption      = "BOOST",
    .unit         = "BAR",
    .wordmark     = "R-GAUGE",
    .tagline      = "PRECISION INSTRUMENT",
    .min          = -1.0f,
    .max          = 2.0f,
    .major_step   = 0.5f,
    .minor_per_major = 5,
    .tick_labels  = NULL,
    .alarm_from   = 1.75f,
    .decimals     = 1,
    .slew_time    = 0.35f,
    .theme        = &gauge_theme_greddy,
};

/* -------------------------------------------------------------------------- */
/* Battery volts: 8-16 V                                                      */
/* -------------------------------------------------------------------------- */
static const gauge_config_t s_volts = {
    .caption      = "VOLTS",
    .unit         = "V DC",
    .wordmark     = "R-GAUGE",
    .tagline      = "PRECISION INSTRUMENT",
    .min          = 8.0f,
    .max          = 16.0f,
    .major_step   = 1.0f,
    .minor_per_major = 2,
    .tick_labels  = NULL,
    .alarm_from   = 16.1f,   /* above max -> no warning band */
    .decimals     = 1,
    .slew_time    = 1.2f,
    .theme        = &gauge_theme_amber,
};

static const gauge_preset_t s_presets[] = {
    { "rpm",   "Tachometer 0-8 x1000 r/min", &s_rpm   },
    { "temp",  "Coolant temperature 50-150 C", &s_temp  },
    { "boost", "Boost -1.0 .. 2.0 bar",       &s_boost },
    { "volts", "Battery 8-16 V",              &s_volts },
    { NULL,    NULL,                          NULL     },
};

const gauge_config_t *gauge_preset_rpm(void)   { return &s_rpm; }
const gauge_config_t *gauge_preset_temp(void)  { return &s_temp; }
const gauge_config_t *gauge_preset_boost(void) { return &s_boost; }
const gauge_config_t *gauge_preset_volts(void) { return &s_volts; }

const gauge_preset_t *gauge_presets_all(void)
{
    return s_presets;
}

const gauge_preset_t *gauge_preset_find(const char *id)
{
    if (!id) {
        return NULL;
    }
    for (const gauge_preset_t *p = s_presets; p->id; p++) {
        if (strcmp(p->id, id) == 0) {
            return p;
        }
    }
    return NULL;
}
