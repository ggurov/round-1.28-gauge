/*
 * gauge_render.h - the gauge, drawn straight into the gfx framebuffer.
 *
 * Same separation as before: gauge_math/theme/presets hold the numbers and the
 * look, this turns them into pixels.  It redraws the whole dial each frame -
 * at 240x240 that costs a couple of milliseconds, far less than the SPI
 * transfer, and it means there is no cached state to get out of step with the
 * panel.
 */
#pragma once

#include <stdbool.h>

#include "gauge_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gauge_render gauge_render_t;

gauge_render_t *gauge_render_create(const gauge_config_t *cfg);
void gauge_render_destroy(gauge_render_t *g);

/* Pushes a reading; the needle slews toward it over the next ticks. */
void gauge_render_set_value(gauge_render_t *g, float value);
/* Jumps straight there. */
void gauge_render_set_immediate(gauge_render_t *g, float value);

float gauge_render_displayed(const gauge_render_t *g);
float gauge_render_target(const gauge_render_t *g);
const gauge_config_t *gauge_render_config(const gauge_render_t *g);

/* Advance the slew by `dt` seconds and draw the dial into the framebuffer. */
void gauge_render_tick(gauge_render_t *g, float dt);
/* Draw without advancing the slew. */
void gauge_render_draw(gauge_render_t *g);

/* True while the needle is still moving. */
bool gauge_render_moving(const gauge_render_t *g);

#ifdef __cplusplus
}
#endif
