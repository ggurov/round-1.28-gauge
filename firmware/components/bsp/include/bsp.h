/*
 * bsp.h - board support for the Waveshare ESP32-S3-LCD-1.28.
 *
 * Deliberately thin and dependency-free: SPI, the GC9A01A panel, the
 * backlight, and nothing else.  There is no graphics library underneath - the
 * gfx component owns the framebuffer and pushes rectangles through
 * bsp_lcd_draw_bitmap().
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LCD_H_RES 240
#define BSP_LCD_V_RES 240

/* Brings up SPI, the panel and the backlight.  Returns an error instead of
 * aborting so a dead panel never costs you the serial console. */
esp_err_t bsp_display_init(void);

/* NULL until bsp_display_init() succeeds. */
esp_lcd_panel_handle_t bsp_lcd_panel(void);

/* Push a rectangle of RGB565 pixels.  x1/y1 are exclusive.  Blocking. */
esp_err_t bsp_lcd_draw_bitmap(int x0, int y0, int x1, int y1, const uint16_t *pixels);

/* Fill a rectangle with one colour; uses a small internal line buffer. */
esp_err_t bsp_lcd_fill_rect(int x0, int y0, int x1, int y1, uint16_t colour);

/* 0..100 */
void bsp_backlight_set(int percent);
int  bsp_backlight_get(void);

/* Number of completed panel transfers, for diagnostics. */
uint32_t bsp_lcd_flush_count(void);

#ifdef __cplusplus
}
#endif
