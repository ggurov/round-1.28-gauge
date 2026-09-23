/*
 * gauge.c - config-driven automotive gauge widget.
 *
 * Rendered entirely from LVGL primitives so that switching gauge type is a
 * matter of swapping the configuration:
 *
 *   glow arc   LVGL arc      -> the soft bloom behind the scale rail
 *   scale      lv_scale      -> rail, major/minor ticks, numerals, warning band
 *   needle     lv_image      -> the generated blade sprite, rotated about the
 *                               dial centre
 *   hub        LVGL obj      -> the black centre cap that hides the pivot
 *   read-out   lv_label      -> caption / value / units / wordmark
 *
 * A 16 ms lv_timer slews `displayed` toward `target` so the needle behaves like
 * a real moving-coil movement rather than snapping between values.
 */
#include "gauge.h"
#include "gauge_needle.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GAUGE_SLEW_PERIOD_MS 16

struct gauge {
    lv_obj_t *root;
    lv_obj_t *glow;
    lv_obj_t *scale;
    lv_obj_t *needle;
    lv_obj_t *hub;
    lv_obj_t *lbl_wordmark;
    lv_obj_t *lbl_tagline;
    lv_obj_t *lbl_caption;
    lv_obj_t *lbl_value;
    lv_obj_t *lbl_unit;
    lv_timer_t *slew;

    /* section styling for the warning band */
    lv_style_t style_alarm_items;
    lv_style_t style_alarm_main;

    gauge_config_t cfg;

    /* resolved geometry */
    int   dial_diameter;
    int   rail_radius;
    float needle_len;
    float label_radius;

    /* label storage (LVGL keeps pointers, not copies) */
    char        label_buf[GAUGE_MAX_TICKS][GAUGE_LABEL_LEN];
    const char *label_ptrs[GAUGE_MAX_TICKS + 1];

    /* smoothing */
    float   target;
    float   displayed;
    float   slew_alpha;
    float   max_step;
    int32_t last_rot_01deg;

    /* cached text so we only touch LVGL labels when they change */
    char value_text[24];
};

/* -------------------------------------------------------------------------- */
/* helpers                                                                    */
/* -------------------------------------------------------------------------- */

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void gauge_format_value(const gauge_t *g, float v, char *out, size_t n)
{
    if (g->cfg.decimals <= 0) {
        lv_snprintf(out, n, "%d", (int)lroundf(v));
    } else if (g->cfg.decimals == 1) {
        lv_snprintf(out, n, "%.1f", (double)v);
    } else {
        lv_snprintf(out, n, "%.2f", (double)v);
    }
}

float gauge_value_to_angle(const gauge_t *g, float value)
{
    const gauge_config_t *c = &g->cfg;
    float span = c->max - c->min;
    float frac = (span > 0.0f) ? (value - c->min) / span : 0.0f;

    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;

    /* Scale angles are measured clockwise from 3 o'clock; the needle sprite
     * points at 12 o'clock when its rotation is zero, hence the +90. */
    return (float)c->rotation + frac * (float)c->angle_range + 90.0f;
}

/* -------------------------------------------------------------------------- */
/* drawing                                                                    */
/* -------------------------------------------------------------------------- */

static void gauge_redraw_needle(gauge_t *g)
{
    if (!g->needle) {
        return;
    }

    int32_t rot = (int32_t)lroundf(gauge_value_to_angle(g, g->displayed) * 10.0f);
    if (rot != g->last_rot_01deg) {
        g->last_rot_01deg = rot;
        lv_image_set_rotation(g->needle, rot);
    }

    char buf[24];
    gauge_format_value(g, g->displayed, buf, sizeof(buf));
    if (strcmp(buf, g->value_text) != 0) {
        strncpy(g->value_text, buf, sizeof(g->value_text) - 1);
        g->value_text[sizeof(g->value_text) - 1] = '\0';
        lv_label_set_text(g->lbl_value, g->value_text);
    }
}

static void gauge_slew_cb(lv_timer_t *t)
{
    gauge_t *g = (gauge_t *)lv_timer_get_user_data(t);

    float err = g->target - g->displayed;
    if (fabsf(err) <= 1e-4f) {
        if (g->displayed != g->target) {
            g->displayed = g->target;
            gauge_redraw_needle(g);
        }
        return;
    }

    float step = err * g->slew_alpha;
    if (step > g->max_step) {
        step = g->max_step;
    } else if (step < -g->max_step) {
        step = -g->max_step;
    }

    g->displayed += step;
    if (fabsf(g->target - g->displayed) < (g->cfg.max - g->cfg.min) * 1e-4f) {
        g->displayed = g->target;
    }
    gauge_redraw_needle(g);
}

/* -------------------------------------------------------------------------- */
/* construction                                                               */
/* -------------------------------------------------------------------------- */

static void build_tick_labels(gauge_t *g)
{
    const gauge_config_t *c = &g->cfg;
    float span = c->max - c->min;
    int majors = (c->major_step > 0.0f) ? (int)lroundf(span / c->major_step) : 0;
    majors = clampi(majors, 0, GAUGE_MAX_TICKS - 1);

    if (c->tick_labels) {
        /* Caller supplied its own numerals; LVGL keeps the pointers, so the
         * array has to outlive the widget (string literals are ideal). */
        for (int i = 0; i < majors; i++) {
            g->label_ptrs[i] = c->tick_labels[i];
        }
    } else {
        for (int i = 0; i < majors; i++) {
            float v = c->min + c->major_step * (float)i;
            if (c->decimals <= 0) {
                lv_snprintf(g->label_buf[i], GAUGE_LABEL_LEN, "%d", (int)lroundf(v));
            } else if (c->decimals == 1) {
                lv_snprintf(g->label_buf[i], GAUGE_LABEL_LEN, "%.1f", (double)v);
            } else {
                lv_snprintf(g->label_buf[i], GAUGE_LABEL_LEN, "%.2f", (double)v);
            }
            g->label_ptrs[i] = g->label_buf[i];
        }
    }
    g->label_ptrs[majors] = NULL;
}

static int resolve_majors(const gauge_config_t *c)
{
    float span = c->max - c->min;
    int majors = (c->major_step > 0.0f) ? (int)lroundf(span / c->major_step) : 0;
    return clampi(majors, 2, GAUGE_MAX_TICKS - 1);
}

static void build_scale(gauge_t *g)
{
    const gauge_config_t *c = &g->cfg;
    const gauge_theme_t *th = c->theme;

    lv_obj_t *scale = lv_scale_create(g->root);
    g->scale = scale;

    lv_obj_set_size(scale, g->dial_diameter, g->dial_diameter);
    lv_obj_align(scale, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(scale, 0, 0);
    lv_obj_set_style_bg_opa(scale, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scale, 0, 0);

    lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(scale, (int32_t)lroundf(c->min), (int32_t)lroundf(c->max));
    lv_scale_set_angle_range(scale, c->angle_range);
    lv_scale_set_rotation(scale, c->rotation);

    int majors = resolve_majors(c);
    int minor = c->minor_per_major > 0 ? c->minor_per_major : 1;
    lv_scale_set_total_tick_count(scale, majors * minor + 1);
    lv_scale_set_major_tick_every(scale, minor);
    lv_scale_set_draw_ticks_on_top(scale, true);

    build_tick_labels(g);
    lv_scale_set_label_show(scale, true);
    lv_scale_set_text_src(scale, g->label_ptrs);

    /* rail == the glowing band that runs outside the ticks */
    lv_obj_set_style_arc_width(scale, th->band_width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(scale, th->band, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(scale, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(scale, true, LV_PART_MAIN);

    /* minor ticks */
    lv_obj_set_style_length(scale, th->tick_minor_len, LV_PART_ITEMS);
    lv_obj_set_style_line_width(scale, th->tick_minor_width, LV_PART_ITEMS);
    lv_obj_set_style_line_color(scale, th->tick_minor, LV_PART_ITEMS);
    lv_obj_set_style_line_opa(scale, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_line_rounded(scale, false, LV_PART_ITEMS);

    /* major ticks + their numerals */
    lv_obj_set_style_length(scale, th->tick_major_len, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(scale, th->tick_major_width, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(scale, th->tick_major, LV_PART_INDICATOR);
    lv_obj_set_style_line_opa(scale, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(scale, th->label, LV_PART_INDICATOR);
    lv_obj_set_style_text_opa(scale, LV_OPA_COVER, LV_PART_INDICATOR);
    if (th->font_label) {
        lv_obj_set_style_text_font(scale, th->font_label, LV_PART_INDICATOR);
    }
    int pad = (int)lroundf((float)g->rail_radius - (float)th->tick_major_len - g->label_radius);
    lv_obj_set_style_pad_radial(scale, clampi(pad, 0, 60), LV_PART_INDICATOR);

    /* warning / redline section */
    if (c->alarm_from <= c->max && c->alarm_from >= c->min) {
        lv_style_init(&g->style_alarm_items);
        lv_style_set_line_color(&g->style_alarm_items, th->alarm);
        lv_style_set_line_width(&g->style_alarm_items, th->tick_major_width);
        lv_style_set_line_opa(&g->style_alarm_items, LV_OPA_COVER);

        lv_style_init(&g->style_alarm_main);
        lv_style_set_arc_color(&g->style_alarm_main, th->alarm);
        lv_style_set_arc_width(&g->style_alarm_main, th->band_width);
        lv_style_set_arc_opa(&g->style_alarm_main, LV_OPA_COVER);

        lv_scale_section_t *sec = lv_scale_add_section(scale);
        if (sec) {
            lv_scale_section_set_range(sec, (int32_t)lroundf(c->alarm_from), (int32_t)lroundf(c->max));
            lv_scale_section_set_style(sec, LV_PART_INDICATOR, &g->style_alarm_items);
            lv_scale_section_set_style(sec, LV_PART_MAIN, &g->style_alarm_main);
        }
    }
}

static void build_glow(gauge_t *g)
{
    const gauge_theme_t *th = g->cfg.theme;
    int band_w = th->band_width;
    int glow_w = band_w * 4;
    int d = 2 * g->rail_radius + glow_w;

    lv_obj_t *glow = lv_arc_create(g->root);
    g->glow = glow;

    lv_obj_set_size(glow, d, d);
    lv_obj_align(glow, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_style(glow, NULL, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(glow, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(glow, 0, 0);

    lv_arc_set_bg_angles(glow, g->cfg.rotation, g->cfg.rotation + g->cfg.angle_range);
    lv_arc_set_angles(glow, 0, 0);

    lv_obj_set_style_arc_width(glow, glow_w, LV_PART_MAIN);
    lv_obj_set_style_arc_color(glow, th->band_glow, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(glow, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(glow, true, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(glow, LV_OPA_TRANSP, LV_PART_INDICATOR);
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
    lv_obj_set_pos(img, (g->dial_diameter / 2) - GAUGE_NEEDLE_PIVOT_X,
                        (g->dial_diameter / 2) - GAUGE_NEEDLE_PIVOT_Y);

    /* tint the white sprite with the theme colour */
    lv_obj_set_style_image_recolor(img, th->needle, LV_PART_MAIN);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_image_opa(img, LV_OPA_COVER, LV_PART_MAIN);

    int32_t scale = (int32_t)lroundf(256.0f * g->needle_len / (float)GAUGE_NEEDLE_TIP_DISTANCE);
    if (scale != 256) {
        lv_image_set_scale(img, (uint32_t)scale);
    }
}

static lv_obj_t *make_label(gauge_t *g, const lv_font_t *font, lv_color_t color,
                            int y_ofs, int letter_space)
{
    lv_obj_t *lbl = lv_label_create(g->root);
    if (font) {
        lv_obj_set_style_text_font(lbl, font, 0);
    }
    lv_obj_set_style_text_color(lbl, color, 0);
    if (letter_space) {
        lv_obj_set_style_text_letter_space(lbl, letter_space, 0);
    }
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, y_ofs);
    return lbl;
}

static void build_text(gauge_t *g)
{
    const gauge_config_t *c = &g->cfg;
    const gauge_theme_t *th = c->theme;
    int hub = th->hub_radius;

    g->lbl_wordmark = make_label(g, th->font_wordmark, th->wordmark, -(hub + 32), 2);
    g->lbl_tagline  = make_label(g, th->font_tagline,  th->tagline,  -(hub + 16), 1);
    g->lbl_caption  = make_label(g, th->font_caption,  th->caption,   (hub + 18), 1);
    g->lbl_value    = make_label(g, th->font_value,    th->value,     (hub + 44), 0);
    g->lbl_unit     = make_label(g, th->font_unit,     th->unit,      (hub + 70), 1);

    if (c->wordmark) { lv_label_set_text(g->lbl_wordmark, c->wordmark); }
    if (c->tagline)  { lv_label_set_text(g->lbl_tagline,  c->tagline); }
    if (c->caption)  { lv_label_set_text(g->lbl_caption,  c->caption); }
    if (c->unit)     { lv_label_set_text(g->lbl_unit,     c->unit); }
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
    lv_obj_set_style_bg_color(hub, th->needle_hub, 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(hub, th->needle_hub_ring, 0);
    lv_obj_set_style_border_width(hub, 2, 0);
    lv_obj_set_style_border_opa(hub, LV_OPA_COVER, 0);
}

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

    if (g->cfg.angle_range <= 0) {
        g->cfg.angle_range = 270;
    }
    if (g->cfg.rotation == 0) {
        g->cfg.rotation = 135;
    }
    if (g->cfg.minor_per_major <= 0) {
        g->cfg.minor_per_major = 4;
    }

    const gauge_theme_t *th = g->cfg.theme;

    /* Resolve geometry from the parent's size. */
    lv_coord_t w = lv_obj_get_content_width(parent);
    lv_coord_t h = lv_obj_get_content_height(parent);
    int side = (int)((w < h) ? w : h);
    if (side <= 0) {
        side = 240;
    }
    g->dial_diameter = side;
    int scale_inset = th->bezel_width + th->band_width + 1;
    int scale_d = side - 2 * scale_inset;
    g->rail_radius = scale_d / 2;

    g->needle_len = (cfg->needle_length > 0.0f)
                        ? cfg->needle_length
                        : (float)(g->rail_radius - th->tick_major_len - 2);
    int glyph_h = th->font_label ? lv_font_get_line_height(th->font_label) : 16;
    g->label_radius = (cfg->label_radius > 0.0f)
                          ? cfg->label_radius
                          : (float)(g->rail_radius - th->tick_major_len - 4 - glyph_h / 2);

    /* slew tuning: full-scale move in cfg.slew_time seconds, exponential approach */
    float slew_time = (g->cfg.slew_time > 0.0f) ? g->cfg.slew_time : 0.35f;
    float range = (g->cfg.max - g->cfg.min);
    if (range <= 0.0f) {
        range = 1.0f;
    }
    float dt = (float)GAUGE_SLEW_PERIOD_MS / 1000.0f;
    float tau = slew_time / 3.0f;
    g->slew_alpha = 1.0f - expf(-dt / tau);
    g->max_step = range * dt / slew_time;

    g->target = g->cfg.min;
    g->displayed = g->cfg.min;
    g->last_rot_01deg = INT32_MIN;
    g->value_text[0] = '\0';

    /* --- widget tree, painted back to front --- */
    g->root = lv_obj_create(parent);
    lv_obj_remove_style_all(g->root);
    lv_obj_set_size(g->root, side, side);
    lv_obj_align(g->root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(g->root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g->root, th->face, 0);
    lv_obj_set_style_bg_opa(g->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g->root, th->bezel, 0);
    lv_obj_set_style_border_width(g->root, th->bezel_width, 0);
    lv_obj_set_style_border_opa(g->root, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(g->root, true, 0);
    lv_obj_set_style_pad_all(g->root, 0, 0);
    lv_obj_remove_flag(g->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g->root, LV_SCROLLBAR_MODE_OFF);

    build_glow(g);
    build_scale(g);
    build_needle(g);
    build_hub(g);
    build_text(g);

    gauge_redraw_needle(g);
    g->slew = lv_timer_create(gauge_slew_cb, GAUGE_SLEW_PERIOD_MS, g);

    return g;
}

void gauge_delete(gauge_t *g)
{
    if (!g) {
        return;
    }
    if (g->slew) {
        lv_timer_delete(g->slew);
        g->slew = NULL;
    }
    if (g->cfg.alarm_from <= g->cfg.max && g->cfg.alarm_from >= g->cfg.min) {
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
/* public API                                                                 */
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
    gauge_redraw_needle(g);
}

float gauge_get_displayed_value(const gauge_t *g)
{
    return g ? g->displayed : 0.0f;
}

float gauge_get_target_value(const gauge_t *g)
{
    return g ? g->target : 0.0f;
}

lv_obj_t *gauge_get_obj(gauge_t *g)
{
    return g ? g->root : NULL;
}
