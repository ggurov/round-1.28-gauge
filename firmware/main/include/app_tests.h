/*
 * app_tests.h - the bring-up test screens.
 *
 * These exist to answer one question before any gauge code gets written: does
 * the panel draw every pixel it is given?  A solid fill is the decisive one -
 * if a region stays dark when the whole screen is white, no amount of
 * rendering care will fix it.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Draw one named test screen: "bars", "grid", "circle", "quad", "fill". */
void app_show_test(const char *name);

/* Cycle to the next test screen. */
void app_next_test(void);

#ifdef __cplusplus
}
#endif
