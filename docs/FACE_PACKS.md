# Face packs (install without updating the firmware)

A **pack** is a small `.json` file (a few KB) that describes a clock face in layers. It is installed from the clock's panel (*Clock → More faces*) and stored in file storage, next to the photos. No firmware flashing needed.

Ready-made examples: [`faces/`](../faces) (`giant-bebas.json`, `pixel-cafe.json`, `analog-radar.json`, `weather-half.json`).

```json
{
  "format": 1,
  "id": "my-clock",
  "name": "My Clock",
  "author": "your-name",
  "desc": "One sentence about the face",
  "bg": "#000000",
  "sprites": { },
  "layers": [
    { "type": "text", "x": 120, "y": 60, "font": "bebas", "size": 96, "align": "center", "color": "accent", "text": "{HH}:{MM}" },
    { "type": "text", "x": 120, "y": 180, "font": "pixel", "size": 2, "align": "center", "color": "muted", "text": "{WD} {DD} {MON}" }
  ]
}
```

| Field | |
|---|---|
| `format` | always `1` |
| `id` | file name on the clock (lowercase, digits, `-` and `_`, up to 23). Reinstalling with the same `id` replaces it |
| `name`, `author`, `desc` | shown in the gallery (English) |
| `name_pt`, `desc_pt`, `name_es`, `desc_es` | optional translations for the panel in Portuguese or Spanish |
| `bg` | background colour |
| `sprites` | pixel-art drawings used by `sprite` layers |
| `layers` | layers, drawn in order (the first one is at the bottom) |

The screen is **240 × 240** pixels; `x` grows to the right and `y` downwards.

## Colours

`"#rrggbb"` or a palette name: `accent` (the colour chosen in the panel), `text`, `muted`, `dim`, `card`, `card2`, `bg`, `work` (tomato red), `short` (green), `long` (blue), `sun`, `cloud`, `rain`, or `none` (don't draw).

## Text variables

| | | | |
|---|---|---|---|
| `{HH}` hour (2 digits, follows 12/24 h) | `{H}` hour without a leading zero | `{MM}` minutes | `{SS}` seconds |
| `{WD}` weekday (`TUE`) | `{DD}` day | `{MON}` month (`OCT`) | `{MO}` month (`10`) |
| `{YYYY}` year | `{AMPM}` | `{TEMP}` temperature | `{TMIN}` `{TMAX}` |
| `{HUM}` humidity | `{WX}` weather description | `{CITY}` city | `{IP}` |
| `{WIND}` wind speed | `{GUST}` gusts | `{WDIR}` wind direction (`SW`) | `{UNIT}` `C` or `F` |
| `{RAIN}` `rain 15 min` / `rain soon`, empty with no rain in the next hour | `{FOCUS}` focus sessions today | `{GOAL}` daily goal | `{STREAK}` `{BEST}` |
| `{FEELS}` feels-like temperature | `{UV}` UV index | `{AQI}` US air quality index | `{MOON}` moon phase (`Full moon`) |
| `{SUNRISE}` `{SUNSET}` (follow 12/24 h) | `{UVMAX}` today's peak UV, `{RAINPC}` `{RAINMM}` today's chance of rain (%) and expected mm | `{PM25}` `{PM10}` particles, µg/m³ | |

`{TEMP}`, `{TMIN}` and `{TMAX}` already come in the unit chosen in the panel (°C or °F). `°` can be written straight into the text (`"{TEMP}°"`).

`{FOCUS}`, `{GOAL}`, `{STREAK}` and `{BEST}` come from the original Pomodoro timer. Weather Clock has no timer, so they stay at zero. Older packs that use the Portuguese names `{FOCOS}` and `{META}` still work.

**Text in 3 languages**: instead of a string, `text` accepts an object with one version per language, and the clock shows the one for the language chosen in the panel. Any missing language falls back to `pt`, so give at least `en` and `pt`:

```json
{ "type": "text", "x": 16, "y": 140, "text": { "en": "WIND", "pt": "VENTO", "es": "VIENTO" } }
```

## Layers

Every layer accepts `"when"` to appear only under certain conditions: `day`, `night`, `rain`, `dry`, `weather` (weather available), `noweather`, `goal`, `nogoal`, `streak`, and for animated weather icons `sunny`, `moon`, `partly`, `partlynight`, `cloudy`, `fog`, `drizzle`, `rainy`, `snowy`, `storm`.

| `type` | Fields |
|---|---|
| `rect` | `x`, `y`, `w`, `h`, `r` (corners), `color` (fill), `stroke` (outline) |
| `circle` | `x`, `y` (centre), `r`, `color`, `stroke` |
| `line` | `x`, `y`, `x2`, `y2`, `size` (thickness), `color` |
| `text` | `x`, `y`, `text`, `font`, `size`, `align` (`left`/`center`/`right`; `x` is the alignment point), `color`, and depending on the font `off` or `bg`; optional `glow`, `pulse`, `spread`, `base` (see "Pulsing glow") |
| `sprite` | `x`, `y`, `sprite` (name in `sprites`), `size` (integer zoom) |
| `mascot` | the original project's tomato: `x`, `y`, `size`, `mood`: `auto` (sleeps at night, umbrella in the rain, cheers at the goal, scarf in the cold, sunglasses in the heat), `happy`, `sleeping`, `focused`, `relaxed`, `cheering`, `umbrella`, `scarf`, `glasses` |
| `plant` | the tomato plant, which grows with the streak: `x`, `y`, `size` (zoom of the 16×16 drawing) |
| `weather` | weather icon: `x`, `y` (centre), `size` (10 = ~40 px) |
| `bar` | `x`, `y`, `w`, `h`, `r`, `value` (`sec`, `min`, `hour`, `focus`, `hum`), `max` (optional), `color`, `track` |
| `hand` | clock hand: `x`, `y` (centre), `value` (`hour`, `min`, `sec`), `len`, `tail`, `size` (thickness), `color` |
| `image` | a JPEG in `/faces/` on the clock: `x`, `y`, `w`, `h`, `file` |
| `arc` | progress ring with round ends, clockwise from the top: `x`, `y` (centre), `r` (outer radius), `size` (thickness), `value`, `max` (optional), `color`, `track` |
| `ticks` | analog dial markings: `x`, `y` (centre), `r` (outer radius), `len`, `count` (12 = hours, 60 = minutes), `size`, `color` |

The old Portuguese mood names (`feliz`, `dormindo`, `focado`, `relax`, `comemorando`, `guarda-chuva`, `cachecol`, `oculos`) and the value `focos` are still accepted.

### Fonts (`text`)

| `font` | `size` | Notes |
|---|---|---|
| `pixel` | scale of the 6×8 font (1, 2, 3…) | any text (accented letters become the plain letter) |
| `dm` | side of each "pixel" of the 5×7 digit | digits, `:`, `-`, `.`; `off` paints the unlit pixels |
| `geo` | height in px | digits and `:`; `w` changes the stroke thickness |
| `dseg` | height in px | LCD 7 segments; `off` shows the unlit segments |
| `bebas` | height in px | smoothed digits and `:`; `bg` is the smoothing background colour (default: the pack's `bg`) |
| `outfit` | height in px | clean geometric, widget style (Outfit SemiBold), with letters too; otherwise like `bebas` |
| `righteous` | height in px | rounded geometric, Bauhaus/art deco style (Righteous); like `bebas` |

## Sprites

```json
"sprites": {
  "cup": {
    "pal": { "o": "#3a2a20", "w": "#f5f5f5", "c": "#6f4e37" },
    "px": [
      ".oooooooo.",
      ".occcccco.",
      ".owwwwwwoo",
      "..oooooo.."
    ]
  }
}
```

Each character of `px` is a pixel; `.` is transparent and the rest come from `pal` (up to 15 colours, one character each). Up to 32×32 and up to 12 sprites per pack.

### Animated sprites

Instead of `px`, use `frames`: a list of frames, each in the same format as `px` and with the same `pal`. `fps` (1 to 10, default 4) sets the speed.

```json
"sprites": {
  "blink": {
    "fps": 2,
    "pal": { "o": "#3a2a20", "y": "#ffd166" },
    "frames": [
      ["..oooo..", ".oyyyyo.", ".oyooyo.", ".oyyyyo.", "..oooo.."],
      ["..oooo..", ".oyyyyo.", ".oyyyyo.", ".oyyyyo.", "..oooo.."]
    ]
  }
}
```

The frame comes from the device's clock, so the animation runs by itself and only the sprite's area is redrawn. A sprite with `px` stays still. Limits: every frame the same size (up to 32×32), at most **8 frames** per sprite and **6 KB** of sprites per pack (each 32×32 frame takes 512 bytes; a 16×16 one, 128). See a full example in [`faces/jumping-tomato.json`](../faces/jumping-tomato.json); other animated ones: [`campfire`](../faces/campfire.json) (a 6-frame flame), [`winter`](../faces/winter.json) (one snow sprite repeated at two scales), [`catnap`](../faces/catnap.json), [`coffee-break`](../faces/coffee-break.json), [`aquarium`](../faces/aquarium.json), [`window`](../faces/window.json) (rain) and [`weather-half`](../faces/weather-half.json) (animated weather icons).

### Pulsing glow

A `text` layer can have a halo that breathes behind the letters, like a nixie tube (older firmware ignores the field and installs the pack without the glow):

```json
{ "type": "text", "x": 66, "y": 78, "font": "geo", "size": 58, "w": 8, "color": "#4a2d1b", "text": "{HH}", "align": "center",
  "glow": "#ff8a2a", "base": "#1c1e1a", "spread": 3, "pulse": 2400 }
```

- `glow`: halo colour (turns the feature on);
- `spread`: reach from 1 to 3 px (default 2);
- `pulse`: period in ms, from 400 to 10000 (without it, the glow stays steady);
- `base`: colour of the surface under the halo (default: the pack's `bg`). The halo is a blend of `base` and `glow`, so use the real colour of whatever is behind the text.

The intensity varies over 8 levels and only the text area is redrawn on each change. With `geo` the halo is just a thicker stroke (cheap); with the other fonts it costs 8 drawings per pixel of reach, so keep such texts small. Example: [`faces/nixie.json`](../faces/nixie.json).

## Limits

- file up to **16 KB**, up to **48 layers**;
- the clock redraws only what changes: layers with `{SS}` or a second hand are redrawn every second, so keep their areas small.

## Testing and publishing

1. Put the `.json` in [`faces/`](../faces) and run `sim/build.sh`: the simulator draws the pack, checks the incremental redraw (`pack-<id>  ok`), writes the preview `faces/<id>.png` and updates `faces/catalog.json`.
2. To get into the online gallery, open a pull request with the `.json`, the preview and the updated catalogue.
