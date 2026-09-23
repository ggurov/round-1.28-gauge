# Tests

Three layers, cheapest first. `tools\test.ps1` runs the first two; add
`-Device` for the third.

```powershell
tools\test.ps1                  # host unit tests + contract checks  (~2 s)
tools\test.ps1 -Filter math     # only suites whose name contains "math"
tools\test.ps1 -Device          # also build, flash and run on the ESP32-S3
```

## How tests register themselves

There is no list to keep in sync. A test declares itself where it lives:

```c
TF_TEST(gauge_math, value_to_angle_maps_endpoints)
{
    TF_NEAR(gauge_math_value_to_angle(&RPM, 0.0f), 225.0, 1e-3);
}
```

`TF_TEST` expands to the test function plus a constructor that files it with
the registry, so anything compiled into the binary is discovered at startup.
On the target the same idea uses ESP-IDF's Unity:

```c
TEST_CASE("gauge: root fills the display", "[gauge]")
{
    ...
}
```

## Layer 1 — host unit tests (`tests/host`)

Plain C compiled with the system GCC, linked against the *real* source files
from the firmware. No hardware, no ESP-IDF, under a second to run.

This is possible because the logic is deliberately separated from LVGL:

| File | Depends on | Covered by |
|---|---|---|
| `gauge_math.c` | libm only | host |
| `gauge_theme.c` | nothing | host |
| `gauge_presets.c` | string.h | host |
| `gauge.c` | LVGL | device |
| `gauge_needle.c` | LVGL + embedded blob | both |

What is checked:

* **`test_gauge_math.c`** — angle mapping (endpoints, midpoints, monotonicity,
  exact sweep width), tick counts, label generation and formatting, the
  rail/numeral/needle geometry formulae, and the needle slew filter including
  convergence time, rate limiting and no-overshoot.
* **`test_gauge_theme.c`** — every theme produces a coherent dial: the band fits
  inside the bezel, major ticks are longer and thicker than minor ones, the
  needle reaches past the numerals but stops short of the tick band, and the
  hub never swallows the numerals.
* **`test_gauge_presets.c`** — every preset is renderable: ranges are ordered,
  the tick budget fits the label storage, explicit label arrays match the tick
  count, generated labels fit their buffer, the alarm band is inside the range
  and wide enough to see, and the whole dial geometry is self-consistent.
* **`test_needle_asset.c`** — the generated sprite matches the constants in
  `gauge_needle_size.h`: exact byte size, pivot covered, tip at the declared
  distance, blade symmetric, edges anti-aliased, and every pixel pure white so
  the theme's `image_recolor` tints it correctly.

The framework lives in `tests/host/test_framework.{h,c}`. Assertions come in
soft (`TF_CHECK`, `TF_EQ_INT`, `TF_NEAR`, `TF_STR_EQ`, …) and hard (`TF_REQUIRE`,
which abandons the current test).

## Layer 2 — contract checks (`tests/py/test_contracts.py`)

Guards the assumptions the firmware makes about things it does not own, and
about its own generated artefacts. These are exactly the assumptions that rot
silently on an upgrade.

* **`LV_SCALE_DEFAULT_LABEL_GAP` must still equal `GAUGE_LABEL_GAP`.** The
  numeral radius is derived from this private LVGL constant; if an LVGL bump
  changes it, every numeral on the dial moves. The check reads the constant
  straight out of `lv_scale.c`.
* **`tools/gen_needle.py` must agree with `gauge_needle_size.h`** — otherwise
  the sprite and the pivot the widget rotates about drift apart.
* **Every Montserrat size a theme asks for must be enabled in
  `sdkconfig.defaults`.** A missing font falls back silently and changes the
  layout.
* **`tools/render_preview.py` must match the firmware** — same preset ids, same
  bezel/band/tick/hub dimensions — so the host previews stay trustworthy.

## Layer 3 — device tests (`tests/device`)

An ESP-IDF app that runs ESP-IDF's Unity against the **real LVGL object tree**,
using a headless 240×240 display (`lvgl_test_env.c`) so no panel or SPI setup is
involved. They pass even on a board with a dead display.

```powershell
tools\test.ps1 -Device
tools\test.ps1 -Device -Port COM7
```

What is checked: every preset builds, renders and tears down; the root fills the
display; defaults are resolved on the live object; needle rotation tracks the
value; the pivot lands on the dial centre; values clamp; the read-out matches
each preset's `decimals`; the needle slews rather than jumps and never leaves
the dial while doing so; create/delete is leak-free (LVGL's heap is compared
before and after); and the embedded needle blob resolves with the right header
and pixel data — which is what catches an `EMBED_FILES` linker-symbol change.

The app prints a final machine-readable line that
[`tools/run_device_tests.py`](../tools/run_device_tests.py) waits for:

```
TESTS_COMPLETE total=11 failures=0 ignored=0
```

Because the board cannot be reset over USB, the runner waits patiently and
prints a hint to press RESET if the chip is still sitting in the ROM bootloader
after flashing.

## Adding a test

1. Put host-testable logic in `gauge_math.c` / `gauge_theme.c` /
   `gauge_presets.c` rather than in `gauge.c`.
2. Add a `TF_TEST(...)` anywhere under `tests/host/` — it is picked up
   automatically.
3. Add a `TEST_CASE(...)` under `tests/device/main/` for anything that needs
   real LVGL objects.
4. If you introduce a dependency on a third-party constant or on a generated
   asset, add a contract check in `tests/py/test_contracts.py`.
