# Orrery

A watchface for Pebble Round 2 (gabbro): the inner solar system as a clock, on
a black sky with no orbit lines.

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
| `src/embeddedjs/planets.js` | Scene layout — takes a `render`, imports nothing |
| `src/embeddedjs/manifest.json` | Both modules must be registered here |
| `resources/img/*.png` | The four bodies, pre-rendered |
| `tools/render_bodies.py` | Generates those PNGs from texture maps |
| `tools/preview.mjs` | Host-side renderer for previewing without an emulator |

`planets.js` deliberately has no Poco import: it receives the renderer, which
lets `tools/preview.mjs` run the exact same layout code on a desktop.

## The artwork

Each body is a bitmap resource rendered offline by `tools/render_bodies.py`:
real equirectangular texture maps projected onto an orthographic sphere, lit,
supersampled 8x for a smooth limb, then reduced to Pebble's 64-colour grid.

The reduction is the hard part, and each body needs a different answer:

- **Sun** — a mostly flat bright disc with hand-placed sunspot groups, not a
  broad radial gradient. A gradient forces the quantiser to dither across the
  whole disc, which reads as noise. Spots are placed in sphere coordinates so
  they foreshorten toward the limb. This is the only body that is dithered;
  its corona would otherwise contour into rings.
- **Mercury and Venus** — the map's brightness drives a designed colour ramp
  whose stops are exact palette colours. Neither body has enough colour of its
  own to survive being graded as a photograph.
- **Earth** — the map is classified into ocean, vegetation, desert and ice, and
  each material gets designed shade tiers. Grading Earth as one image cannot
  hold the materials apart at 28px: whatever makes the ocean read blue drags
  the continents with it. Downsampling is by majority vote rather than
  averaging, because averaging across a coastline produces a colour belonging
  to neither side (pink and teal, in practice).

Lighting is full-phase with a slightly off-axis key light: enough to raise
crater and cloud relief through the bump maps, not enough to cut a terminator
across a disc. A terminator would have to track each planet's orbital position,
which pre-rendered artwork cannot do.

### Regenerating the artwork

```bash
git clone --depth 1 https://github.com/jeromeetienne/threex.planets.git /tmp/tp
python3 tools/render_bodies.py /tmp/tp/images resources/img   # needs numpy, pillow
```

Texture maps are from [planetpixelemporium.com](http://planetpixelemporium.com/planets.html)
(James Hastings-Trew), derived from NASA imagery, obtained via the MIT-licensed
`jeromeetienne/threex.planets`. **Check their terms before publishing this
watchface to the app store** — they are free for personal use, but the maps are
not this project's to relicense.

## Layout

Sprites are square with black corners, drawn on a black sky, and no alpha
channel is used — the SDK's handling of bitmap transparency could not be
verified here, and black corners on a black background need no such support.

The catch is that a square sprite of half-size `h` reaches `h * sqrt(2)` at its
corners. Orbit radii are therefore set so that no sprite's *corner* reaches
another body's visible disc:

| | visible radius | sprite | orbit | corner clearance |
| --- | --- | --- | --- | --- |
| Sun | 30 (disc 48 + corona) | 64 | — | — |
| Mercury | 9 | 20 | 45 | 45 − 14 = 31 |
| Venus | 12 | 26 | 72 | 72 − 18 = 54 |
| Earth | 14 | 30 | 106 | ends at 120, bezel 130 |

Getting this wrong is visible as a black notch bitten out of the sun's corona
when Mercury passes, so re-check it if any size changes.

## Battery

This face redraws once a second, unlike the `minutechange` default, because
Mercury *is* the seconds hand. That is inherent to the design. Dropping the
seconds would mean driving it from `minutechange` and giving Mercury the
minutes.

## Preview without an SDK

```bash
node tools/preview.mjs 10 8 40 preview.png   # hour minute second out.png
```

The preview mirrors Poco's semantics — integer coordinates, no antialiasing,
opaque bitmap blits — and tints the area outside the round bezel so clipping is
obvious. It is a design check, not a substitute for the emulator.

## Build

```bash
pebble build
pebble install --emulator gabbro
```
