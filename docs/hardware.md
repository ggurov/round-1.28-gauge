# Hardware

## Identification

Physically labelled **Waveshare "ESP32-S3-LCD-1.28"** (non-touch variant). The
board presents itself over USB as a CH343P USB-to-UART bridge, so nothing about
it is self-identifying until the chip is probed.

### Enumeration on the host

```
Ports   USB-Enhanced-SERIAL CH343 (COM6)   USB\VID_1A86&PID_55D3\58A6069520
```

`VID_1A86` is QinHeng (WCH), `PID_55D3` is the CH343. The board's sibling
device on `COM49` is an unrelated CH340.

Note there is **no native USB-Serial-JTAG interface** exposed: the ESP32-S3's
own USB pins (GPIO19/20) are not brought out to the Type-C connector, which is
wired to the CH343P only.

### Chip probe

`python -m esptool --port COM6 --before no-reset --after no-reset flash-id`:

```
Chip type:          ESP32-S3 (QFN56) (revision v0.2)
Features:           Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded PSRAM 2MB (AP_3v3)
Crystal frequency:  40MHz
MAC:                3c:84:27:26:ca:ac

Flash Memory Information:
Manufacturer: ef
Device:       4018
Detected flash size: 16MB
Flash type set in eFuse: quad (4 data lines)
Flash voltage set by eFuse: 3.3V
```

| | |
|---|---|
| Chip | ESP32-S3, QFN56, revision v0.2 |
| Cores | 2× Xtensa LX7 @ 240 MHz + LP core |
| Crystal | 40 MHz |
| Flash | 16 MB, `ef`/`4018` = Winbond **W25Q128JV**, quad |
| PSRAM | 2 MB in-package, quad, 3.3 V (`AP_3v3`) |
| MAC | `3c:84:27:26:ca:ac` |
| ROM bootloader | `esp32s3-20210327` |

The factory firmware in flash was **crash-looping** — booting into an
`RTCWDT_RTC_RST` reset every ~1.5 s with no application output. There was
therefore no way to reach the application, and a fresh image had to be flashed.

## Pinout

From the Waveshare wiki and schematic.

### Display — GC9A01A, 240×240 round IPS, 4-wire SPI

| Signal | GPIO |
|---|---|
| LCD_DC | 8 |
| LCD_CS | 9 |
| LCD_CLK | 10 |
| LCD_MOSI | 11 |
| LCD_RST | 12 |
| LCD_BL | 40 |

SPI is good to 80 MHz. The panel is driven through `esp_lcd_gc9a01`.

### Other peripherals

| Function | GPIO | Notes |
|---|---|---|
| UART0 TXD | 43 | → CH343P, 115200 baud console |
| UART0 RXD | 44 | ← CH343P |
| BOOT button | 0 | boot strap, active low |
| Battery sense | 1 | ADC, 200K/100K divider, `V = 3.3 / 4096 * 3 * raw` |
| QMI8658 SDA | 6 | 6-axis IMU |
| QMI8658 SCL | 7 | |
| QMI8658 INT1 | 47 | |
| QMI8658 INT2 | 48 | |
| Touch INT | 5 | touch variant only |
| RESET | — | board reset button |

Everything else is brought out on 1.27 mm pitch headers.

## The catch: no software reset path

The board is documented as having an "integrated automatic download circuit",
but on this unit **the CH343P's DTR and RTS lines are not wired to `EN` and
`GPIO0`**.

Evidence — all 16 combinations of the two control lines were driven, using
pyserial directly, with a 3 s listen window after each transition, and none of
them produced the ROM's download-mode banner
(`waiting for download` / `boot:0x0 (DOWNLOAD(UART0))`):

```
      dtr/rts 00 -> 01
      dtr/rts 00 -> 10
      dtr/rts 00 -> 11
      ...
      dtr/rts 11 -> 10
RESULT: no software-accessible download mode.
```

esptool's own `--before default_reset` sequence fails the same way with
`Failed to connect to Espressif device: No serial data received`.

### Consequences

1. **First flash needs hands.** Either hold **BOOT**, tap **RESET**, release
   **BOOT** — or, once our firmware is running, use the `bootloader` console
   command, which parks `RTC_CNTL_FORCE_DOWNLOAD_BOOT` and resets, leaving the
   ROM bootloader running.

2. **esptool cannot restart the chip after flashing it.** Every `--after`
   strategy esptool 4.12 offers (`hard-reset`, `soft-reset`, `watchdog-reset`)
   drives DTR/RTS. So after a flash the board may need a physical **RESET**
   press before the new image runs.

3. Because of (2), the plan is to add **serial OTA**: the running app writes a
   new image into the inactive OTA slot and reboots itself, which needs no
   buttons at all. `partitions.csv` already reserves two 4 MB app slots for
   exactly this.

`CONFIG_PARTITION_TABLE_CUSTOM` is in use, so the bootloader is not run in
`--bootloader-only` mode and the two OTA slots are ready.

## Notes for later

* **PSRAM** is present but disabled in `sdkconfig.defaults`. The 240×240 demo
  fits comfortably in internal SRAM, and leaving it off removes a class of
  boot-time failure while the board is still bring-up. Enable with
  `CONFIG_SPIRAM=y` when assets (fonts, images, canvases) start to need it.
* **JTAG** is not usable over the Type-C port. The ESP32-S3's default JTAG pins
  are GPIO39–42, but GPIO40 is taken by the backlight, so on-chip debugging
  would need either a remap via `CONFIG_ESP_DEBUG_...`/`esp_core_dump`, or
  bodging to the 1.27 mm headers. The console is the practical debug channel
  today; OpenOCD and GDB are installed and ready if that changes.
