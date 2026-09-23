# Development

## Toolchain

Everything is installed under `C:\Espressif`:

| | |
|---|---|
| ESP-IDF | `v5.5.5` at `C:\Espressif\frameworks\esp-idf-v5.5.5` |
| Tools | `C:\Espressif\tools` |
| Python venv | `C:\Espressif\python_env\idf5.5_py3.13_env` |
| Host Python | 3.13.14 (used by `install.bat` to build the venv) |

The Xtensa toolchain, OpenOCD, GDB, CMake, Ninja and ccache all come from
`install.bat esp32s3`:

```
xtensa-esp-elf        esp-14.2.0        compiler
xtensa-esp-elf-gdb    (gdb)             debugger
openocd-esp32         (openocd)         on-chip debug
cmake, ninja, ccache, idf-exe
```

Nothing is added to `PATH`. Every command goes through
[`tools/idf.bat`](../tools/idf.bat), which sources `export.bat` and forwards its
arguments to `idf.py`:

```powershell
tools\idf.bat build
tools\idf.bat -p COM6 flash monitor
tools\idf.bat menuconfig
tools\idf.bat size
```

Override the install location with the `IDF_PATH` / `IDF_TOOLS_PATH`
environment variables if yours lives elsewhere.

To reproduce the install from scratch:

```powershell
git clone --depth 1 --shallow-submodules --recursive --branch v5.5.5 `
    https://github.com/espressif/esp-idf.git C:\Espressif\frameworks\esp-idf-v5.5.5
$env:IDF_TOOLS_PATH = 'C:\Espressif'
cd C:\Espressif\frameworks\esp-idf-v5.5.5
.\install.bat esp32s3
```

## Managed components

Pulled from `components.espressif.com` on first configure, pinned in
`dependencies.lock`:

| Component | Version |
|---|---|
| `lvgl/lvgl` | 9.6.0 |
| `espressif/esp_lcd_gc9a01` | 2.0.4 |

Bump with `tools\idf.bat update-dependencies` or by clearing
`dependencies.lock`.

## Build and flash cycle

```powershell
# 1. put the board in the ROM bootloader (see docs/hardware.md)
#    from the running app:  type `bootloader` at the gauge> prompt

# 2. flash
tools\idf.bat -p COM6 flash monitor

# 3. if the app does not come up, press RESET once
```

Exit the monitor with `Ctrl+]`.

`tools\flash.ps1` wraps steps 1–2:

```powershell
tools\flash.ps1              # COM6 by default
tools\flash.ps1 -Port COM7
```

It opens the port, sends the `bootloader` command, waits for the chip to
re-enumerate, then runs `idf.py flash monitor`.

## Configuration

Board-level settings live under **`menuconfig → Round gauge BSP`** so you can
change pins and the SPI clock without touching code:

| Option | Default | Notes |
|---|---|---|
| `BSP_LCD_SPI_CLK_MHZ` | 80 | drop to 40 if the panel shimmers |
| `BSP_LCD_INVERT_COLOR` | y | off if the dial looks like a negative |
| `BSP_LCD_SWAP_RGB565_BYTES` | y | off if colours are wrong but shapes are right |
| `BSP_LCD_BUFFER_LINES` | 40 | bigger = fewer, longer SPI bursts |
| `BSP_BACKLIGHT_DEFAULT_PERCENT` | 85 | boot brightness |

Deliberate defaults in `sdkconfig.defaults`:

* `CONFIG_SPIRAM=n` — the 2 MB in-package PSRAM is there but nothing needs it
  yet, and PSRAM timing is a common cause of boot loops. Turn it on when you
  start storing real assets.
* Two 4 MB OTA app slots, so a future serial/OTA updater has somewhere to write.
* `CONFIG_FREERTOS_HZ=1000` — the gauge slew timer runs at 16 ms.

## Design notes

### Why LVGL

The gauge needs anti-aliased arcs, rotated sprites and cheap partial redraws.
LVGL 9.6 gives all three — `lv_scale` in `ROUND_INNER` mode draws the rail,
major/minor ticks, numerals and coloured sections natively, and
`lv_image_set_rotation()` rotates the needle sprite about the dial centre. The
whole dial is vector, so changing the range or tick spacing re-renders instead
of requiring new artwork.

### Why the needle is a sprite

The blade is rasterised once by [`tools/gen_needle.py`](../tools/gen_needle.py)
with 4× supersampling and shipped as a white-with-alpha blob, which the theme
tints at runtime through `image_recolor`. That means changing the needle colour
is a one-line theme edit, not a re-export.

The blob is embedded with ESP-IDF's `EMBED_FILES` rather than committed as a
generated `.c` array — a 123 KB binary instead of a ~900 KB source file, and it
compiles instantly. `gauge_needle.c` wraps it in an `lv_image_dsc_t` at runtime.

### Why the needle slews

`gauge_set_value()` only stores a target. A 16 ms `lv_timer` moves
`displayed` toward it with an exponential approach capped by a maximum slew
rate, so the needle accelerates off a stop and settles like a real
moving-coil movement. `slew_time` in the config is the time for a full-scale
move — 0.30 s for a tachometer, 2.0 s for a temperature gauge.

### Why the console comes first

`app_main()` starts the UART REPL *before* `bsp_display_init()`. If the panel
fails to come up the app logs the error and returns, leaving a working console —
so a bad pin assignment or an unstable SPI clock costs you a `menuconfig` edit,
not a BOOT-button recovery.

## Debugging

The console is the primary channel, and `ESP_LOGI` output is interleaved with it
on UART0 at 115200.

`idf.py monitor` decodes panics and backtraces automatically
(`esp-idf-panic-decoder` is installed).

**JTAG is not wired.** The ESP32-S3's default JTAG pins are GPIO39–42, and
GPIO40 is the backlight. OpenOCD and GDB are installed and ready if the pins are
ever remapped or brought out to the 1.27 mm headers.

## Regenerating the needle

```powershell
python tools\gen_needle.py
```

Writes `firmware/components/gauge/assets/needle_argb8888.bin` plus a PNG
preview in `tools/preview/`. Keep `BLADE_LEN` in step with
`GAUGE_NEEDLE_TIP_DISTANCE` in
[`gauge_needle.h`](../firmware/components/gauge/include/gauge_needle.h).
