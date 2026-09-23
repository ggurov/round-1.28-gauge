/*
 * app_console.h - the recovery / control console on UART0.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the interactive REPL on the console UART.  Deliberately brought up
 * before the display so there is always a way back in. */
esp_err_t app_console_start(void);

/* Parks the RTC_CNTL force-download-boot bit and resets, so the ROM
 * bootloader comes up and esptool can talk to the chip without the physical
 * BOOT + RESET dance. */
void app_reboot_to_bootloader(void);

#ifdef __cplusplus
}
#endif
