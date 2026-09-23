/*
 * gauge.c - config-driven automotive gauge widget.
 *
 * Rendered entirely from LVGL primitives so switching gauge type is a matter
 * of swapping configuration:
 *
 *   glow    lv_arc    soft bloom behind the scale rail
 *   scale   lv_scale  ROUND_INNER: rail, major/minor ticks, numerals, alarm
 *   needle  lv_image  generated blade sprite, rotated about the dial centre
 *   hub     lv_obj    black centre cap that hides the pivot
 *   text    lv_label  caption / value / units / wordmark
 *
 * A 16 ms lv_timer slews `displayed` toward `target` via gauge_math_slew_step
 * so the needle behaves like a real movement rather than snapping.
 */
#include "gauge.h"

#include <math.h>
#include <string.h>

#include "gauge_math.h"
#include "gauge_needle.h"

#define GAUGE_TICK_PERIOD_MS 16
#define GAUGE_TICK_SECONDS   (GAUGE_TICK_PERIOD_MS / 1000.0f)
#define GAUGE_TEXT_SLOTS     8

/* -------------------------------------------------------------------------- */
/* fonts                                                                      */
/* -------------------------------------------------------------------------- */

static const lv_font_t *font_for(gauge_font_t f)
{
    switch (f) {
#if LV_FONT_MONTSERRAT_10
    case GAUGE_FONT_10: return &lv_font_montserrat_10;
#endif
#if LV_FONT_MONTSERRAT_12
    case GAUGE_FONT_12: return &lv_font_montserrat_12;
#endif
#if LV_FONT_MONTSERRAT_14
    case GAUGE_FONT_14: return &lv_font_montserrat_14;
#endif
#if LV_FONT_MONTSERRAT_16
    case GAUGE_FONT_16: return &lv_font_montserrat_16;
#endif
#if LV_FONT_MONTSERRAT_18
    case GAUGE_FONT_18: return &lv_font_montserrat_18;
#endif
#if LV_FONT_MONTSERRAT_20
    case GAUGE_FONT_20: return &lv_font_montserrat_20;
#endif
#if LV_FONT_MONTSERRAT_30
    case GAUGE_FONT_30: return &lv_font_montserrat_30;
#endif
#if LV_FONT_MONTSERRAT_36
    case GAUGE_FONT_36: return &lv_font_montserrat_36;
#endif
    default: return NULL;   /* inherit whatever LVGL has as default */
    }
}

static inline lv_color_t col(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

/* -------------------------------------------------------------------------- */
/* state                                                                      */
/* -------------------------------------------------------------------------- */

struct gauge {
    lv_obj_t *root;
    lv_obj_t *glow;
    lv_obj_t *scale;
    lv_obj_t *needle;
    lv_obj_t *hub;
    lv_obj_t *labels[GAUGE_TEXT_SLOTS];   /* 0 wordmark, 1 tagline, 2 caption,
                                             3 value, 4 unit */

    lv_timer_t *tick;

    lv_style_t style_alarm_main;
    lv_style_t style_alarm_items;

    gauge_config_t cfg;
    gauge_scale_t  scale_math;

    /* resolved geometry */
    int   dial_diameter;
    int   rail_radius;
    int   scale_diameter;
    int   needle_len;
    int   label_radius;
    int   pad_radial;
    bool  has_alarm;

    /* LVGL keeps pointers to these, not copies */
    char        label_buf[GAUGE_MAX_TICKS][GAUGE_LABEL_LEN];
    const char *label_ptrs[GAUGE_MAX_TICKS + 1];

    /* needle dynamics */
    float   target;
    float   displayed;
    float   slew_alpha;
    float   max_step;
    float   epsilon;
    int32_t last_rot_01deg;

    char value_text[GAUGE_LABEL_LEN];
};

enum {
    LBL_WORDMARK = 0,
    LBL_TAGLINE,
    LBL_CAPTION,
    LBL_VALUE,
    LBL_UNIT,
};

/* -------------------------------------------------------------------------- */
/* painting                                                                   */
/* -------------------------------------------------------------------------- */

static void apply_needle(gauge_t *g)
{
    if (!g->needle) {
        return;
    }

    int32_t rot = (int32_t)lroundf(gauge_math_value_to_angle(&g->scale_math, g->displayed) * 10.0f);
    if (rot != g->last_rot_01deg) {
        g->last_rot_01deg = rot;
        lv_image_set_rotation(g->needle, rot);
    }

    char buf[GAUGE_LABEL_LEN];
    gauge_math_format(&g->scale_math, g->displayed, buf, sizeof(buf));
    if (strcmp(buf, g->value_text) != 0) {
        strncpy(g->value_text, buf, sizeof(g->value_text) - 1);
        g->value_text[sizeof(g->value_text) - 1] = '\0';
        lv_label_set_text(g->labels[LBL_VALUE], g->value_text);
    }
}

static void tick_cb(lv_timer_t *t)
{
    gauge_t *g = (gauge_t *)lv_timer_get_user_data(t);

    if (g->displayed != g->target) {
        g->displayed = gauge_math_slew_step(g->displayed, g->target,
                                            g->slew_alpha, g->max_step, g->epsilon);
        apply_needle(g);
    }
}

/* -------------------------------------------------------------------------- */
/* construction helpers                                                       */
/* -------------------------------------------------------------------------- */

static void build_tick_labels(gauge_t *g)
{
    const gauge_config_t *c = &g->cfg;
    int majors = gauge_math_major_ticks(&g->scale_math);

    if (majors > GAUGE_MAX_TICKS) {
        majors = GAUGE_MAX_TICKS;
    }

    for (int i = 0; i < majors; i++) {
        if (c->tick_labels) {
            g->label_ptrs[i] = c->tick_labels[i];
        } else {
            gauge_math_major_label(&g->scale_math, i, g->label_buf[i], GAUGE_LABEL_LEN);
            g->label_ptrs[i] = g->label_buf[i];
        }
    }
    g->label_ptrs[majors] = NULL;
}

static void build_glow(gauge_t *g)
{
    const gauge_theme_t *th = g->cfg.theme;
    int glow_w = th->band_width * 4;
    int d = 2 * g->rail_radius + glow_w;

    lv_obj_t *glow = lv_arc_create(g->root);
    g->glow = glow;

    lv_obj_set_size(glow, d, d);
    lv_obj_align(glow, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_style(glow, NULL, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(glow, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(glow, 0, 0);

    /*
     * lv_arc angles are degrees clockwise from 3 o'clock and must be given
     * within 0..360.  A 270-degree sweep starting at rotation 135 ends at 405,
     * which LVGL does not wrap for us - it would draw from 135 to 45 the short
     * way, i.e. a quarter of the dial.  Normalise both ends; the wrap is then
     * expressed by start > end.
     */
    int a0 = ((g->cfg.rotation % 360) + 360) % 360;
    int a1 = (((g->cfg.rotation + g->cfg.angle_range) % 360) + 360) % 360;
    lv_arc_set_bg_angles(glow, a0, a1);
    lv_arc_set_angles(glow, 0, 0);

    lv_obj_set_style_arc_width(glow, glow_w, LV_PART_MAIN);
    lv_obj_set_style_arc_color(glow, col(th->band_glow), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(glow, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(glow, true, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(glow, LV_OPA_TRANSP, LV_PART_INDICATOR);
}

static void build_scale(gauge_t *g)
{
    const gauge_config_t *c = &g->cfg;
    const gauge_theme_t *th = c->theme;

    lv_obj_t *scale = lv_scale_create(g->root);
    g->scale = scale;

    lv_obj_set_size(scale, g->scale_diameter, g->scale_diameter);
    lv_obj_align(scale, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(scale, 0, 0);
    lv_obj_set_style_bg_opa(scale, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scale, 0, 0);

    lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(scale, (int32_t)lroundf(c->min), (int32_t)lroundf(c->max));
    lv_scale_set_angle_range(scale, c->angle_range);
    lv_scale_set_rotation(scale, c->rotation);
    lv_scale_set_total_tick_count(scale, gauge_math_total_ticks(&g->scale_math));
    lv_scale_set_major_tick_every(scale, c->minor_per_major);
    lv_scale_set_draw_ticks_on_top(scale, true);

    build_tick_labels(g);
    lv_scale_set_label_show(scale, true);
    lv_scale_set_text_src(scale, g->label_ptrs);

    /* rail == the glowing band that runs outside the ticks */
    lv_obj_set_style_arc_width(scale, th->band_width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(scale, col(th->band), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(scale, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(scale, true, LV_PART_MAIN);

    /* minor ticks */
    lv_obj_set_style_length(scale, th->tick_minor_len, LV_PART_ITEMS);
    lv_obj_set_style_line_width(scale, th->tick_minor_width, LV_PART_ITEMS);
    lv_obj_set_style_line_color(scale, col(th->tick_minor), LV_PART_ITEMS);
    lv_obj_set_style_line_opa(scale, LV_OPA_COVER, LV_PART_ITEMS);

    /* major ticks + their numerals */
    lv_obj_set_style_length(scale, th->tick_major_len, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(scale, th->tick_major_width, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(scale, col(th->tick_major), LV_PART_INDICATOR);
    lv_obj_set_style_line_opa(scale, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(scale, col(th->label), LV_PART_INDICATOR);
    lv_obj_set_style_text_opa(scale, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_text_letter_space(scale, th->label_letter_space, LV_PART_INDICATOR);
    const lv_font_t *lf = font_for(th->font_label);
    if (lf) {
        lv_obj_set_style_text_font(scale, lf, LV_PART_INDICATOR);
    }
    lv_obj_set_style_pad_radial(scale, g->pad_radial, LV_PART_INDICATOR);

    /* warning / redline section */
    if (g->has_alarm) {
        lv_style_init(&g->style_alarm_items);
        lv_style_set_line_color(&g->style_alarm_items, col(th->alarm));
        lv_style_set_line_width(&g->style_alarm_items, th->tick_major_width);
        lv_style_set_line_opa(&g->style_alarm_items, LV_OPA_COVER);

        lv_style_init(&g->style_alarm_main);
        lv_style_set_arc_color(&g->style_alarm_main, col(th->alarm));
        lv_style_set_arc_width(&g->style_alarm_main, th->band_width);
        lv_style_set_arc_opa(&g->style_alarm_main, LV_OPA_COVER);

        lv_scale_section_t *sec = lv_scale_add_section(scale);
        if (sec) {
            lv_scale_section_set_range(sec, (int32_t)lroundf(c->alarm_from), (int32_t)lroundf(c->max));
            lv_scale_set_section_style_indicator(scale, sec, &g->style_alarm_items);
            lv_scale_set_section_style_main(scale, sec, &g->style_alarm_main);
        } else {
            g->has_alarm = false;
        }
    }
}

static void build_needle(gauge_t *g)
{
    const gauge_theme_t *th = g->cfg.theme;
    const lv_image_dsc_t *dsc = gauge_needle_image();
    if (!dsc) {
        return;
    }

    lv_obj_t *img = lv_image_create(g->root);
    g->needle = img;

    lv_image_set_src(img, dsc);
    lv_obj_set_size(img, GAUGE_NEEDLE_W, GAUGE_NEEDLE_H);
    lv_image_set_pivot(img, GAUGE_NEEDLE_PIVOT_X, GAUGE_NEEDLE_PIVOT_Y);
    lv_obj_set_pos(img,
                   g->dial_diameter / 2 - GAUGE_NEEDLE_PIVOT_X,
                   g->dial_diameter / 2 - GAUGE_NEEDLE_PIVOT_Y);

    /* tint the white sprite with the theme colour */
    lv_obj_set_style_image_recolor(img, col(th->needle), LV_PART_MAIN);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, LV_PART_MAIN);

    int32_t sc = (int32_t)lroundf(256.0f * (float)g->needle_len / (float)GAUGE_NEEDLE_TIP_DISTANCE);
    if (sc != 256) {
        lv_image_set_scale(img, (uint32_t)sc);
    }
}

static void build_hub(gauge_t *g)
{
    const gauge_theme_t *th = g->cfg.theme;
    lv_obj_t *hub = lv_obj_create(g->root);
    g->hub = hub;

    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, th->hub_radius * 2, th->hub_radius * 2);
    lv_obj_align(hub, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, col(th->needle_hub), 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(hub, col(th->needle_hub_ring), 0);
    lv_obj_set_style_border_width(hub, 2, 0);
    lv_obj_set_style_border_opa(hub, LV_OPA_COVER, 0);
}

static lv_obj_t *make_label(gauge_t *g, int slot, gauge_font_t font, uint32_t color,
                            const char *text, int y_ofs, int letter_space)
{
    if (!text) {
        return NULL;
    }
    lv_obj_t *lbl = lv_label_create(g->root);
    const lv_font_t *f = font_for(font);
    if (f) {
        lv_obj_set_style_text_font(lbl, f, 0);
    }
    lv_obj_set_style_text_color(lbl, col(color), 0);
    if (letter_space) {
        lv_obj_set_style_text_letter_space(lbl, letter_space, 0);
    }
    lv_label_set_text(lbl, text);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, y_ofs);
    g->labels[slot] = lbl;
    return lbl;
}

static void build_text(gauge_t *g)
{
    const gauge_config_t *c = &g->cfg;
    const gauge_theme_t *th = c->theme;
    int hub = th->hub_radius;

    make_label(g, LBL_WORDMARK, th->font_wordmark, th->wordmark, c->wordmark, -(hub + 26), 2);
    make_label(g, LBL_TAGLINE,  th->font_tagline,  th->tagline,  c->tagline,  -(hub + 10), 1);
    make_label(g, LBL_CAPTION,  th->font_caption,  th->caption,  c->caption,   (hub + 16), 1);
    make_label(g, LBL_UNIT,     th->font_unit,     th->unit,     c->unit,      (hub + 68), 1);
    make_label(g, LBL_VALUE,    th->font_value,    th->value,    "0",          (hub + 42), 0);
}

/* -------------------------------------------------------------------------- */
/* lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

gauge_t *gauge_create(lv_obj_t *parent, const gauge_config_t *cfg)
{
    if (!parent || !cfg || !cfg->theme) {
        return NULL;
    }

    gauge_t *g = lv_malloc_zeroed(sizeof(*g));
    if (!g) {
        return NULL;
    }
    g->cfg = *cfg;
    gauge_math_from_config(&g->cfg, &g->scale_math);
    g->cfg.angle_range = g->scale_math.angle_range;
    g->cfg.rotation = g->scale_math.rotation;
    g->cfg.minor_per_major = g->scale_math.minor_per_major;

    const gauge_theme_t *th = g->cfg.theme;

    /* --- resolve geometry (see gauge_math.c for the formulae) ------------ */
    lv_coord_t w = lv_obj_get_content_width(parent);
    lv_coord_t h = lv_obj_get_content_height(parent);
    int side = (int)((w < h) ? w : h);
    if (side <= 0) {
        side = 240;
    }
    g->dial_diameter = side;
    g->rail_radius = gauge_math_rail_radius(side, th->bezel_width, th->band_gap, th->band_width);
    g->scale_diameter = gauge_math_scale_diameter(g->rail_radius);

    g->needle_len = (cfg->needle_length > 0.0f)
                        ? (int)lroundf(cfg->needle_length)
                        : gauge_math_needle_length(g->rail_radius, th->tick_major_len);

    int glyph_h = 20;
    const lv_font_t *lf = font_for(th->font_label);
    if (lf) {
        glyph_h = lv_font_get_line_height(lf);
    }
    g->label_radius = (cfg->label_radius > 0.0f)
                          ? (int)lroundf(cfg->label_radius)
                          : (g->rail_radius - th->tick_major_len - 4 - glyph_h / 2);
    g->pad_radial = gauge_math_pad_radial_for(g->rail_radius, th->tick_major_len,
                                              g->label_radius, th->label_letter_space);

    g->has_alarm = (cfg->alarm_from <= cfg->max && cfg->alarm_from >= cfg->min);

    /* --- needle dynamics -------------------------------------------------- */
    float slew_time = (cfg->slew_time > 0.0f) ? cfg->slew_time : 0.35f;
    float range = cfg->max - cfg->min;
    if (range <= 0.0f) {
        range = 1.0f;
    }
    g->slew_alpha = gauge_math_slew_alpha(GAUGE_TICK_SECONDS, slew_time);
    g->max_step = gauge_math_slew_max_step(range, GAUGE_TICK_SECONDS, slew_time);
    g->epsilon = range * 1e-4f;

    g->target = cfg->min;
    g->displayed = cfg->min;
    g->last_rot_01deg = INT32_MIN;
    g->value_text[0] = '\0';

    /* --- widget tree, painted back to front ------------------------------- */
    g->root = lv_obj_create(parent);
    lv_obj_remove_style_all(g->root);
    lv_obj_set_size(g->root, side, side);
    lv_obj_align(g->root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(g->root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g->root, col(th->face), 0);
    lv_obj_set_style_bg_opa(g->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g->root, col(th->bezel), 0);
    lv_obj_set_style_border_width(g->root, th->bezel_width, 0);
    lv_obj_set_style_border_opa(g->root, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(g->root, true, 0);
    lv_obj_set_style_pad_all(g->root, 0, 0);
    lv_obj_set_scrollable(g->root, false);

    build_glow(g);
    build_scale(g);
    build_needle(g);
    build_hub(g);
    build_text(g);

    gauge_set_value_immediate(g, cfg->min);
    g->tick = lv_timer_create(tick_cb, GAUGE_TICK_PERIOD_MS, g);

    return g;
}

void gauge_delete(gauge_t *g)
{
    if (!g) {
        return;
    }
    if (g->tick) {
        lv_timer_delete(g->tick);
        g->tick = NULL;
    }
    if (g->has_alarm) {
        lv_style_reset(&g->style_alarm_items);
        lv_style_reset(&g->style_alarm_main);
    }
    if (g->root) {
        lv_obj_delete(g->root);
        g->root = NULL;
    }
    lv_free(g);
}

/* -------------------------------------------------------------------------- */
/* value API                                                                  */
/* -------------------------------------------------------------------------- */

void gauge_set_value(gauge_t *g, float value)
{
    if (!g) {
        return;
    }
    if (value < g->cfg.min) value = g->cfg.min;
    if (value > g->cfg.max) value = g->cfg.max;
    g->target = value;
}

void gauge_set_value_immediate(gauge_t *g, float value)
{
    if (!g) {
        return;
    }
    if (value < g->cfg.min) value = g->cfg.min;
    if (value > g->cfg.max) value = g->cfg.max;
    g->target = value;
    g->displayed = value;
    apply_needle(g);
}

float gauge_get_displayed_value(const gauge_t *g)
{
    return g ? g->displayed : 0.0f;
}

float gauge_get_target_value(const gauge_t *g)
{
    return g ? g->target : 0.0f;
}

const gauge_config_t *gauge_get_config(const gauge_t *g)
{
    return g ? &g->cfg : NULL;
}

float gauge_get_value_angle(const gauge_t *g, float value)
{
    return g ? gauge_math_value_to_angle(&g->scale_math, value) : 0.0f;
}

lv_obj_t *gauge_get_obj(gauge_t *g)
{
    return g ? g->root : NULL;
}

lv_obj_t *gauge_get_needle(gauge_t *g)
{
    return g ? g->needle : NULL;
}

lv_obj_t *gauge_get_scale(gauge_t *g)
{
    return g ? g->scale : NULL;
}

lv_obj_t *gauge_get_hub(gauge_t *g)
{
    return g ? g->hub : NULL;
}

const char *gauge_get_value_text(gauge_t *g)
{
    return g ? g->value_text : NULL;
}

int gauge_widget_count(const gauge_t *g)
{
    if (!g || !g->root) {
        return 0;
    }
    int n = 1;   /* root */
    if (g->glow)   n++;
    if (g->scale)  n++;
    if (g->needle) n++;
    if (g->hub)    n++;
    for (int i = 0; i < GAUGE_TEXT_SLOTS; i++) {
        if (g->labels[i]) {
            n++;
        }
    }
    return n;
}
