/*
 * stub_bsp.h - hooks for the stubbed panel, for use by the host tests.
 */
#pragma once

#include <stdint.h>

#include "bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of successful bsp_lcd_draw_bitmap() calls. */
uint32_t stub_bsp_calls(void);

/* Clears the call count and the "fail from call N onwards" setting. */
void stub_bsp_reset(void);

/* Make every call from the Nth onwards fail, to exercise error paths. */
void stub_bsp_fail_after(uint32_t n);

/* Arguments of the most recent call. */
void stub_bsp_last_rect(int *x0, int *y0, int *x1, int *y1);

#ifdef __cplusplus
}
#endif
