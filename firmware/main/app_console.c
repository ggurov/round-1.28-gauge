/*
 * app_console.c - interactive console on the CH343P UART (UART0, 115200).
 *
 * This is the board's lifeline.  The Waveshare ESP32-S3-LCD-1.28 has no
 * software path into the ROM bootloader (DTR/RTS are not wired to the boot
 * strap), so without the `bootloader` command every reflash would need a
 * physical BOOT + RESET press.  With it, reflashing is:
 *
 *     idf.py -p COM6 flash monitor
 *
 * after the board has been told to drop into download mode.
 */
#include "app_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gauge.h"
#include "gauge_demo.h"
#include "gauge_presets.h"
#include "soc/rtc_cntl_reg.h"

static const char *TAG = "console";

/* LVGL has no getter for the refresher period, so remember it here. */
static uint32_t s_refr_period_ms = LV_DEF_REFR_PERIOD;

/* -------------------------------------------------------------------------- */
/* commands                                                                   */
/* -------------------------------------------------------------------------- */

void app_reboot_to_bootloader(void)
{
    printf("\nRebooting into ROM download mode...\n");
    printf("The board will sit in the bootloader until you flash it.\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(50));

#if defined(RTC_CNTL_FORCE_DOWNLOAD_BOOT) && defined(RTC_CNTL_OPTION1_REG)
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_restart();
#else
    printf("This target has no force-download-boot bit; restarting normally.\n");
    esp_restart();
#endif
}

static int cmd_bootloader(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    app_reboot_to_bootloader();
    return 0; /* not reached */
}

static int cmd_reset(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("Restarting...\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_restart();
    return 0;
}

static int cmd_gauge(int argc, char **argv)
{
    if (argc < 2) {
        const gauge_preset_t *cur = gauge_demo_current();
        printf("Available gauges:\n");
        for (const gauge_preset_t *p = gauge_presets_all(); p->id; p++) {
            printf("  %-6s %s%s\n", p->id, p->name,
                   (cur && strcmp(cur->id, p->id) == 0) ? "   <- active" : "");
        }
        printf("Usage: gauge <id>\n");
        return 0;
    }

    const gauge_preset_t *p = gauge_preset_find(argv[1]);
    if (!p) {
        printf("Unknown gauge '%s'. Run `gauge` for the list.\n", argv[1]);
        return 1;
    }
    gauge_demo_select(p);
    printf("Gauge -> %s\n", p->name);
    return 0;
}

static int cmd_demo(int argc, char **argv)
{
    if (argc < 2) {
        printf("Simulator is %s\n", gauge_demo_is_running() ? "ON" : "OFF");
        return 0;
    }
    if (strcmp(argv[1], "on") == 0) {
        gauge_demo_set_running(true);
        printf("Simulator on\n");
    } else if (strcmp(argv[1], "off") == 0) {
        gauge_demo_set_running(false);
        printf("Simulator off - use `value <n>`\n");
    } else if (strcmp(argv[1], "sweep") == 0) {
        gauge_demo_run_selftest();
        printf("Self-test sweep\n");
    } else {
        printf("Usage: demo [on|off|sweep]\n");
        return 1;
    }
    return 0;
}

static int cmd_value(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: value <number>\n");
        return 1;
    }
    float v = strtof(argv[1], NULL);
    gauge_demo_set_value(v);
    printf("Value -> %.2f (simulator off)\n", (double)v);
    return 0;
}

static int cmd_backlight(int argc, char **argv)
{
    if (argc < 2) {
        printf("Backlight %d%%\n", bsp_backlight_get());
        return 0;
    }
    int pct = atoi(argv[1]);
    bsp_backlight_set(pct);
    printf("Backlight -> %d%%\n", bsp_backlight_get());
    return 0;
}

static int cmd_redraw(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!bsp_lvgl_lock(1000)) {
        printf("could not take the LVGL lock\n");
        return 1;
    }
    lv_obj_invalidate(lv_screen_active());
    bsp_lvgl_unlock();
    printf("Full redraw requested.\n");
    return 0;
}

static int cmd_fps(int argc, char **argv)
{
    lv_display_t *disp = bsp_display_get();
    if (!disp) {
        printf("no display\n");
        return 1;
    }
    lv_timer_t *refr = lv_display_get_refr_timer(disp);
    if (!refr) {
        printf("no refresh timer\n");
        return 1;
    }

    if (argc < 2) {
        printf("display refresh: %u ms  (~%u fps)\n", (unsigned)s_refr_period_ms,
               s_refr_period_ms ? (unsigned)(1000 / s_refr_period_ms) : 0);
        printf("render mode: %s, %u bytes per draw buffer\n",
               bsp_render_mode(), (unsigned)bsp_draw_buffer_bytes());
        return 0;
    }

    int hz = atoi(argv[1]);
    if (hz < 1) {
        printf("usage: fps <hz>   (1..200)\n");
        return 1;
    }
    if (hz > 200) {
        hz = 200;
    }
    uint32_t period = 1000u / (uint32_t)hz;
    if (period < 1) {
        period = 1;
    }

    if (!bsp_lvgl_lock(1000)) {
        printf("could not take the LVGL lock\n");
        return 1;
    }
    s_refr_period_ms = period;
    lv_timer_set_period(refr, period);
    /* repaint everything, so the effect of the change is visible immediately */
    lv_obj_invalidate(lv_screen_active());
    bsp_lvgl_unlock();

    printf("display refresh set to %u ms (~%d fps)\n", (unsigned)period, hz);
    return 0;
}

static int cmd_flush(int argc, char **argv)
{
    if (argc >= 2) {
        if (strcmp(argv[1], "sync") == 0) {
            bsp_flush_set_sync(true);
        } else if (strcmp(argv[1], "async") == 0) {
            bsp_flush_set_sync(false);
        } else if (strcmp(argv[1], "reset") == 0) {
            /* nothing to reset; the counters are informative only */
        } else {
            printf("usage: flush [sync|async|stats]\n");
            return 1;
        }
    }

    uint32_t count = 0, errors = 0, timeouts = 0;
    bsp_flush_get_stats(&count, &errors, &timeouts);
    printf("flush mode : %s\n", bsp_flush_get_sync() ? "synchronous" : "asynchronous");
    printf("flushes    : %u\n", (unsigned)count);
    printf("driver errs: %u\n", (unsigned)errors);
    printf("timeouts   : %u\n", (unsigned)timeouts);
    return 0;
}

static int cmd_testpattern(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!bsp_lvgl_lock(2000)) {
        printf("could not take the LVGL lock\n");
        return 1;
    }

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);   /* drops the gauge as well; `gauge rpm` brings it back */

    static const uint32_t colours[4] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *o = lv_obj_create(scr);
        lv_obj_remove_style_all(o);
        lv_obj_set_size(o, 120, 120);
        lv_obj_set_pos(o, (i % 2) * 120, (i / 2) * 120);
        lv_obj_set_style_bg_color(o, lv_color_hex(colours[i]), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    }
    /* plus a one-pixel white frame, so the panel's edges are identifiable */
    lv_obj_t *frame = lv_obj_create(scr);
    lv_obj_remove_style_all(frame);
    lv_obj_set_size(frame, 240, 240);
    lv_obj_set_pos(frame, 0, 0);
    lv_obj_set_style_border_color(frame, lv_color_white(), 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_border_opa(frame, LV_OPA_COVER, 0);

    bsp_lvgl_unlock();
    printf("Test pattern: red/green top, blue/yellow bottom, white frame.\n");
    printf("`gauge rpm` restores the dial.\n");
    return 0;
}

static int cmd_free(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("Internal heap : %u B free / %u B total (min ever %u B)\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    printf("Largest block : %u B\n",
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    printf("PSRAM         : %u B free\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return 0;
}

static int cmd_version(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("round-1.28-gauge  |  ESP-IDF %s  |  LVGL %d.%d.%d\n",
           esp_get_idf_version(), LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    printf("Target: %s   Cores: %d   CPU: %d MHz (configured)\n",
           CONFIG_IDF_TARGET, portNUM_PROCESSORS, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    return 0;
}

/* -------------------------------------------------------------------------- */

esp_err_t app_console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "gauge>";
    repl_cfg.max_cmdline_length = 128;
    repl_cfg.task_stack_size = 4096;

    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    esp_err_t err = esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl);
    if (err != ESP_OK) {
        return err;
    }

    static const esp_console_cmd_t cmds[] = {
        { .command = "bootloader", .help = "Reboot into ROM download mode (for idf.py flash)",
          .func = &cmd_bootloader },
        { .command = "reset", .help = "Restart the application", .func = &cmd_reset },
        { .command = "gauge", .help = "List or select a gauge: gauge [id]", .func = &cmd_gauge },
        { .command = "demo", .help = "Simulator: demo [on|off|sweep]", .func = &cmd_demo },
        { .command = "value", .help = "Drive the needle: value <number>", .func = &cmd_value },
        { .command = "backlight", .help = "Backlight: backlight [0-100]", .func = &cmd_backlight },
        { .command = "redraw", .help = "Force a full screen repaint", .func = &cmd_redraw },
        { .command = "fps", .help = "Show or set display refresh: fps [hz]", .func = &cmd_fps },
        { .command = "flush", .help = "Flush mode/stats: flush [sync|async|stats]", .func = &cmd_flush },
        { .command = "test", .help = "Draw a quadrant test pattern", .func = &cmd_testpattern },
        { .command = "free", .help = "Show heap usage", .func = &cmd_free },
        { .command = "version", .help = "Show build information", .func = &cmd_version },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
    ESP_ERROR_CHECK(esp_console_register_help_command());

    err = esp_console_start_repl(repl);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "console ready on UART0 @ 115200 (try `help`)");
    }
    return err;
}
