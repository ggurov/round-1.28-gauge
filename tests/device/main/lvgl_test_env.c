#include "lvgl_test_env.h"

#include "esp_timer.h"

static lv_display_t *s_disp;

/* Two partial draw buffers, in internal RAM like the real firmware. */
#define BUF_LINES 40
static uint8_t s_buf1[TEST_DISPLAY_W * BUF_LINES * 2];
static uint8_t s_buf2[TEST_DISPLAY_W * BUF_LINES * 2];

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}

static uint32_t tick_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

lv_display_t *lvgl_test_env_init(void)
{
    if (s_disp) {
        return s_disp;
    }

    lv_init();
    lv_tick_set_cb(tick_cb);

    s_disp = lv_display_create(TEST_DISPLAY_W, TEST_DISPLAY_H);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_disp, flush_cb);
    lv_display_set_buffers(s_disp, s_buf1, s_buf2, sizeof(s_buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return s_disp;
}

lv_display_t *lvgl_test_env_display(void)
{
    return s_disp;
}

void lvgl_test_env_render(void)
{
    /* let queued timers fire (the gauge slew timer lives here) */
    for (int i = 0; i < 8; i++) {
        lv_timer_handler();
    }
    if (s_disp) {
        lv_refr_now(s_disp);
    }
}

lv_obj_t *lvgl_test_env_fresh_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (scr) {
        lv_obj_clean(scr);
    }
    return scr;
}

long lvgl_test_env_mem_used(void)
{
    lv_mem_monitor_t mon;
    if (lv_mem_monitor(&mon) != LV_RESULT_OK) {
        return -1;
    }
    return (long)mon.total_size - (long)mon.free_size;
}
