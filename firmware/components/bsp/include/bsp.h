/*
 * bsp.h - board support for the Waveshare ESP32-S3-LCD-1.28.
 *
 * Wraps the GC9A01A panel, its SPI bus, the backlight PWM and the LVGL
 * port (display driver + tick + render task) behind a small API.
 */
#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Brings up SPI, the panel, the backlight, LVGL and the LVGL render task.
 * Safe to call once; returns an error instead of aborting so a dead panel
 * never costs you the serial console. */
esp_err_t bsp_display_init(void);

/* NULL until bsp_display_init() succeeds. */
lv_display_t *bsp_display_get(void);

/* 0..100 */
void bsp_backlight_set(int percent);
int  bsp_backlight_get(void);

/* Take/give the LVGL lock before touching any LVGL object from a task other
 * than the render task.  Timeout in ms; pass -1 to wait forever. */
bool bsp_lvgl_lock(int timeout_ms);
void bsp_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif
