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

/*
 * Flush behaviour and diagnostics.
 *
 * In synchronous mode the flush callback waits for the SPI transfer to finish
 * before releasing the LVGL draw buffer, which costs render/DMA overlap but
 * makes lost tiles impossible.  In asynchronous mode the ISR releases the
 * buffer, which is faster but relies on the handshake being tight.
 */
void bsp_flush_set_sync(bool sync);
bool bsp_flush_get_sync(void);
void bsp_flush_get_stats(uint32_t *count, uint32_t *errors, uint32_t *timeouts);

/* "full" or "partial", matching CONFIG_BSP_LCD_RENDER_FULL. */
const char *bsp_render_mode(void);
/* Bytes per draw buffer. */
uint32_t bsp_draw_buffer_bytes(void);

#ifdef __cplusplus
}
#endif
