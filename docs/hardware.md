# Hardware

## Identification

Physically labelled **Waveshare "ESP32-S3-LCD-1.28"** (non-touch variant). The
board presents itself over USB as a CH343P USB-to-UART bridge, so nothing about
it is self-identifying until the chip is probed.

### Enumeration on the host

```
Ports   USB-Enhanced-SERIAL CH343 (COM6)   USB\VID_1A86&PID_55D3\58A6069520
```

`VID_1A86` is QinHeng (WCH), `PID_55D3` is the CH343. The sibling device on
`COM49` is an unrelated CH340.

There is **no native USB-Serial-JTAG interface**: the ESP32-S3's own USB pins
(GPIO19/20) are not brought out to the Type-C connector, which is wired to the
CH343P only.

### Chip probe

```
Chip type:          ESP32-S3 (QFN56) (revision v0.2)
Features:           Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz,
                    Embedded PSRAM 2MB (AP_3v3)
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

The factory firmware in flash was crash-looping — booting into an
`RTCWDT_RTC_RST` reset every ~1.5 s with no application output — so a fresh
image had to be flashed before anything could be observed.

## Pinout

### Display — GC9A01A, 240×240 round IPS, 4-wire SPI

| Signal | GPIO |
|---|---|
| LCD_DC | 8 |
| LCD_CS | 9 |
| LCD_CLK | 10 |
| LCD_MOSI | 11 |
| LCD_RST | 12 |
| LCD_BL | 40 |

### Other peripherals

| Function | GPIO | Notes |
|---|---|---|
| UART0 TXD | 43 | → CH343P, 115200 baud console |
| UART0 RXD | 44 | ← CH343P |
| BOOT button | 0 | boot strap, active low |
| Battery sense | 1 | ADC, 200K/100K divider, `V = 3.3 / 4096 * 3 * raw` |
| QMI8658 SDA | 6 | 6-axis IMU |
| QMI8658 SCL | 7 | |
| QMI8658 INT1 / INT2 | 47 / 48 | |
| Touch INT | 5 | touch variant only |

Everything else is brought out on 1.27 mm pitch headers.

## The panel init sequence matters

`esp_lcd_gc9a01` ships its own init table, but this board is driven with
**Waveshare's sequence**, taken from their `ESP32-S3-LCD-1.28-Test` demo
(`LCD_1in28.cpp`, `LCD_1IN28_InitReg`) and passed in through
`gc9a01_vendor_config_t`. The two differ in the gate-driver settings:

| Command | `esp_lcd_gc9a01` | Waveshare |
|---|---|---|
| `0x62`, `0x63` (GOA timing) | `0x38 …` | `0x18 …` |
| `0xBD`, `0xBC` | absent | `0x06`, `0x00` |
| `0x35` (tearing effect on) | absent | present |
| `0x84` / `0x89` / `0x8D` / `0xC9` | `0x60` / `0x23` / `0x03` / `0x30` | `0x40` / `0x21` / `0x01` / `0x22` |
| `0x74` byte 1 | `0x45` | `0x85` |
| `0x3A` (COLMOD) | `0x55` | `0x05` |

`0x3A = 0x05` is the MCU-interface 16 bpp value; `0x55` is the RGB-interface
value. Both appear to work, but there is no reason to deviate from the vendor.

**Inversion:** Waveshare's sequence ends with `0x21` (INVON). Do **not** call
`esp_lcd_panel_invert_color(panel, false)` after it — that sends `0x20`
(INVOFF) and cancels it, leaving the display showing a photographic negative.
This caught us out once.

## Fault 1 — the display only painted part of the dial

### Symptom

Whole contiguous sectors of the dial stayed black, or were painted correctly
and then went black a fraction of a second later. With the needle moving the
missing region changed, which read as flickering; with the needle frozen the
missing region stayed missing.

### Root cause: LVGL

The first version used LVGL 9.6 with partial-tile rendering. The decisive test
was a **solid white fill drawn straight into the framebuffer**, bypassing LVGL
entirely:

* with LVGL: large regions dark
* without LVGL: the panel fills edge to edge, uniformly, brightness spread
  0.06 out of 255

So the panel, the wiring and the power were all fine, and the fault was in
LVGL's invalidate-and-repaint path. The firmware was rewritten around a plain
framebuffer and the problem disappeared completely.

### What was ruled out first

Every one of these was tested on hardware, and none of them changed the
missing regions:

| Changed | Effect |
|---|---|
| SPI clock 80 / 40 / 20 MHz | no change |
| Backlight 35 / 60 / 100 % | no change (100 % removes PWM entirely) |
| Refresh rate 2 / 5 / 10 / 20 / 30 fps | no change |
| Sync flush (wait for DMA before releasing the buffer) | no change |
| Full-frame render mode instead of partial tiles | **worse** — only the bezel drew |
| Needle frozen vs animated | region changes only because the needle invalidates it |
| Driver error counters | `flushes: 3629, driver errs: 0, timeouts: 0` |

Two observations narrowed it down early:

1. **It happened with nothing being drawn.** With the simulator off and a fixed
   value, LVGL had nothing invalidated and issued no flushes at all — the image
   was static on our side — yet the region still went dark and stayed dark.
2. **It was not a brownout.** A 40 s watch of the console saw zero spontaneous
   reboots while the artefact was present.

## Fault 2 — flashing was unreliable

### Symptom

esptool could always identify the chip, read the MAC and read flash info, but
large `write_flash` operations failed:

| Single write size | Result |
|---|---|
| 16 KB | OK, hash verified, every time |
| 32 KB | `No more data to read from the serial port` |
| 64 KB | same |
| 128 KB | could not even reconnect afterwards |
| 869 KB (the app) | died at 3.9 %, just after the first 16 KB block |

The failure point was deterministic — always the same byte — and afterwards the
chip hung: no UART output, no answer to esptool, only a physical RESET
recovered it. Holding BOOT did not help, because the chip was hanging rather
than resetting.

### Root cause: the USB cable

**A change of cable fixed it completely.** With a USB-C cable running straight
from the motherboard, a full 869 KB flash takes 11 seconds and verifies. The
earlier cable (via an extension) was marginal: enough for enumeration and small
transfers, not enough for sustained ones.

Two red herrings on the way:

* `rst:0xf (BROWNOUT_RST)` did appear during image hashing, and it is real, but
  it was a symptom of the same marginal link rather than an independent fault.
  Reducing the flash frequency to 40 MHz, the LCD clock to 20 MHz, the backlight
  to 35 % and the image size by 61 % did not move the 16 KB boundary.
* A baud-change theory (esptool's stub left listening at a raised baud) was
  tested by flashing at 115200 throughout — it failed identically.

`tools/flash_chunked.py` splits an image into 16 KB pieces and hands them to
esptool as separate regions in a single invocation, which is the closest thing
to a workaround if a marginal cable is ever unavoidable.

## The reset lines, correctly

An early conclusion here was wrong and is worth stating correctly, because it
determines the whole flashing workflow:

| Line | Wired to | Consequence |
|---|---|---|
| DTR | **nothing** | cannot enter download mode in software |
| RTS | `EN` | esptool *can* reset the chip after flashing |

Evidence for DTR: all 16 combinations of the two control lines were driven, with
a 3 s listen after each transition, and none produced the ROM's download-mode
banner (`waiting for download` / `boot:0x10 (DOWNLOAD(UART0))`).

Evidence for RTS: `idf.py flash` ends with `Hard resetting via RTS pin...` and
the chip reliably leaves the bootloader and runs the new image afterwards.

So the rule is:

* **entering** download mode needs the physical BOOT button, or the firmware's
  own `bootloader` console command
* **leaving** it is automatic — esptool handles that itself

The firmware's `bootloader` command sets `RTC_CNTL_FORCE_DOWNLOAD_BOOT` and
calls `esp_restart()`, which the ROM honours:

```
gauge> bootloader
Rebooting into ROM download mode...
rst:0xc (RTC_SW_CPU_RST),boot:0x10 (DOWNLOAD(USB/UART0))
waiting for download
```

That is the normal flashing path and needs no buttons at all.

## Notes for later

* **PSRAM** is present but disabled in `sdkconfig.defaults`. The framebuffer is
  115 KB and there is ~240 KB of internal heap free, so nothing needs it yet.
  Enable with `CONFIG_SPIRAM=y` when assets start to.
* **JTAG** is not usable over the Type-C port. The ESP32-S3's default JTAG pins
  are GPIO39–42, and GPIO40 is the backlight, so on-chip debugging would need a
  remap or bodging to the 1.27 mm headers. OpenOCD and GDB are installed and
  ready if that changes; the console is the practical debug channel today.
