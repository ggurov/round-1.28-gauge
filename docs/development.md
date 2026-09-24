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
xtensa-esp-elf-gdb                      debugger
openocd-esp32                           on-chip debug
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

To reproduce the install from scratch:

```powershell
git clone --depth 1 --shallow-submodules --recursive --branch v5.5.5 `
    https://github.com/espressif/esp-idf.git C:\Espressif\frameworks\esp-idf-v5.5.5
$env:IDF_TOOLS_PATH = 'C:\Espressif'
cd C:\Espressif\frameworks\esp-idf-v5.5.5
.\install.bat esp32s3
```

## Managed components

| Component | Version | Why |
|---|---|---|
| `espressif/esp_lcd_gc9a01` | 2.0.4 | panel command interface only; the init sequence comes from Waveshare |

LVGL is deliberately **not** a dependency — see
[`hardware.md`](hardware.md#fault-1--the-display-only-painted-part-of-the-dial).
A contract check fails if it creeps back in.

## Build and flash cycle

```powershell
# 1. put the board in the ROM bootloader
#    from the running app, type `bootloader` at the gauge> prompt

# 2. flash
tools\idf.bat -p COM6 flash monitor

# 3. exit the monitor with Ctrl+]
```

`tools\flash.ps1` wraps that into one command and `tools\flash.ps1 -NoMonitor`
skips the monitor.

No button presses are needed: `EN` is wired to the CH343P's RTS line so esptool
restarts the chip itself after flashing. See
[`hardware.md`](hardware.md#the-reset-lines-correctly).

## Testing

```powershell
tools\test.ps1                  # host unit tests + contract checks (~2 s)
tools\test.ps1 -Filter render   # only suites whose name contains "render"
```

Run this before flashing anything. It is fast, needs no hardware, and covers
the whole renderer.

The key enabler: **`gfx` talks to the panel through exactly one function**
(`bsp_lcd_draw_bitmap`). Stubbing that lets `gfx.c`, `gfx_text.c` and
`gauge_render.c` be compiled and exercised on the desktop with a real
framebuffer. That is where most bugs get caught now — the host tests found a
real one where every glyph was drawn one ascent too low, which on the panel
showed up as the bottom of the "6" vanishing into the green band behind it.

See [`tests/README.md`](../tests/README.md).

The host tests need a native compiler. They default to MSYS2's
`C:\msys64\mingw64\bin\gcc.exe`; override with `-Gcc <path>`. Nothing else in
the project needs it.

## Configuration

Board settings live under **`menuconfig → Round gauge BSP`**:

| Option | Default | Notes |
|---|---|---|
| `BSP_LCD_SPI_CLK_MHZ` | 80 | sets the frame-rate ceiling: 115 KB per frame |
| `BSP_LCD_SWAP_RGB565_BYTES` | y | off if colours are wrong but shapes are right |
| `BSP_BACKLIGHT_DEFAULT_PERCENT` | 60 | raise with `backlight 100` |

`CONFIG_SPIRAM=n` on purpose: nothing needs it and PSRAM timing is a common
cause of boot loops.

## Design notes

### Layering, and why it matters for tests

| File | Depends on | Tested by |
|---|---|---|
| `gauge_math.c` | libm only | host unit tests |
| `gauge_theme.c` | nothing | host unit tests |
| `gauge_presets.c` | string.h | host unit tests |
| `gfx.c` | one BSP call | host unit tests, panel stubbed |
| `gfx_text.c` | `gfx.c` | host unit tests |
| `gauge_render.c` | `gfx` | host unit tests |
| `bsp.c` | ESP-IDF | hardware only |

Resist moving arithmetic into `gauge_render.c` or `bsp.c`.

### Frame budget

A full 240×240 RGB565 frame is 115 KB. At 80 MHz that is **11.5 ms of SPI**,
which is the frame-rate ceiling; the render is a couple of milliseconds on top.
Measured on the dial: **38.5 fps**.

Three things got it there, in order of how much they mattered:

1. **Raising the SPI clock 40 → 80 MHz** (26.6 → 38.5 fps)
2. **Not sleeping a fixed 20 ms per frame** (18.2 → 26.6 fps) — the loop now
   measures the real frame interval and passes it to the slew filter, so the
   panel sets the rate rather than the delay
3. **Drawing the warning sector as one arc band** rather than stamping a thick
   arc, which would have been thousands of discs per frame

`gfx_flush_rect()` exists for partial updates if a future design needs them,
but the current renderer always redraws the whole dial: at these sizes the
render is cheap and there is no cached state to fall out of step with the panel.

### Why the fonts are generated

`tools/gen_font.py` rasterises a system TTF into 8-bit coverage masks, emitted
as `gfx_font_data.c`. No font library runs on the device, the glyphs are
anti-aliased, and the whole set is 64 KB of flash.

Two details that are easy to get wrong and are both covered by tests:

* **`bearing_y` is measured from the baseline**, not the ascender. PIL places
  the text origin on the ascender line, so the ascent has to be subtracted
  again. Getting this wrong draws every glyph one ascent too low.
* **The charset is a contiguous `0x20..0x7E`**, so the device can index glyphs
  with `(c - first)` and needs no lookup table. A non-contiguous set silently
  indexes the wrong glyph.

Text is positioned on **cap height**, not line height: the ascent includes room
that digits and capitals never use, so centring on the line box puts text
visibly low. Use `gfx_text_cap_centered()`.

### Why the needle slews

`gauge_render_set_value()` only stores a target. The task moves `displayed`
toward it with an exponential approach capped by a maximum slew rate, so the
needle accelerates off a stop and settles like a real moving-coil movement.
`slew_time` in the config is the time for a full-scale ramp — 0.30 s for a
tachometer, 2.0 s for a temperature gauge.

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

## Regenerating the fonts

```powershell
python tools\gen_font.py
```

Writes `firmware/components/gfx/gfx_font_data.c` and its header. A contract
check fails if the generated file drifts from the generator's declared sizes.
