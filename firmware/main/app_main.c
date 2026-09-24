/*
 * app_main.c - Round 1.28 Gauge bring-up.
 *
 * No graphics library.  The panel is driven from a plain RGB565 framebuffer in
 * the gfx component, and this app currently draws test screens so the panel can
 * be verified before any gauge code is layered on top.
 *
 * Order matters: the console comes up before the display so a dead panel never
 * locks you out of the board.
 */
#include <stdio.h>

#include "app_console.h"
#include "app_tests.h"
#include "bsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "gfx.h"
#include "nvs_flash.h"

static const char *TAG = "app";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    printf("\n\n");
    ESP_LOGI(TAG, "round-1.28-gauge bring-up (ESP-IDF %s, chip %s, no LVGL)",
             esp_get_idf_version(), CONFIG_IDF_TARGET);

    /* 1. Console first - the recovery path. */
    ESP_ERROR_CHECK(app_console_start());

    /* 2. Display, but never fatally. */
    err = bsp_display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "display init failed: %s (0x%x)", esp_err_to_name(err), err);
        ESP_LOGW(TAG, "No display, but the console is alive.");
        ESP_LOGW(TAG, "Check pins and clock under menuconfig -> Round gauge BSP.");
        ESP_LOGW(TAG, "Then `bootloader` and reflash.");
        return;
    }

    gfx_init();

    /* 3. Panel self-test, then leave a test screen up. */
    app_boot_sequence();

    printf("\n");
    ESP_LOGI(TAG, "Ready. `test <fill|bars|grid|circle|quad>`, `next`, `help`.");
}
