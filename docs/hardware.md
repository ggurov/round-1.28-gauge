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

## Known hardware fault on this unit: the display

Separate from the flashing trouble below, the panel does not paint reliably.
A large contiguous region of the dial - typically a quarter to a half of it -
goes unpainted or drops out and stays that way.

What it looks like: the white bezel ring always draws, and the green band,
ticks and numerals draw correctly *where* they draw, but whole sectors of the
dial are missing.  When the needle is moving the missing region changes, which
reads as flickering; with the needle frozen the missing region stays missing.

### What has been ruled out

Every one of these was tested on hardware:

| Changed | Effect on the missing regions |
|---|---|
| SPI clock 80 / 40 / 20 MHz | no change |
| Backlight 35 / 60 / 100 % | no change (100 % removes PWM entirely) |
| Refresh rate 2 / 5 / 10 / 20 / 30 fps | no change |
| Sync flush (wait for DMA before releasing the buffer) | no change |
| Full-frame render mode instead of partial tiles | **worse** - only the bezel drew |
| Needle frozen vs animated | missing region changes only because the needle invalidates it |
| Driver error counters | `flushes: 3629, driver errs: 0, timeouts: 0` |

Two observations narrow it down a lot:

1. **It happens with nothing being drawn.** With `demo off` and a fixed
   `value`, LVGL has nothing invalidated and issues no flushes at all - the
   image is static on our side - yet the missing region still appears and
   stays.  So the corruption is not coming from the render or flush path.
2. **It is not the brownout.** A 40 s watch of the console saw zero
   spontaneous reboots while the artifact was present.

### Leading hypothesis

A partial connection on the panel's flex.  A whole *contiguous block* of the
display staying dark, with everything else crisp, is what missing source-driver
lines look like - and it would be intermittent if the joint is marginal.

Worth trying before replacing anything:

* **Reseat the display flex** if your revision has a connector rather than a
  bonded FPC.  Press it home and re-test.
* Try the board on a **different 5V source** (powered hub, or 5V into VSYS) to
  rule out the rail sagging under panel load.
* Failing that, **swap in another board.**  These are inexpensive and all of
  the software here is known good.

## Known hardware fault on this unit: flashing

This specific board has a reproducible fault that makes programming it
unreliable. It is documented here in detail because it is a hardware defect,
not a software one, and it is worth raising with the vendor.

### Symptom

esptool can always identify the chip, read the MAC and read flash info. It
cannot complete a large `write_flash`:

| Single write size | Result |
|---|---|
| 16 KB | **OK**, hash verified, every time |
| 32 KB | `No more data to read from the serial port` |
| 64 KB | same |
| 128 KB | cannot even reconnect afterwards |
| 869 KB (the app) | dies at 3.9 %, i.e. just after the first 16 KB block |

The failure point is deterministic - it is always the same byte, not a random
glitch. After a failed write the chip **hangs**: it does not reset, does not
produce UART output, and does not answer esptool. Only a physical RESET
recovers it. (If it were resetting, holding BOOT would send it straight back
into the bootloader; it does not.)

### What has been ruled out

* **Brownout.** The bootloader does report `rst:0xf (BROWNOUT_RST)` during
  image hashing, and that is real, but it is not what kills the flash: the
  write failure is deterministic at exactly 16 KB at any baud, and the chip
  hangs rather than resetting. Lowering the LCD SPI clock (80 -> 40 MHz), the
  backlight (85 % -> 60 %), the flash frequency (80 -> 40 MHz) and the image
  size (945 -> 869 KB) did not move the boundary.
* **esptool baud / stub baud mismatch.** Flashing at 115200 with no baud
  change at all fails identically.
* **esptool's flasher stub.** `--no-stub` (driving the ROM loader directly)
  gets as far as `Failed to configure SPI flash pins (result was C000: Bad
  data length)`.
* **Compression.** `--no-compress` fails identically; the stub writes 16 KB
  blocks either way.
* **The USB path.** Two cables, two ports, no hub, and an extension cable
  removed from the chain. The CH343P stays enumerated and healthy throughout -
  it is the chip that stops answering, and it recovers on RESET without a
  re-enumeration.

### One flash did succeed

Before any of this the board accepted a complete 945 KB image in one go, and
that firmware ran (see below). So the hardware is marginal rather than dead,
and it appears to have degraded over the session.

### Working around it

`tools/flash_chunked.py` splits an image into 16 KB pieces and hands them all
to esptool as separate regions in a **single** invocation, so the chip is never
disconnected between them:

```
python tools/flash_chunked.py --port COM6 0x20000 firmware/build/round_gauge.bin
```

That is the closest thing to a workaround found so far. It has not yet
completed on this unit.

### Recommendation

Treat this board as faulty and try another one - these are inexpensive, and all
of the software in this repository is known good. If a replacement behaves the
same way, the next suspects are the USB port's power delivery (try a powered
hub, or feed 5 V into the VSYS pin so the board is not drawing from USB) and
the CH343P bridge.

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
