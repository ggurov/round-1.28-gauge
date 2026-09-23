/*
 * app_main.c - Round 1.28 Gauge
 *
 * Waveshare ESP32-S3-LCD-1.28 turned into a GReddy-flavoured automotive
 * instrument.  Current state: RPM tachometer with a self-test sweep and an
 * engine simulator, switchable to temperature / boost / volts at runtime.
 *
 * Boot order matters: the console comes up before the display so that a dead
 * panel, a blown SPI configuration or a bad LVGL theme can never lock you out
 * of the board.
 */
#include <stdio.h>

#include "app_console.h"
#include "bsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "gauge_demo.h"
#include "nvs_flash.h"
#include "esp_system.h"

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
    ESP_LOGI(TAG, "round-1.28-gauge booting (ESP-IDF %s, chip %s)",
             esp_get_idf_version(), CONFIG_IDF_TARGET);

    /* 1. Console first - this is the recovery path. */
    ESP_ERROR_CHECK(app_console_start());

    /* 2. Display, but never fatally. */
    err = bsp_display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "display init failed: %s (0x%x)", esp_err_to_name(err), err);
        ESP_LOGW(TAG, "No dial, but the console is alive.");
        ESP_LOGW(TAG, "Check Pins/SPI under `idf.py menuconfig` -> Round gauge BSP,");
        ESP_LOGW(TAG, "then `bootloader` + reflash. Try 40 MHz if 80 MHz is unstable.");
        return;
    }

    /* 3. Gauge + simulator. */
    err = gauge_demo_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gauge start failed: %s (0x%x)", esp_err_to_name(err), err);
        ESP_LOGW(TAG, "Console still alive; `bootloader` to reflash.");
        return;
    }

    printf("\n");
    ESP_LOGI(TAG, "Running. Type `help` for commands, `bootloader` to reflash.");
}
