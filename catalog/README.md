# Face catalogue

`index.json` lists the installable face packs (format 1 of the firmware's `facepack` engine).
The clock's panel reads this file for its online gallery.

```json
{
  "format": 1,
  "faces": [
    {
      "id": "bento",
      "name": "Bento",
      "author": "davicorrea0",
      "desc": "Widget-style grid of cards: time, weather and focus",
      "name_pt": "Bento",
      "desc_pt": "Cartões em grade, como widgets: hora, clima e focos",
      "file": "bento.json",
      "preview": "bento.png",
      "size": 2317,
      "fw": "1.6.5",
      "tags": ["digital"]
    }
  ]
}
```

- `file`: the face's `.json` pack (the same format as `faces/*.json` in the firmware repository).
- `preview`: 240×240 PNG rendered by the simulator (`sim/build.sh`); `anim` (optional) is an animated GIF preview.
- `name` / `desc`: English. `name_pt` / `desc_pt` and `name_es` / `desc_es`: optional translations for the panel.
- `fw`: minimum firmware version (e.g. `"2.0.3"`); older clocks are asked to update first.
- `tags`: styles: `analog`, `digital`, `pixel`, `sporty`, `retro` (one or more).
- Paths are relative to this `index.json` (or full URLs). Entries without `name` or `file` are ignored.
