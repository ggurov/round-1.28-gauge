/*
 * gauge_theme.h - palette, typography and tick geometry for the renderer.
 *
 * Free of LVGL on purpose: colours are 24-bit 0xRRGGBB values that gauge.c
 * converts with lv_color_hex(), and fonts are an enum that gauge.c maps onto
 * LVGL's built-in Montserrat faces.  That keeps themes testable on the host
 * and makes the hex values directly comparable with tools/render_preview.py.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LVGL's built-in Montserrat sizes we actually enable in sdkconfig. */
typedef enum {
    GAUGE_FONT_10 = 0,
    GAUGE_FONT_12,
    GAUGE_FONT_14,
    GAUGE_FONT_16,
    GAUGE_FONT_18,
    GAUGE_FONT_20,
    GAUGE_FONT_30,
    GAUGE_FONT_36,
    GAUGE_FONT_COUNT
} gauge_font_t;

typedef struct {
    /* Dial */
    uint32_t face;            /* dial background               0xRRGGBB */
    uint32_t bezel;           /* outer ring                              */

    /* Scale */
    uint32_t tick_major;
    uint32_t tick_minor;
    uint32_t label;           /* numerals                                */
    uint32_t band;            /* arc rail outside the ticks              */
    uint32_t band_glow;       /* wider, dimmer arc behind `band`         */
    uint32_t alarm;           /* warning / redline band                  */

    /* Needle */
    uint32_t needle;
    uint32_t needle_hub;
    uint32_t needle_hub_ring;

    /* Text */
    uint32_t value;           /* big numeric read-out                    */
    uint32_t caption;         /* "RPM"                                   */
    uint32_t unit;            /* "x1000 r/min"                           */
    uint32_t wordmark;
    uint32_t tagline;
    uint32_t status;          /* small print under the read-out          */

    /* Typography */
    gauge_font_t font_label;
    gauge_font_t font_caption;
    gauge_font_t font_value;
    gauge_font_t font_unit;
    gauge_font_t font_wordmark;
    gauge_font_t font_tagline;

    /* Tick / band geometry, in pixels at 240x240 */
    int band_gap;             /* black gap between bezel and the rail     */
    int band_width;           /* rail thickness                           */
    int alarm_gap;            /* gap between the rail and the alarm arc   */
    int alarm_width;          /* alarm arc thickness                      */
    int tick_gap;             /* gap between the alarm arc and the ticks  */
    int tick_major_len;
    int tick_minor_len;
    int tick_major_width;     /* half-width of the major tick wedge       */
    int tick_minor_width;
    int bezel_width;
    int hub_radius;
    int label_pad_radial;     /* gap between major tick and its numeral   */
    int label_letter_space;   /* must match the scale's text letter space */
} gauge_theme_t;

/*
 * GReddy-inspired: black face, phosphor-green scale, orange blade needle,
 * red warning band.  The wordmark shipped in the presets is our own - the
 * layout and palette are a homage, not a copy of anyone's trademark.
 */
extern const gauge_theme_t gauge_theme_greddy;

/* Same geometry, amber/red "motorsport" palette. */
extern const gauge_theme_t gauge_theme_amber;

/* Safely map an enum value onto a font table index. */
int gauge_theme_font_index(gauge_font_t f);

#ifdef __cplusplus
}
#endif
