/*
 * app_tests.c - bring-up test screens, drawn straight into the framebuffer.
 */
#include "app_tests.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app_gauge.h"
#include "bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx.h"

static const char *TAG = "tests";

#define CX (GFX_W / 2)
#define CY (GFX_H / 2)

static const char *const k_tests[] = {"fill", "bars", "grid", "circle", "quad"};
#define TEST_COUNT (sizeof(k_tests) / sizeof(k_tests[0]))
static int s_current;

/* -------------------------------------------------------------------------- */

/* Every pixel white.  Anything dark here is the panel, not the drawing. */
static void test_fill(void)
{
    gfx_clear(gfx_rgb(255, 255, 255));
}

/* Colour bars in a known order, so the byte order and inversion are obvious. */
static void test_bars(void)
{
    static const uint32_t cols[] = {
        0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF,
        0xFFFF00, 0x00FFFF, 0xFF00FF, 0x000000,
    };
    const int n = sizeof(cols) / sizeof(cols[0]);
    const int w = GFX_W / n;
    for (int i = 0; i < n; i++) {
        gfx_fill_rect(i * w, 0, (i + 1) * w - 1, GFX_H - 1, gfx_hex(cols[i]));
    }
    /* a white top strip and a black bottom strip, to show where the edges are */
    gfx_fill_rect(0, 0, GFX_W - 1, 3, gfx_rgb(255, 255, 255));
    gfx_fill_rect(0, GFX_H - 4, GFX_W - 1, GFX_H - 1, gfx_rgb(0, 0, 0));
}

/* A 1 px grid every 16 px.  Missing lines show up as gaps in a regular
 * pattern, which is much easier to read than gaps in a picture. */
static void test_grid(void)
{
    gfx_clear(gfx_rgb(0, 0, 0));
    for (int x = 0; x < GFX_W; x += 16) {
        gfx_vline(0, GFX_H - 1, x, gfx_rgb(0, 90, 0));
    }
    for (int y = 0; y < GFX_H; y += 16) {
        gfx_hline(0, GFX_W - 1, y, gfx_rgb(0, 90, 0));
    }
    /* brighter lines every 64 px as landmarks */
    for (int x = 0; x < GFX_W; x += 64) {
        gfx_vline(0, GFX_H - 1, x, gfx_rgb(0, 255, 0));
    }
    for (int y = 0; y < GFX_H; y += 64) {
        gfx_hline(0, GFX_W - 1, y, gfx_rgb(0, 255, 0));
    }
    gfx_rect(0, 0, GFX_W - 1, GFX_H - 1, gfx_rgb(255, 255, 255));
}

/* Concentric circles and a crosshair: geometry, and the round bezel. */
static void test_circle(void)
{
    gfx_clear(gfx_rgb(0, 0, 0));
    gfx_circle(CX, CY, 119, gfx_rgb(255, 255, 255));
    gfx_circle(CX, CY, 60, gfx_rgb(0, 200, 60));
    gfx_circle(CX, CY, 30, gfx_rgb(0, 120, 40));
    gfx_hline(0, GFX_W - 1, CY, gfx_rgb(60, 60, 60));
    gfx_vline(0, GFX_H - 1, CX, gfx_rgb(60, 60, 60));
    gfx_disc(CX, CY, 6, gfx_rgb(255, 60, 0));
    /* four quadrant ticks, so rotation is unambiguous */
    gfx_fill_rect(CX - 2, 0, CX + 2, 20, gfx_rgb(255, 0, 0));
    gfx_fill_rect(GFX_W - 21, CY - 2, GFX_W - 1, CY + 2, gfx_rgb(0, 255, 0));
    gfx_fill_rect(CX - 2, GFX_H - 21, CX + 2, GFX_H - 1, gfx_rgb(0, 0, 255));
    gfx_fill_rect(0, CY - 2, 20, CY + 2, gfx_rgb(255, 255, 0));
}

/* Four flat quadrants: the quickest way to see which region of the panel is
 * not being driven. */
static void test_quad(void)
{
    gfx_fill_rect(0, 0, CX - 1, CY - 1, gfx_rgb(255, 0, 0));
    gfx_fill_rect(CX, 0, GFX_W - 1, CY - 1, gfx_rgb(0, 255, 0));
    gfx_fill_rect(0, CY, CX - 1, GFX_H - 1, gfx_rgb(0, 0, 255));
    gfx_fill_rect(CX, CY, GFX_W - 1, GFX_H - 1, gfx_rgb(255, 255, 0));
    gfx_rect(0, 0, GFX_W - 1, GFX_H - 1, gfx_rgb(255, 255, 255));
}

/* -------------------------------------------------------------------------- */

void app_show_test(const char *name)
{
    /* a test screen owns the display; stop the gauge redrawing over it */
    app_gauge_stop();

    if (!name || !*name) {
        name = k_tests[s_current];
    } else {
        for (int i = 0; i < (int)TEST_COUNT; i++) {
            if (strcmp(name, k_tests[i]) == 0) {
                s_current = i;
                break;
            }
        }
    }

    const char *which = k_tests[s_current];
    if (strcmp(which, "fill") == 0) {
        test_fill();
    } else if (strcmp(which, "bars") == 0) {
        test_bars();
    } else if (strcmp(which, "grid") == 0) {
        test_grid();
    } else if (strcmp(which, "circle") == 0) {
        test_circle();
    } else {
        test_quad();
    }

    gfx_flush();
    ESP_LOGI(TAG, "test screen: %s", which);
}

void app_next_test(void)
{
    s_current = (s_current + 1) % (int)TEST_COUNT;
    app_show_test(NULL);
}
