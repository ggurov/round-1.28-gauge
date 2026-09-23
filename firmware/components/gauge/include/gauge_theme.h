/*
 * gauge_theme.h - palette + typography for the gauge renderer.
 *
 * A theme is pure data so a gauge can be re-skinned at runtime.  Add a new
 * theme by creating another gauge_theme_t and pointing gauge_config_t::theme
 * at it.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Dial */
    lv_color_t face;          /* dial background                       */
    lv_color_t face_inner;    /* subtle centre vignette                */
    lv_color_t bezel;         /* outer ring                            */
    lv_color_t bezel_shadow;  /* ring highlight / inner edge           */

    /* Scale */
    lv_color_t tick_major;
    lv_color_t tick_minor;
    lv_color_t label;         /* numerals                              */
    lv_color_t band;          /* arc rail running outside the ticks    */
    lv_color_t band_glow;     /* wider, dimmer arc behind `band`       */
    lv_color_t alarm;         /* warning/redline band colour           */

    /* Needle */
    lv_color_t needle;
    lv_color_t needle_hub;
    lv_color_t needle_hub_ring;

    /* Text */
    lv_color_t value;         /* big numeric read-out                  */
    lv_color_t caption;       /* "RPM"                                 */
    lv_color_t unit;          /* "x1000 r/min"                         */
    lv_color_t wordmark;      /* italic-style brand text               */
    lv_color_t tagline;       /* small print under the wordmark        */

    /* Typography */
    const lv_font_t *font_label;    /* numerals on the scale  */
    const lv_font_t *font_caption;
    const lv_font_t *font_value;
    const lv_font_t *font_unit;
    const lv_font_t *font_wordmark;
    const lv_font_t *font_tagline;

    /* Scale geometry, relative to the dial radius (0..1 of half the widget) */
    int tick_major_len;
    int tick_minor_len;
    int tick_major_width;
    int tick_minor_width;
    int band_width;           /* arc rail thickness                    */
    int label_pad_radial;     /* gap between major tick and its numeral */
    int bezel_width;
    int hub_radius;
} gauge_theme_t;

/* GReddy-inspired: black face, phosphor-green scale, orange blade needle,
 * magenta warning band.  The wordmark is deliberately our own - the layout
 * and palette evoke the classic 1990s Japanese gauge look without copying
 * anyone's trademark. */
extern const gauge_theme_t gauge_theme_greddy;

/* Same geometry, amber/white "motorsport" palette. */
extern const gauge_theme_t gauge_theme_amber;

#ifdef __cplusplus
}
#endif
