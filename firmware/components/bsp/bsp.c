/*
 * bsp.c - Waveshare ESP32-S3-LCD-1.28 board support.
 *
 *   LCD   GC9A01A 240x240 round IPS, 4-wire SPI
 *   SCK   GPIO10
 *   MOSI  GPIO11
 *   CS    GPIO9
 *   DC    GPIO8
 *   RST   GPIO12
 *   BL    GPIO40   (LEDC PWM)
 *
 * Everything here is deliberately defensive: a failure to bring the panel up
 * logs, unwinds and returns, leaving the UART console alive so the board can
 * always be recovered without touching the BOOT button.
 */
#include "bsp.h"

#include <string.h>

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define TAG "bsp"

#define LCD_H_RES   CONFIG_BSP_LCD_H_RES
#define LCD_V_RES   CONFIG_BSP_LCD_V_RES
#define LCD_HOST    ((spi_host_device_t)CONFIG_BSP_LCD_SPI_HOST)

#define LCD_PIN_SCK  CONFIG_BSP_LCD_PIN_SCK
#define LCD_PIN_MOSI CONFIG_BSP_LCD_PIN_MOSI
#define LCD_PIN_CS   CONFIG_BSP_LCD_PIN_CS
#define LCD_PIN_DC   CONFIG_BSP_LCD_PIN_DC
#define LCD_PIN_RST  CONFIG_BSP_LCD_PIN_RST
#define LCD_PIN_BL   CONFIG_BSP_LCD_PIN_BL

#define LCD_CLK_HZ   (CONFIG_BSP_LCD_SPI_CLK_MHZ * 1000 * 1000)
#define LCD_BUF_LINES CONFIG_BSP_LCD_BUFFER_LINES

/* ESP-IDF does not emit CONFIG_* for boolean options left at their default of
 * n, so normalise them here rather than testing with #ifdef at each use. */
#ifdef CONFIG_BSP_LCD_SWAP_XY
#define LCD_SWAP_XY  true
#else
#define LCD_SWAP_XY  false
#endif
#ifdef CONFIG_BSP_LCD_MIRROR_X
#define LCD_MIRROR_X true
#else
#define LCD_MIRROR_X false
#endif
#ifdef CONFIG_BSP_LCD_MIRROR_Y
#define LCD_MIRROR_Y true
#else
#define LCD_MIRROR_Y false
#endif

#define LVGL_TASK_STACK   8192
#define LVGL_TASK_PRIO    4
#define LVGL_TASK_AFFINITY 1
#define LVGL_TICK_PERIOD_US 2000

static const char *const k_tag = TAG;

static lv_display_t *s_disp;
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_lvgl_mutex;
static esp_timer_handle_t s_tick_timer;
static int s_backlight = -1;

/* Flush diagnostics.  `flush stats` on the console prints these. */
static volatile uint32_t s_flush_count;
static volatile uint32_t s_flush_errors;
static volatile uint32_t s_flush_timeouts;
static volatile bool     s_flush_sync = true;
static SemaphoreHandle_t s_flush_done;

void bsp_flush_set_sync(bool sync);
bool bsp_flush_get_sync(void);
void bsp_flush_get_stats(uint32_t *count, uint32_t *errors, uint32_t *timeouts);

void bsp_flush_set_sync(bool sync)
{
    s_flush_sync = sync;
}

bool bsp_flush_get_sync(void)
{
    return s_flush_sync;
}

void bsp_flush_get_stats(uint32_t *count, uint32_t *errors, uint32_t *timeouts)
{
    if (count)    *count = s_flush_count;
    if (errors)   *errors = s_flush_errors;
    if (timeouts) *timeouts = s_flush_timeouts;
}

#ifdef CONFIG_BSP_LCD_RENDER_FULL
const char *bsp_render_mode(void)      { return "full"; }
uint32_t bsp_draw_buffer_bytes(void)   { return LCD_H_RES * LCD_V_RES * sizeof(uint16_t); }
#else
const char *bsp_render_mode(void)      { return "partial"; }
uint32_t bsp_draw_buffer_bytes(void)   { return LCD_H_RES * LCD_BUF_LINES * sizeof(uint16_t); }
#endif

/* -------------------------------------------------------------------------- */
/* byte order                                                                 */
/* -------------------------------------------------------------------------- */

/* LVGL hands us little-endian RGB565; the GC9A01A wants the high byte first. */
static inline void swap_rgb565_bytes(uint8_t *buf, size_t pixel_count)
{
#if CONFIG_BSP_LCD_SWAP_RGB565_BYTES
    for (size_t i = 0; i < pixel_count; i++) {
        uint8_t tmp = buf[0];
        buf[0] = buf[1];
        buf[1] = tmp;
        buf += 2;
    }
#else
    (void)buf;
    (void)pixel_count;
#endif
}

/* -------------------------------------------------------------------------- */
/* LVGL plumbing                                                              */
/* -------------------------------------------------------------------------- */

static bool lcd_color_trans_done_cb(esp_lcd_panel_io_handle_t io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    (void)io;
    (void)edata;
    lv_display_t *disp = (lv_display_t *)user_ctx;
    if (s_flush_sync) {
        /* Hand back to lcd_flush_cb, which owns the flush_ready() call. */
        BaseType_t woken = pdFALSE;
        if (s_flush_done) {
            xSemaphoreGiveFromISR(s_flush_done, &woken);
        }
        return woken == pdTRUE;
    }
    if (disp) {
        lv_display_flush_ready(disp);
    }
    return false;
}

static void lcd_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    if (!panel) {
        lv_display_flush_ready(disp);
        return;
    }

    const int w = area->x2 - area->x1 + 1;
    const int h = area->y2 - area->y1 + 1;

    s_flush_count++;

    swap_rgb565_bytes(px_map, (size_t)w * (size_t)h);

    if (s_flush_sync) {
        /* Drain any previous transfer before touching the buffer again. */
        if (s_flush_done) {
            xSemaphoreTake(s_flush_done, 0);
        }
    }

    if (esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map) != ESP_OK) {
        s_flush_errors++;
        lv_display_flush_ready(disp);
        return;
    }

    if (s_flush_sync) {
        /*
         * Complete the transfer before telling LVGL the buffer is free.  The
         * async path hands this to the ISR, which lets LVGL start rendering
         * the next area while the DMA is still reading this one; if anything
         * in that handshake slips, whole tiles are lost and only come back
         * when the needle sweeps over them again.
         */
        if (s_flush_done && xSemaphoreTake(s_flush_done, pdMS_TO_TICKS(500)) != pdTRUE) {
            s_flush_timeouts++;
        }
    }

    lv_display_flush_ready(disp);
}

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_US / 1000);
}

/*
 * This board loses display tiles: an area is painted correctly, then goes black
 * a fraction of a second later and stays black until something invalidates it
 * again.  Repainting the whole screen periodically makes the dial heal itself.
 */
static void heal_timer_cb(lv_timer_t *t)
{
    (void)t;
    lv_obj_t *scr = lv_screen_active();
    if (scr) {
        lv_obj_invalidate(scr);
    }
}

static void lvgl_task(void *arg)
{
    (void)arg;
    uint32_t delay_ms = 5;
    for (;;) {
        if (bsp_lvgl_lock(-1)) {
            uint32_t busy = lv_timer_handler();
            bsp_lvgl_unlock();
            delay_ms = busy < 5 ? busy : 5;
            if (delay_ms < 1) {
                delay_ms = 1;
            }
        } else {
            delay_ms = 5;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

bool bsp_lvgl_lock(int timeout_ms)
{
    if (!s_lvgl_mutex) {
        return false;
    }
    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mutex, ticks) == pdTRUE;
}

void bsp_lvgl_unlock(void)
{
    if (s_lvgl_mutex) {
        xSemaphoreGiveRecursive(s_lvgl_mutex);
    }
}

/* -------------------------------------------------------------------------- */
/* backlight                                                                  */
/* -------------------------------------------------------------------------- */

void bsp_backlight_set(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    s_backlight = percent;
#if LCD_PIN_BL >= 0
    uint32_t duty = (uint32_t)((1023 * percent) / 100);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
#endif
}

int bsp_backlight_get(void)
{
    return s_backlight;
}

static esp_err_t backlight_init(void)
{
#if LCD_PIN_BL >= 0
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), k_tag, "ledc timer");

    ledc_channel_config_t ch = {
        .gpio_num = LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch), k_tag, "ledc channel");
    bsp_backlight_set(CONFIG_BSP_BACKLIGHT_DEFAULT_PERCENT);
#else
    s_backlight = 0;
#endif
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/* panel                                                                      */
/* -------------------------------------------------------------------------- */

static esp_err_t panel_init(void)
{
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = LCD_PIN_SCK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO), k_tag, "spi bus");

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_CLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io),
                        k_tag, "panel io");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(
                            io,
                            &(esp_lcd_panel_io_callbacks_t){ .on_color_trans_done = lcd_color_trans_done_cb },
                            s_disp),
                        k_tag, "io callbacks");

    esp_lcd_panel_dev_config_t dev_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(io, &dev_cfg, &s_panel), k_tag, "gc9a01");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), k_tag, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), k_tag, "panel init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, CONFIG_BSP_LCD_INVERT_COLOR), k_tag, "invert");
    if (LCD_SWAP_XY) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true), k_tag, "swap_xy");
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, LCD_MIRROR_X, LCD_MIRROR_Y), k_tag, "mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), k_tag, "disp on");

    ESP_LOGI(k_tag, "GC9A01A up: %dx%d @ %d MHz", LCD_H_RES, LCD_V_RES, CONFIG_BSP_LCD_SPI_CLK_MHZ);
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/* init                                                                       */
/* -------------------------------------------------------------------------- */

esp_err_t bsp_display_init(void)
{
    if (s_disp) {
        return ESP_OK;
    }

    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    ESP_RETURN_ON_FALSE(s_lvgl_mutex, ESP_ERR_NO_MEM, k_tag, "lvgl mutex");

    s_flush_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_flush_done, ESP_ERR_NO_MEM, k_tag, "flush semaphore");

    lv_init();

    s_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    ESP_RETURN_ON_FALSE(s_disp, ESP_ERR_NO_MEM, k_tag, "lv_display_create");
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);

    const size_t buf_bytes = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
#ifdef CONFIG_BSP_LCD_RENDER_FULL
    /*
     * One full-screen buffer, rendered and pushed as a single rectangle.  See
     * the Kconfig help: the partial path loses tiles on this board.
     */
    void *buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(buf1, ESP_ERR_NO_MEM, k_tag, "draw buffer (%u B)", (unsigned)buf_bytes);
    lv_display_set_buffers(s_disp, buf1, NULL, buf_bytes, LV_DISPLAY_RENDER_MODE_FULL);
    ESP_LOGI(k_tag, "render mode: FULL (%u B single buffer)", (unsigned)buf_bytes);
#else
    const size_t part_bytes = LCD_H_RES * LCD_BUF_LINES * sizeof(uint16_t);
    void *buf1 = heap_caps_malloc(part_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *buf2 = heap_caps_malloc(part_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(buf1 && buf2, ESP_ERR_NO_MEM, k_tag, "draw buffers (%u B each)", (unsigned)part_bytes);
    lv_display_set_buffers(s_disp, buf1, buf2, part_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    ESP_LOGI(k_tag, "render mode: PARTIAL (%u B x2 buffers)", (unsigned)part_bytes);
#endif
    lv_display_set_flush_cb(s_disp, lcd_flush_cb);

    esp_err_t err = panel_init();
    if (err != ESP_OK) {
        return err;
    }
    lv_display_set_user_data(s_disp, s_panel);

    ESP_RETURN_ON_ERROR(backlight_init(), k_tag, "backlight");

    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &s_tick_timer), k_tag, "tick timer");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(s_tick_timer, LVGL_TICK_PERIOD_US), k_tag, "tick start");

    BaseType_t ok = xTaskCreatePinnedToCore(lvgl_task, "lvgl", LVGL_TASK_STACK, NULL,
                                            LVGL_TASK_PRIO, NULL, LVGL_TASK_AFFINITY);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, k_tag, "lvgl task");

    /* Paint the screen black straight away so a failed app still looks tidy. */
    if (bsp_lvgl_lock(0)) {
        lv_obj_t *scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
#if CONFIG_BSP_LCD_HEAL_PERIOD_MS > 0
        lv_timer_create(heal_timer_cb, CONFIG_BSP_LCD_HEAL_PERIOD_MS, NULL);
#endif
        bsp_lvgl_unlock();
    }

    ESP_LOGI(k_tag, "display ready (%u B draw buffers, LVGL %d.%d.%d)",
             (unsigned)buf_bytes, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    return ESP_OK;
}

lv_display_t *bsp_display_get(void)
{
    return s_disp;
}
