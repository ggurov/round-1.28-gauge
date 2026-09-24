/*
 * app_console.c - interactive console on the CH343P UART (UART0, 115200).
 *
 * This is the board's lifeline.  The Waveshare ESP32-S3-LCD-1.28 has no
 * software path into the ROM bootloader (DTR is not wired to the boot strap),
 * so without the `bootloader` command every reflash would need a physical
 * BOOT + RESET press.  With it, reflashing is completely hands-free.
 */
#include "app_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_tests.h"
#include "bsp.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx.h"
#include "soc/rtc_cntl_reg.h"

static const char *TAG = "console";

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
    return 0;   /* not reached */
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

static int cmd_test(int argc, char **argv)
{
    if (argc < 2) {
        printf("Test screens: fill, bars, grid, circle, quad\n");
        printf("`next` cycles; a bare `test` redraws the current one.\n");
        return 0;
    }
    app_show_test(argv[1]);
    return 0;
}

static int cmd_next(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    app_next_test();
    return 0;
}

static int cmd_backlight(int argc, char **argv)
{
    if (argc < 2) {
        printf("Backlight %d%%\n", bsp_backlight_get());
        return 0;
    }
    bsp_backlight_set(atoi(argv[1]));
    printf("Backlight -> %d%%\n", bsp_backlight_get());
    return 0;
}

static int cmd_flush(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uint32_t frames = 0, pixels = 0;
    gfx_get_stats(&frames, &pixels);
    printf("full frames pushed : %u\n", (unsigned)frames);
    printf("panel transfers    : %u\n", (unsigned)bsp_lcd_flush_count());
    printf("pixels pushed      : %u\n", (unsigned)pixels);
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
    printf("Framebuffer   : %u B static\n", (unsigned)(GFX_W * GFX_H * 2));
    return 0;
}

static int cmd_version(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("round-1.28-gauge  |  ESP-IDF %s  |  no graphics library\n",
           esp_get_idf_version());
    printf("Target: %s   Cores: %d   CPU: %d MHz (configured)\n",
           CONFIG_IDF_TARGET, portNUM_PROCESSORS, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    printf("Panel: %dx%d GC9A01A, %d MHz SPI, Waveshare vendor init\n",
           BSP_LCD_H_RES, BSP_LCD_V_RES, CONFIG_BSP_LCD_SPI_CLK_MHZ);
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
        { .command = "test", .help = "Test screen: test [fill|bars|grid|circle|quad]",
          .func = &cmd_test },
        { .command = "next", .help = "Next test screen", .func = &cmd_next },
        { .command = "backlight", .help = "Backlight: backlight [0-100]", .func = &cmd_backlight },
        { .command = "flush", .help = "Show panel transfer statistics", .func = &cmd_flush },
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
