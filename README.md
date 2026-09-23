# Round 1.28 Gauge

Turning a **Waveshare ESP32-S3-LCD-1.28** into an automotive instrument cluster.

Current state: a GReddy-inspired **RPM tachometer** on a 240×240 round LCD — black
dial, phosphor-green scale, glowing green rail, magenta redline sector, orange
blade needle — with a boot self-test sweep and an engine simulator. Temperature,
boost and battery gauges are already wired up and switchable at runtime.

![dial preview](tools/preview/dial_rpm.png)

---

## Hardware

| | |
|---|---|
| Board | Waveshare ESP32-S3-LCD-1.28 (non-touch) |
| SoC | ESP32-S3R2, dual-core LX7 @ 240 MHz |
| Flash | 16 MB Winbond W25Q128JV |
| PSRAM | 2 MB in-package (quad, disabled for now — see `sdkconfig.defaults`) |
| Display | GC9A01A round IPS, 240×240, 4-wire SPI, up to 80 MHz |
| USB | CH343P → UART0 on GPIO43/44 (COM port, 115200) |
| IMU | QMI8658 on I²C (SDA GPIO6 / SCL GPIO7) |
| Battery | ADC on GPIO1 through a 200K/100K divider |

Full discovery record, pinout and wiring notes: [`docs/hardware.md`](docs/hardware.md).

## Layout

```
firmware/
  components/
    bsp/     board support: SPI, GC9A01A panel, backlight PWM, LVGL port
    gauge/   the gauge widget
             gauge_math.c    pure arithmetic  (host-testable)
             gauge_theme.c   palettes         (host-testable)
             gauge_presets.c the catalogue    (host-testable)
             gauge.c         LVGL rendering
  main/      application: console, demo driver, boot
tests/
  host/      unit tests for the pure logic, run with the system GCC
  py/        contract checks on LVGL, the generator and the preview renderer
  device/    Unity app exercising the real LVGL tree on the ESP32-S3
tools/
  test.ps1            run every suite
  idf.bat             run idf.py with the toolchain environment loaded
  gen_needle.py       rasterise the needle sprite -> gauge/assets/*.bin
  render_preview.py   host-rendered dial mock-ups (tools/preview/*.png)
  probe.py            identify the board / dump chip info
  flash.ps1           reboot into the ROM bootloader and flash
docs/
  hardware.md      what the board is and how it was discovered
  development.md   toolchain, build, flash and recovery workflow
```

## Tests

Everything is covered by a suite, and tests register themselves where they live
so there is no list to keep in sync.

```powershell
tools\test.ps1                  # host unit tests + contract checks (~2 s)
tools\test.ps1 -Filter math     # only suites matching a name
tools\test.ps1 -Device          # also build, flash and run on the board
```

The logic that decides where things land on the dial lives in `gauge_math.c`,
`gauge_theme.c` and `gauge_presets.c`, all deliberately free of LVGL, so 63
unit tests run on the host in under a second. Contract checks then guard the
assumptions that would otherwise rot silently — that LVGL's private
`LV_SCALE_DEFAULT_LABEL_GAP` is still what the layout maths assumes, that
`gen_needle.py` and `gauge_needle_size.h` agree, that every font a theme asks
for is actually enabled, and that the preview renderer matches the firmware.
Finally `tests/device` runs Unity against the real LVGL widget tree with a
headless display, so it passes even on a board with a dead panel.

See [`tests/README.md`](tests/README.md) for what each suite covers and how to
add a test.

## Quick start

```powershell
# one-time:  build the needle sprite (only needed if you edit tools/gen_needle.py)
python tools\gen_needle.py

# build
tools\idf.bat build

# flash (the board must be in the ROM bootloader first - see below)
tools\idf.bat -p COM6 flash monitor
```

### Getting into the ROM bootloader

**This board cannot be reset into download mode over USB.** The CH343P's DTR/RTS
lines are not wired to `EN`/`GPIO0`, so esptool's reset sequences do nothing.
There are two ways in:

* **From the running app** — type `bootloader` at the `gauge>` console. The app
  sets `RTC_CNTL_FORCE_DOWNLOAD_BOOT` and resets, and the chip sits in the ROM
  bootloader until you flash it.
* **From cold** — hold **BOOT**, press and release **RESET**, keep holding BOOT
  for a second, then release it.

After flashing you may need to press **RESET** once, because the same missing
reset line means esptool cannot restart the chip afterwards.

## Console

The board brings up a REPL on the USB serial port **before** it touches the
display, so a dead panel can never lock you out.

| Command | What it does |
|---|---|
| `help` | List commands |
| `gauge` | List available instruments; `gauge temp` switches |
| `demo on\|off\|sweep` | Engine simulator, or replay the self-test sweep |
| `value 4200` | Drive the needle directly (stops the simulator) |
| `backlight 40` | Backlight duty, 0–100 |
| `bootloader` | Reboot into ROM download mode, ready for `idf.py flash` |
| `reset` | Restart the app |
| `free` | Heap usage |
| `version` | Build / chip information |

## Adding a gauge

Everything that distinguishes one instrument from another lives in
`gauge_config_t` (`firmware/components/gauge/include/gauge_config.h`). Add a
preset in
[`gauge_presets.c`](firmware/components/gauge/gauge_presets.c):

```c
static const gauge_config_t s_oil_temp = {
    .caption         = "OIL TEMP",
    .unit            = "DEG C",
    .wordmark        = "R-GAUGE",
    .tagline         = "TUNING SYSTEM",
    .min             = 40.0f,
    .max             = 160.0f,
    .major_step      = 20.0f,
    .minor_per_major = 4,
    .alarm_from      = 130.0f,     /* warning band start; > max for none  */
    .decimals        = 0,
    .slew_time       = 2.5f,       /* slow, damped movement               */
    .theme           = &gauge_theme_greddy,
};
```

Register it in `s_presets[]` and it appears in the `gauge` console command.
`tools\test.ps1` will then pick it up automatically: the preset tests build a
dial for every entry in the table and check the geometry is self-consistent.

To see what it looks like before flashing, add the same preset to
`tools/render_preview.py` and run it — a contract check fails if the two lists
drift apart.

## Visual design

The dial is drawn entirely from LVGL primitives rather than a bitmap, so it
re-renders at any geometry:

| Layer | Built from |
|---|---|
| Glow behind the rail | `lv_arc`, wide and semi-transparent |
| Rail, ticks, numerals, redline | `lv_scale` in `ROUND_INNER` mode |
| Needle | the generated sprite, `lv_image_set_rotation()` about the dial centre |
| Centre cap | `lv_obj` with `LV_RADIUS_CIRCLE` |
| Read-out and branding | `lv_label` |

The palette deliberately evokes the classic 1990s Japanese instrument look, but
the wordmark is our own — the default is `R-GAUGE`, change `.wordmark` in the
presets if you want something else.

`tools/render_preview.py` renders every preset on the host so the design can be
reviewed without flashing — the results are the PNGs in `tools/preview/`.

## Roadmap

- [x] Board bring-up, toolchain, gauge widget, RPM demo
- [x] Test suites: host unit tests, contract checks, on-target LVGL tests
- [ ] Real data: CAN / OBD-II / analogue inputs
- [ ] Serial OTA so reflashing needs no buttons at all
- [ ] Enable the 2 MB PSRAM and move draw buffers / assets into it
- [ ] Persist gauge selection and calibration in NVS
- [ ] QMI8658 IMU: accelerometer-driven peak-hold, orientation sensing
