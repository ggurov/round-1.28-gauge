/*
 * lvgl_test_env.h - a headless LVGL display for on-target tests.
 *
 * Deliberately does not touch the panel or SPI: the widget tests only care
 * about the object tree and the layout arithmetic, so they should run even on a
 * board with a dead panel or a misconfigured bus.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_DISPLAY_W 240
#define TEST_DISPLAY_H 240

/* lv_init() + a 240x240 display with a flush callback that completes instantly. */
lv_display_t *lvgl_test_env_init(void);

/* The display created by lvgl_test_env_init(), or NULL. */
lv_display_t *lvgl_test_env_display(void);

/* Drains LVGL's timers and forces a full refresh of the active screen. */
void lvgl_test_env_render(void);

/* Fresh screen with no children, so each test starts from a clean slate. */
lv_obj_t *lvgl_test_env_fresh_screen(void);

/* Current allocation of LVGL's own heap, in bytes; -1 when unavailable. */
long lvgl_test_env_mem_used(void);

#ifdef __cplusplus
}
#endif
