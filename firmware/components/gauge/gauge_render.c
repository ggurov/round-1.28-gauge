/*
 * gauge_render.c - draws the dial with the gfx primitives.
 *
 * No graphics library, no cached framebuffer: every frame is drawn from
 * scratch into the shared surface.  The geometry comes from gauge_math so the
 * host tests keep covering it.
 */
#include "gauge_render.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gauge_math.h"
#include "gauge_theme.h"
#include "gfx.h"
#include "gfx_font_data.h"
#include "gfx_text.h"

#define CX (GFX_W / 2)
#define CY (GFX_H / 2)

struct gauge_render {
    gauge_config_t cfg;
    gauge_scale_t  scale;

    /* resolved geometry */
    int r_rail;        /* tick base radius */
    int r_band_out;
    int r_band_in;
    int needle_len;
    int label_radius;
    int pad_radial;
    int tick_major_len;
    int tick_minor_len;
    int hub_radius;

    /* value */
    float target;
    float displayed;
    float slew_alpha;
    float max_step;
    float epsilon;
    char  value_text[GAUGE_LABEL_LEN];

    /* label storage for lv_scale-style auto numerals */
    char label_buf[GAUGE_MAX_TICKS][GAUGE_LABEL_LEN];
};

/* -------------------------------------------------------------------------- */
/* helpers                                                                    */
/* -------------------------------------------------------------------------- */

/* Angles inside the renderer are gfx angles: degrees clockwise from 3 o'clock.
 * gauge_math works from 12 o'clock, hence the -90. */
static float scale_angle(const gauge_render_t *g, float value)
{
    return gauge_math_value_to_angle(&g->scale, value) - 90.0f;
}

static void polar(float cx, float cy, float r, float deg, int *x, int *y)
{
    const float rad = deg * (float)M_PI / 180.0f;
    *x = (int)lroundf(cx + r * cosf(rad));
    *y = (int)lroundf(cy + r * sinf(rad));
}

static uint16_t font_line(const gfx_font_t *f)
{
    return (uint16_t)f->line_height;
}

/* -------------------------------------------------------------------------- */
/* drawing                                                                    */
/* -------------------------------------------------------------------------- */

static void draw_band(gauge_render_t *g)
{
    const gauge_theme_t *th = g->cfg.theme;
    const int a0 = (int)scale_angle(g, g->cfg.min);
    const int a1 = (int)scale_angle(g, g->cfg.max);

    /* glow: a wider, dimmer band just outside and inside the rail */
    const uint16_t glow = gfx_blend(gfx_hex(th->face), gfx_hex(th->band_glow), 90);
    gfx_arc_band(CX, CY, g->r_band_out + th->band_width, g->r_band_in - th->band_width,
                 a0, a1, glow);

    gfx_arc_band(CX, CY, g->r_band_out, g->r_band_in, a0, a1, gfx_hex(th->band));

    if (g->cfg.alarm_from <= g->cfg.max) {
        const int aa0 = (int)scale_angle(g, g->cfg.alarm_from);
        gfx_arc_band(CX, CY, g->r_band_out, g->r_band_in, aa0, a1, gfx_hex(th->alarm));
    }
}

static void draw_ticks(gauge_render_t *g)
{
    const gauge_theme_t *th = g->cfg.theme;
    const int ticks = gauge_math_total_ticks(&g->scale);
    const int minor = g->cfg.minor_per_major;
    const float alarm_angle = (g->cfg.alarm_from <= g->cfg.max)
                                  ? scale_angle(g, g->cfg.alarm_from)
                                  : 1e9f;

    for (int i = 0; i < ticks; i++) {
        const float frac = (ticks > 1) ? (float)i / (float)(ticks - 1) : 0.0f;
        const float deg = (float)g->cfg.rotation + frac * (float)g->cfg.angle_range;
        const bool major = (minor > 0) && (i % minor == 0);
        const int len = major ? g->tick_major_len : g->tick_minor_len;
        const int width = major ? th->tick_major_width : th->tick_minor_width;
        uint16_t colour = major ? gfx_hex(th->tick_major) : gfx_hex(th->tick_minor);

        if (major && deg >= alarm_angle) {
            colour = gfx_hex(th->alarm);
        }

        int x0, y0, x1, y1;
        polar((float)CX, (float)CY, (float)g->r_rail, deg, &x0, &y0);
        polar((float)CX, (float)CY, (float)(g->r_rail - len), deg, &x1, &y1);
        if (width <= 1) {
            gfx_line(x0, y0, x1, y1, colour);
        } else {
            gfx_thick_line(x0, y0, x1, y1, width, colour);
        }
    }
}

static void draw_numerals(gauge_render_t *g)
{
    const gauge_theme_t *th = g->cfg.theme;
    const int majors = gauge_math_major_ticks(&g->scale);
    const uint16_t colour = gfx_hex(th->label);
    const uint16_t face = gfx_hex(th->face);

    for (int i = 0; i < majors; i++) {
        if (g->cfg.tick_labels) {
            strncpy(g->label_buf[i], g->cfg.tick_labels[i], GAUGE_LABEL_LEN - 1);
            g->label_buf[i][GAUGE_LABEL_LEN - 1] = '\0';
        } else {
            gauge_math_major_label(&g->scale, i, g->label_buf[i], GAUGE_LABEL_LEN);
        }

        const float frac = (majors > 1) ? (float)i / (float)(majors - 1) : 0.0f;
        const float deg = (float)g->cfg.rotation + frac * (float)g->cfg.angle_range;

        int x, y;
        polar((float)CX, (float)CY, (float)g->label_radius, deg, &x, &y);

        const int w = gfx_text_width(g->label_buf[i], &gfx_font_label);
        const int h = (int)font_line(&gfx_font_label);
        /* Punch a hole in the face first: without it the numeral overlaps the
         * ticks and the band on a busy dial. */
        gfx_fill_rect(x - w / 2 - 1, y - h / 2, x + w / 2 + 1, y + h / 2, face);
        gfx_text_centered(x, y - h / 2, g->label_buf[i], &gfx_font_label, colour);
    }
}

static void draw_needle(gauge_render_t *g)
{
    const gauge_theme_t *th = g->cfg.theme;
    const float deg = scale_angle(g, g->displayed);
    const float rad = deg * (float)M_PI / 180.0f;
    const float ux = cosf(rad), uy = sinf(rad);      /* along the needle */
    const float vx = -uy, vy = ux;                   /* perpendicular     */

    const float L = (float)g->needle_len;
    const float hb = 5.0f;     /* half width at the hub */
    const float ht = 1.2f;     /* half width at the tip */
    const float tail = 13.0f;
    const float htail = 4.0f;

    const float pts[6][2] = {
        {-hb, 0.0f}, {-ht, L}, {ht, L}, {hb, 0.0f}, {htail, -tail}, {-htail, -tail},
    };

    int px[6], py[6];
    for (int i = 0; i < 6; i++) {
        px[i] = CX + (int)lroundf(pts[i][0] * vx + pts[i][1] * ux);
        py[i] = CY + (int)lroundf(pts[i][0] * vy + pts[i][1] * uy);
    }
    gfx_fill_polygon(px, py, 6, gfx_hex(th->needle));
}

static void draw_face_and_bezel(gauge_render_t *g)
{
    const gauge_theme_t *th = g->cfg.theme;

    gfx_clear(gfx_hex(th->face));
    /* silver bezel: a ring at the very edge, and a darker line inside it so the
     * dial reads as recessed */
    gfx_ring(CX, CY, 119, 119 - th->bezel_width, gfx_hex(th->bezel));
    gfx_circle(CX, CY, 119 - th->bezel_width - 1, gfx_hex(th->needle_hub_ring));
}

static void draw_hub(gauge_render_t *g)
{
    const gauge_theme_t *th = g->cfg.theme;
    gfx_disc(CX, CY, g->hub_radius, gfx_hex(th->needle_hub));
    gfx_circle_thick(CX, CY, g->hub_radius, 2, gfx_hex(th->needle_hub_ring));
}

static void draw_text(gauge_render_t *g)
{
    const gauge_config_t *c = &g->cfg;
    const gauge_theme_t *th = c->theme;
    const int hub = g->hub_radius;

    /*
     * Centre stack, measured from the middle of the dial.  These offsets are
     * deliberately tight: everything below the hub has to fit inside the ring
     * of numerals, and the available width shrinks as the radius grows.  At
     * +36 the value box still clears the numerals; at +60 it did not and the
     * read-out ran into the "8" and the tick band.
     */
    const int y_word = CY - (hub + 26);
    const int y_tag = CY - (hub + 10);
    const int y_cap = CY + (hub + 14);
    const int y_val = CY + (hub + 36);
    const int y_unit = CY + (hub + 58);

    if (c->wordmark) {
        gfx_text_centered(CX, y_word - gfx_font_small.line_height / 2, c->wordmark,
                          &gfx_font_small, gfx_hex(th->wordmark));
    }
    if (c->tagline) {
        gfx_text_centered(CX, y_tag - gfx_font_small.line_height / 2, c->tagline,
                          &gfx_font_small, gfx_hex(th->tagline));
    }
    if (c->caption) {
        gfx_text_centered(CX, y_cap - gfx_font_small.line_height / 2, c->caption,
                          &gfx_font_small, gfx_hex(th->caption));
    }

    gauge_math_format(&g->scale, g->displayed, g->value_text, sizeof(g->value_text));
    gfx_text_centered(CX, y_val - gfx_font_value.line_height / 2, g->value_text,
                      &gfx_font_value, gfx_hex(th->value));

    if (c->unit) {
        gfx_text_centered(CX, y_unit - gfx_font_small.line_height / 2, c->unit,
                          &gfx_font_small, gfx_hex(th->unit));
    }
}

/* -------------------------------------------------------------------------- */

void gauge_render_draw(gauge_render_t *g)
{
    if (!g) {
        return;
    }
    draw_face_and_bezel(g);
    draw_band(g);
    draw_ticks(g);
    draw_numerals(g);
    draw_needle(g);
    draw_hub(g);
    draw_text(g);
}

void gauge_render_tick(gauge_render_t *g, float dt)
{
    if (!g) {
        return;
    }
    if (g->displayed != g->target) {
        const float alpha = gauge_math_slew_alpha(dt, g->cfg.slew_time > 0 ? g->cfg.slew_time : 0.35f);
        const float range = g->cfg.max - g->cfg.min;
        const float max_step = gauge_math_slew_max_step(range > 0 ? range : 1.0f, dt,
                                                        g->cfg.slew_time > 0 ? g->cfg.slew_time : 0.35f);
        g->displayed = gauge_math_slew_step(g->displayed, g->target, alpha, max_step,
                                            g->epsilon);
    }
    gauge_render_draw(g);
}

bool gauge_render_moving(const gauge_render_t *g)
{
    return g && (g->displayed != g->target);
}

/* -------------------------------------------------------------------------- */

gauge_render_t *gauge_render_create(const gauge_config_t *cfg)
{
    if (!cfg || !cfg->theme) {
        return NULL;
    }

    gauge_render_t *g = calloc(1, sizeof(*g));
    if (!g) {
        return NULL;
    }
    g->cfg = *cfg;
    gauge_math_from_config(&g->cfg, &g->scale);
    g->cfg.angle_range = g->scale.angle_range;
    g->cfg.rotation = g->scale.rotation;
    g->cfg.minor_per_major = g->scale.minor_per_major;

    const gauge_theme_t *th = g->cfg.theme;

    g->tick_major_len = th->tick_major_len;
    g->tick_minor_len = th->tick_minor_len;
    g->hub_radius = th->hub_radius;

    g->r_rail = gauge_math_rail_radius(GFX_W, th->bezel_width, th->band_gap, th->band_width);
    g->r_band_out = g->r_rail;
    g->r_band_in = g->r_rail - th->band_width;

    g->needle_len = (cfg->needle_length > 0.0f)
                        ? (int)lroundf(cfg->needle_length)
                        : gauge_math_needle_length(g->r_rail, g->tick_major_len);

    const int glyph_h = gfx_font_label.line_height;
    g->label_radius = (cfg->label_radius > 0.0f)
                          ? (int)lroundf(cfg->label_radius)
                          : (g->r_rail - g->tick_major_len - 4 - glyph_h / 2);
    g->pad_radial = gauge_math_pad_radial_for(g->r_rail, g->tick_major_len,
                                              g->label_radius, th->label_letter_space);
    (void)g->pad_radial;

    const float range = (g->cfg.max > g->cfg.min) ? (g->cfg.max - g->cfg.min) : 1.0f;
    g->epsilon = range * 1e-4f;
    g->target = g->cfg.min;
    g->displayed = g->cfg.min;

    return g;
}

void gauge_render_destroy(gauge_render_t *g)
{
    free(g);
}

void gauge_render_set_value(gauge_render_t *g, float value)
{
    if (!g) {
        return;
    }
    if (value < g->cfg.min) value = g->cfg.min;
    if (value > g->cfg.max) value = g->cfg.max;
    g->target = value;
}

void gauge_render_set_immediate(gauge_render_t *g, float value)
{
    if (!g) {
        return;
    }
    if (value < g->cfg.min) value = g->cfg.min;
    if (value > g->cfg.max) value = g->cfg.max;
    g->target = value;
    g->displayed = value;
}

float gauge_render_displayed(const gauge_render_t *g)
{
    return g ? g->displayed : 0.0f;
}

float gauge_render_target(const gauge_render_t *g)
{
    return g ? g->target : 0.0f;
}

const gauge_config_t *gauge_render_config(const gauge_render_t *g)
{
    return g ? &g->cfg : NULL;
}
