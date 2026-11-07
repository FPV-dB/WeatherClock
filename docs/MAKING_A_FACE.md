# How to make a clock face

A face is **one file** in `weatherclock/` and **one line** in `faces_list.h`. Everything else follows on its own: the theme number, the panel gallery (with a preview), the tests and the simulator pictures.

## 1. Create the file

`weatherclock/face_<name>.cpp`, with a function `void themeX(const UiState &s, bool full)`:

```cpp
// Big face: just the time, huge, with the date underneath.
#include "ui_kit.h"

// HH:MM layout in pixel art: x of each digit, y, cell size, x of the colon
static const TimeLayout TL = {{20, 66, 136, 182}, 70, 8, 116};

void themeBig(const UiState &s, bool full) {
  // full = first call (the screen was already cleared with BG): draw the fixed parts here
  if (full) G->drawFastHLine(40, 150, 160, CARD2);

  // time: drawTime4 only repaints the digits that changed
  if (s.timeOk) {
    int h = displayHour(s);
    drawTime4(TL, h / 10, h % 10, s.now.tm_min / 10, s.now.tm_min % 10,
              s.now.tm_sec % 2 == 0, s.accent, BG);
  }

  // date: changed() says whether the text changed since last time
  char d[24];
  dateLabel(s, d, sizeof d);
  if (changed(cA, d)) uiTextBox(0, 164, 240, 16, BG, 120, 164, 2, MUTED, d, 'c');  // off-screen: no flicker
}
```

## 2. Register it

At the **end** of the list in `weatherclock/faces_list.h` (the position is the number saved in the settings; never insert in the middle):

```cpp
  X(WIND, themeWind, "Wind", "Compass, speed, gusts and wind strength") \
  X(BIG, themeBig, "Big", "Just the time, huge")
```

## 3. Look at it in the simulator

```
sim/build.sh
```

- `sim/out/face-N.png`: the face as it appears on screen (N = position in the list);
- the line `<name>  ok (0 different pixels, 0 flickering pixels)` confirms that incremental redrawing leaves nothing behind and nothing flickers. If it says `FAILED`, something in that area isn't erased when it changes, or a pixel briefly shows the wrong colour;
- the same script updates `docs/faces/` (gallery previews) and `web.h`.

## 4. Build and check the size

```
arduino-cli compile --fqbn "esp8266:esp8266:generic:eesz=4M2M,xtal=160,ip=lm2n" --build-property "build.float=" --output-dir build weatherclock
```

## What `UiState` gives you

| Field | What it is |
|---|---|
| `timeOk`, `now` | valid time and the local `struct tm` (`tm_hour`, `tm_min`, `tm_sec`, `tm_wday`, `tm_mday`, `tm_mon`) |
| `hour12` | 12-hour format (use `displayHour(s)`) |
| `accent` | the colour chosen in the panel for the numbers |
| `weatherOn`, `wx` | weather on, and its data: `wx.ok`, `wx.t`, `wx.tmin`, `wx.tmax`, `wx.hum`, `wx.wind`, `wx.gust`, `wx.wdir`, `wx.code`, `wx.day`, `wx.version` |
| `city`, `ip` | city and IP |
| `ms` | `millis()`, for animations |

## The kit (`ui_kit.h`)

- **Text** (6×8 font, ASCII only): `text`, `textC` (centred), `textR`, `textW`, `uiTextBox` (off-screen box), `degree`, `tempText`, `tempDesc`, `weatherText`
- **Digits**: `dmDigit` / `drawTime4` (pixel art), `geoDigit` (stroke vector font, any size), `outfitNum` and the smoothed vector fonts
- **Weather**: `weatherIcon(cx, cy, scale, s.wx.code, s.wx.day, background, ms)` (animated when `ms` > 0)
- **Sprites**: `uiSprite(&SPR_…)` (see `assets/`)
- **Shapes**: `thickLine`, `mix`, and everything in Adafruit GFX through `G->` (`fillRoundRect`, `fillCircle`…)
- **Palette**: `BG`, `CARD`, `CARD2`, `TEXT`, `MUTED`, `DIM`, `WORK`, `SHORT`, `LONG`, `SUN`, `CLOUD`, `RAIN`
- **Off-screen drawing**: `uiPaint(false, x, y, w, h, draw, ctx)` draws an area off-screen and sends it finished
- **Redraw caches**: `changed(cA…cC, text)`, `lastDig`, `lastColon`, `lastMin`, `lastHour`, `lastWx` (compare with `s.wx.version`), reset on every screen change

## Rules that avoid trouble on the device

- **Redraw only what changed.** The function runs every ~100 ms; clearing the whole screen flickers. To change a text, repaint only that area.
- **No flicker**: never clear an area on screen and then draw over it. Paint over it with the background colour (like `drawTime4` does with digits) or build it off-screen with `uiPaint`/`uiTextBox`.
- **RAM is scarce (~80 KB in total)**: no big buffers. Constant tables go in `PROGMEM` and are read with `pgm_read_byte`/`pgm_read_word`; on the ESP8266 a plain `static const` takes RAM.
- **No floating point in a per-pixel loop**: the ESP8266 has no FPU. Per row or per object is fine; per pixel, use integers.
- **Animations** (like the Flip turnover) may run in a loop of their own: call `uiFrameHook()` between frames (it gives time to Wi-Fi) and use `uiMillis()` to drive them by time.
- **Third-party fonts**: only with a licence that allows embedding (e.g. SIL OFL), with the licence file in `licenses/`.
