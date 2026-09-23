/*
 * gauge_presets.h - the gauge catalogue.
 *
 * Adding a gauge type is: write a preset here, add it to gauge_presets_all(),
 * and it shows up in the `gauge` console command automatically.
 */
#pragma once

#include "gauge_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char           *id;     /* console id, e.g. "rpm"  */
    const char           *name;   /* human readable          */
    const gauge_config_t *cfg;
} gauge_preset_t;

const gauge_config_t *gauge_preset_rpm(void);    /* tachometer, 0-8000 r/min */
const gauge_config_t *gauge_preset_temp(void);   /* coolant,    50-150 deg C */
const gauge_config_t *gauge_preset_boost(void);  /* boost,      -1..2 bar    */
const gauge_config_t *gauge_preset_volts(void);  /* battery,    8..16 V      */

/* NULL-terminated table of every preset, for menus and the console. */
const gauge_preset_t *gauge_presets_all(void);

/* Number of entries in gauge_presets_all(), excluding the terminator. */
int gauge_presets_count(void);

/* Look up a preset by id; NULL when unknown. */
const gauge_preset_t *gauge_preset_find(const char *id);

/* Preset at `index`, or NULL when out of range. */
const gauge_preset_t *gauge_preset_at(int index);

#ifdef __cplusplus
}
#endif
