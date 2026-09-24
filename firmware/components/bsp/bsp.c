/*
 * bsp.c - Waveshare ESP32-S3-LCD-1.28 board support, no graphics library.
 *
 *   LCD   GC9A01A 240x240 round IPS, 4-wire SPI
 *   SCK   GPIO10      MOSI  GPIO11      CS   GPIO9
 *   DC    GPIO8       RST   GPIO12      BL   GPIO40
 *
 * The panel init sequence is taken verbatim from Waveshare's own
 * ESP32-S3-LCD-1.28-Test demo (LCD_1in28.cpp, LCD_1IN28_InitReg), not from
 * esp_lcd_gc9a01's built-in table.  The two differ in the gate-driver (GOA)
 * settings - esp_lcd uses 0x38 where Waveshare uses 0x18 for commands 0x62 and
 * 0x63, and omits 0xBD, 0xBC and 0x35 entirely.  Wrong GOA timing is a known
 * cause of whole bands of a panel staying dark, which is the fault this board
 * shows, so the vendor sequence is the one to trust.
 */
#include "bsp.h"

#include <string.h>

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "bsp"

#define LCD_HOST     ((spi_host_device_t)CONFIG_BSP_LCD_SPI_HOST)
#define LCD_PIN_SCK  CONFIG_BSP_LCD_PIN_SCK
#define LCD_PIN_MOSI CONFIG_BSP_LCD_PIN_MOSI
#define LCD_PIN_CS   CONFIG_BSP_LCD_PIN_CS
#define LCD_PIN_DC   CONFIG_BSP_LCD_PIN_DC
#define LCD_PIN_RST  CONFIG_BSP_LCD_PIN_RST
#define LCD_PIN_BL   CONFIG_BSP_LCD_PIN_BL

#define LCD_CLK_HZ   (CONFIG_BSP_LCD_SPI_CLK_MHZ * 1000 * 1000)

/* -------------------------------------------------------------------------- */
/* Waveshare's GC9A01A init sequence                                          */
/* -------------------------------------------------------------------------- */

static const gc9a01_lcd_init_cmd_t waveshare_init_cmds[] = {
    {0xEF, (uint8_t []){0x00}, 0, 0},
    {0xEB, (uint8_t []){0x14}, 1, 0},
    {0xFE, (uint8_t []){0x00}, 0, 0},
    {0xEF, (uint8_t []){0x00}, 0, 0},
    {0xEB, (uint8_t []){0x14}, 1, 0},
    {0x84, (uint8_t []){0x40}, 1, 0},
    {0x85, (uint8_t []){0xFF}, 1, 0},
    {0x86, (uint8_t []){0xFF}, 1, 0},
    {0x87, (uint8_t []){0xFF}, 1, 0},
    {0x88, (uint8_t []){0x0A}, 1, 0},
    {0x89, (uint8_t []){0x21}, 1, 0},
    {0x8A, (uint8_t []){0x00}, 1, 0},
    {0x8B, (uint8_t []){0x80}, 1, 0},
    {0x8C, (uint8_t []){0x01}, 1, 0},
    {0x8D, (uint8_t []){0x01}, 1, 0},
    {0x8E, (uint8_t []){0xFF}, 1, 0},
    {0x8F, (uint8_t []){0xFF}, 1, 0},

    {0xB6, (uint8_t []){0x00, 0x20}, 2, 0},

    /* Let the driver own MADCTL/COLMOD; 0x05 is the MCU-interface 16bpp value
     * Waveshare uses.  esp_lcd's default of 0x55 is the RGB-interface value. */
    {0x3A, (uint8_t []){0x05}, 1, 0},

    {0x90, (uint8_t []){0x08, 0x08, 0x08, 0x08}, 4, 0},

    {0xBD, (uint8_t []){0x06}, 1, 0},
    {0xBC, (uint8_t []){0x00}, 1, 0},

    {0xFF, (uint8_t []){0x60, 0x01, 0x04}, 3, 0},
    {0xC3, (uint8_t []){0x13}, 1, 0},
    {0xC4, (uint8_t []){0x13}, 1, 0},
    {0xC9, (uint8_t []){0x22}, 1, 0},
    {0xBE, (uint8_t []){0x11}, 1, 0},
    {0xE1, (uint8_t []){0x10, 0x0E}, 2, 0},
    {0xDF, (uint8_t []){0x21, 0x0C, 0x02}, 3, 0},

    /* gamma */
    {0xF0, (uint8_t []){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6, 0},
    {0xF1, (uint8_t []){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6, 0},
    {0xF2, (uint8_t []){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6, 0},
    {0xF3, (uint8_t []){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6, 0},

    {0xED, (uint8_t []){0x1B, 0x0B}, 2, 0},
    {0xAE, (uint8_t []){0x77}, 1, 0},
    {0xCD, (uint8_t []){0x63}, 1, 0},
    {0x70, (uint8_t []){0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 9, 0},
    {0xE8, (uint8_t []){0x34}, 1, 0},

    /* Gate driver (GOA) timing.  These are the bytes esp_lcd_gc9a01 gets
     * differently - 0x18 here against 0x38 there. */
    {0x62, (uint8_t []){0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12, 0},
    {0x63, (uint8_t []){0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12, 0},
    {0x64, (uint8_t []){0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 7, 0},
    {0x66, (uint8_t []){0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}, 10, 0},
    {0x67, (uint8_t []){0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}, 10, 0},
    {0x74, (uint8_t []){0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}, 7, 0},
    {0x98, (uint8_t []){0x3E, 0x07}, 2, 0},

    {0x35, (uint8_t []){0x00}, 0, 0},   /* tearing effect line on */
    {0x21, (uint8_t []){0x00}, 0, 0},   /* invert on (matches Waveshare) */
    {0x11, (uint8_t []){0x00}, 0, 120}, /* sleep out */
    {0x29, (uint8_t []){0x00}, 0, 20},  /* display on */
};

/* -------------------------------------------------------------------------- */

static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_flush_done;
static int s_backlight = -1;
static volatile uint32_t s_flush_count;

/* LVGL-less byte order fix: the panel wants the high byte of each RGB565
 * pixel first, our framebuffers are plain little-endian. */
static inline void swap_rgb565(uint16_t *px, size_t count)
{
#if CONFIG_BSP_LCD_SWAP_RGB565_BYTES
    for (size_t i = 0; i < count; i++) {
        px[i] = (uint16_t)((px[i] >> 8) | (px[i] << 8));
    }
#else
    (void)px;
    (void)count;
#endif
}

static bool on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                esp_lcd_panel_io_event_data_t *edata,
                                void *user_ctx)
{
    (void)io;
    (void)edata;
    (void)user_ctx;
    BaseType_t woken = pdFALSE;
    if (s_flush_done) {
        xSemaphoreGiveFromISR(s_flush_done, &woken);
    }
    return woken == pdTRUE;
}

esp_err_t bsp_lcd_draw_bitmap(int x0, int y0, int x1, int y1, const uint16_t *pixels)
{
    if (!s_panel || !pixels || x1 <= x0 || y1 <= y0) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t count = (size_t)(x1 - x0) * (size_t)(y1 - y0);
    swap_rgb565((uint16_t *)pixels, count);

    /* Drain a stale completion before starting a new transfer. */
    if (s_flush_done) {
        xSemaphoreTake(s_flush_done, 0);
    }

    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, x0, y0, x1, y1, pixels);
    if (err != ESP_OK) {
        return err;
    }

    if (s_flush_done && xSemaphoreTake(s_flush_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_flush_count++;
    return ESP_OK;
}

esp_err_t bsp_lcd_fill_rect(int x0, int y0, int x1, int y1, uint16_t colour)
{
    static uint16_t line[BSP_LCD_H_RES];

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > BSP_LCD_H_RES) x1 = BSP_LCD_H_RES;
    if (y1 > BSP_LCD_V_RES) y1 = BSP_LCD_V_RES;
    if (x1 <= x0 || y1 <= y0) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int x = x0; x < x1; x++) {
        line[x] = colour;
    }

    for (int y = y0; y < y1; y++) {
        esp_err_t err = bsp_lcd_draw_bitmap(x0, y, x1, y + 1, line + x0);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

uint32_t bsp_lcd_flush_count(void)
{
    return s_flush_count;
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
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "ledc timer");

    ledc_channel_config_t ch = {
        .gpio_num = LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch), TAG, "ledc channel");
    bsp_backlight_set(CONFIG_BSP_BACKLIGHT_DEFAULT_PERCENT);
#else
    s_backlight = 0;
#endif
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/* panel                                                                      */
/* -------------------------------------------------------------------------- */

esp_err_t bsp_display_init(void)
{
    if (s_panel) {
        return ESP_OK;
    }

    s_flush_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_flush_done, ESP_ERR_NO_MEM, TAG, "flush semaphore");

    spi_bus_config_t bus_cfg = {
        .sclk_io_num = LCD_PIN_SCK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO), TAG, "spi bus");

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_CLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io),
                        TAG, "panel io");

    esp_lcd_panel_io_callbacks_t cbs = { .on_color_trans_done = on_color_trans_done };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(io, &cbs, NULL), TAG, "io callbacks");

    static gc9a01_vendor_config_t vendor_cfg = {
        .init_cmds = waveshare_init_cmds,
        .init_cmds_size = sizeof(waveshare_init_cmds) / sizeof(waveshare_init_cmds[0]),
    };

    esp_lcd_panel_dev_config_t dev_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(io, &dev_cfg, &s_panel), TAG, "gc9a01");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");
    /*
     * Do NOT call esp_lcd_panel_invert_color() here.  Waveshare's sequence
     * ends with 0x21 (INVON) itself, and the driver's API sends 0x20 (INVOFF)
     * when passed false - which cancels the vendor sequence and leaves the
     * panel showing a photographic negative.
     */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, false, false), TAG, "mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "disp on");

    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "backlight");

    ESP_LOGI(TAG, "GC9A01A up: %dx%d @ %d MHz (Waveshare vendor init)",
             BSP_LCD_H_RES, BSP_LCD_V_RES, CONFIG_BSP_LCD_SPI_CLK_MHZ);
    return ESP_OK;
}

esp_lcd_panel_handle_t bsp_lcd_panel(void)
{
    return s_panel;
}
