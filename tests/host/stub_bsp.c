/*
 * stub_bsp.c - the only thing the graphics layer asks of the board.
 *
 * gfx.c talks to the panel through exactly one function, so replacing it lets
 * every drawing primitive and the whole gauge renderer be tested on the host:
 * the framebuffer is real, only the SPI is stubbed.
 */
#include "stub_bsp.h"

#include <string.h>

static int s_last_x0, s_last_y0, s_last_x1, s_last_y1;
static uint32_t s_calls;
static uint32_t s_fail_after = UINT32_MAX;
static int s_backlight;

esp_err_t bsp_lcd_draw_bitmap(int x0, int y0, int x1, int y1, const uint16_t *pixels)
{
    (void)pixels;
    s_last_x0 = x0;
    s_last_y0 = y0;
    s_last_x1 = x1;
    s_last_y1 = y1;
    if (s_calls >= s_fail_after) {
        return ESP_ERR_TIMEOUT;
    }
    s_calls++;
    return ESP_OK;
}

esp_err_t bsp_lcd_fill_rect(int x0, int y0, int x1, int y1, uint16_t colour)
{
    (void)x0; (void)y0; (void)x1; (void)y1; (void)colour;
    return ESP_OK;
}

esp_lcd_panel_handle_t bsp_lcd_panel(void)
{
    return (esp_lcd_panel_handle_t)1;
}

esp_err_t bsp_display_init(void)
{
    return ESP_OK;
}

void bsp_backlight_set(int percent) { s_backlight = percent; }
int  bsp_backlight_get(void)        { return s_backlight; }
uint32_t bsp_lcd_flush_count(void)  { return s_calls; }

/* --- test hooks ---------------------------------------------------------- */

uint32_t stub_bsp_calls(void) { return s_calls; }

void stub_bsp_reset(void)
{
    s_calls = 0;
    s_fail_after = UINT32_MAX;
    s_last_x0 = s_last_y0 = s_last_x1 = s_last_y1 = -1;
}

void stub_bsp_fail_after(uint32_t n) { s_fail_after = n; }

void stub_bsp_last_rect(int *x0, int *y0, int *x1, int *y1)
{
    if (x0) *x0 = s_last_x0;
    if (y0) *y0 = s_last_y0;
    if (x1) *x1 = s_last_x1;
    if (y1) *y1 = s_last_y1;
}
