/*
 * Minimal esp_err.h for host tests.
 *
 * bsp.h includes the real one, which needs ESP-IDF.  Putting this directory
 * first on the include path lets gfx.c and gauge_render.c be compiled and
 * exercised on the desktop against a stubbed panel.
 */
#pragma once

#include <stdint.h>

typedef int esp_err_t;

#define ESP_OK                  0
#define ESP_FAIL                -1
#define ESP_ERR_NO_MEM          0x101
#define ESP_ERR_INVALID_ARG     0x102
#define ESP_ERR_INVALID_STATE   0x103
#define ESP_ERR_INVALID_SIZE    0x104
#define ESP_ERR_NOT_FOUND       0x105
#define ESP_ERR_NOT_SUPPORTED   0x106
#define ESP_ERR_TIMEOUT         0x107

/* bsp.h also pulls in esp_lcd_types.h; kept alongside this header. */
