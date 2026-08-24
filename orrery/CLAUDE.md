# Orrery

A watchface for Pebble Round 2 (gabbro): the inner solar system as a clock, on a
black sky.

- **Sun** — fixed at the centre of the screen.
- **Mercury** — one orbit per minute, so it reads as the seconds hand.
- **Venus** — one orbit per hour: minutes.
- **Earth** — one orbit per 12 hours: hours.

Twelve o'clock is straight up and all three planets travel clockwise, so the
positions read like a conventional analogue face.

## Structure

Alloy (JavaScript) project, `"projectType": "moddable"`.

| File | Role |
| --- | --- |
| `src/embeddedjs/main.js` | Poco/screen setup and the one-second redraw |
| `src/embeddedjs/planets.js` | All scene drawing — takes a `render`, imports nothing |
| `src/embeddedjs/manifest.json` | Both modules must be registered here |
| `tools/preview.mjs` | Host-side renderer for previewing without an emulator |

`planets.js` deliberately has no Poco import: it receives the renderer, which
lets `tools/preview.mjs` run the exact same drawing code on a desktop.

## Design notes

- **No resource bitmaps.** Every body is built from filled discs and chord
  lines, which keeps the app to two JS files and lets each planet be lit from
  the sun's live direction rather than from a direction baked into artwork.
- **Lighting.** Each planet's surface features are laid out in a local frame
  whose +x axis points at the sun, then rotated into screen space. Surface
  detail therefore turns with the planet as it goes round.
- **Colours** are all on the 64-colour Pebble grid (channels 0/85/170/255), so
  nothing shifts when the firmware quantises them.
- **Band chords** are drawn one row at a time, each stopping at the limb, so
  polar caps and cloud decks never spill past a planet's silhouette.
- **Orbit rings** are drawn by filling a disc and punching it out with the
  background, so they must be drawn outermost first.
- **Geometry** keeps bodies clear of each other at conjunction and clear of the
  round bezel: sun with corona 37 px, Mercury 40–60, Venus 66–90, Earth 94–122,
  bezel at 130.

## Battery

This face redraws once a second, unlike the `minutechange` default, because
Mercury *is* the seconds hand. That is inherent to the design. Dropping the
seconds would mean driving it from `minutechange` and giving Mercury the
minutes.

## Preview without an SDK

```bash
node tools/preview.mjs 10 8 40 preview.png   # hour minute second out.png
```

The preview mirrors Poco's semantics — integer coordinates, filled discs, no
antialiasing — and tints the area outside the round bezel so clipping is
obvious. It is a design check, not a substitute for the emulator.

## Build

```bash
pebble build
pebble install --emulator gabbro
```
