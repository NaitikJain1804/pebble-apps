# Rhodium

An analogue watchface whose hands spell out the time they point at. At 11:18 the
hour hand points at the 11 marker and carries **11**; the minute hand points at
minute 18 and carries **18**.

Salmon dial (`GColorMelon`, with `GColorSunsetOrange` for the minute track),
rhodium-plated hands — white bodies, light-grey bevel, dark-grey outline — and
black numerals in Droid Serif Bold where the screen is big enough for it.

## Written in C, not Alloy

Unlike `orrery/`, this face targets **every** Pebble platform: aplite, basalt,
chalk, diorite, emery, flint, gabbro. Alloy only runs on emery and gabbro, so C
is the only option. Round 2 (gabbro) gets the largest layout tier.

## Two design decisions worth knowing

**The numerals stay upright.** The SDK cannot rotate text, so each hand ends in
a pill-shaped plaque that carries its number horizontally. That is also the more
readable answer — a rotated "18" at the bottom of the dial would be upside down.

**The hour hand snaps to its marker.** A conventional hour hand creeps toward
the next hour as the minutes pass; here it would then read "11" while sitting
almost on the 12. Snapping keeps the hand and its numeral always in agreement,
which is the whole point of the face. The minute hand snaps to its minute
naturally.

## Layout

One table in `src/c/main.c` (between the `LAYOUT_TABLE_BEGIN`/`END` markers)
picks sizes from the shorter screen dimension:

| Tier | Platforms | Hour plaque r | Minute plaque r | Plaque | Numerals |
| --- | --- | --- | --- | --- | --- |
| ≥240 | gabbro 260×260 | 44 | 92 | 52×34 | Droid Serif 28 Bold |
| ≥190 | emery 200×228 | 32 | 72 | 48×32 | Droid Serif 28 Bold |
| ≥170 | chalk 180×180 | 30 | 62 | 36×26 | Gothic 18 Bold |
| else | 144×168, Quick View | 24 | 52 | 32×22 | Gothic 18 Bold |

Two constraints govern those numbers, and both need re-checking if any of them
change:

- **Plaques must never collide.** When the hands line up (12:00, 3:15, …) the
  gap `minute_r - hour_r` has to exceed the plaque height.
- **Nothing may leave the screen.** On the round platforms the binding case is
  the plaque's far corner, at `minute_r + sqrt((w/2)² + (h/2)²)` from centre; on
  the rectangular ones it is `minute_r + w/2` at 3 and 9 o'clock.

Drawing order matters too: both shafts and the centre cap are drawn first, then
both plaques. Drawing each hand complete in turn puts the minute shaft straight
through the hour numeral whenever the hands align.

Black-and-white platforms (aplite, diorite) get a white dial with solid black
markers — outlined markers on a white dial disappear.

Quick View is handled by laying the hands out inside
`layer_get_unobstructed_bounds()`, which also drops the layout down a tier while
the screen is obstructed. The dial itself still fills the whole layer.

## Preview without an SDK

The Pebble SDK cannot be downloaded from this repo's build environment, so
layout is checked host-side:

```bash
python3 tools/preview.py 11 18 preview   # needs pillow; writes one PNG per platform
```

It parses the layout table out of `main.c` — the numbers cannot drift — but
mirrors the drawing order by hand and stands DejaVu in for the Pebble system
fonts at the same pixel height. Glyph widths are therefore close, not exact.
It is a design check for fit, position and colour; the emulator is still the
authority. Round screens have the area outside the bezel tinted so clipping
shows.

Committed PNGs in `preview/` show 11:18 on every platform.

To catch C typos without the SDK:

```bash
gcc -fsyntax-only -Wall -Itools/pebble_stub src/c/main.c                 # mono build
gcc -fsyntax-only -Wall -DSTUB_COLOR -Itools/pebble_stub src/c/main.c    # colour build
```

`tools/pebble_stub/pebble.h` declares only what `main.c` uses. It checks syntax
and types, nothing else — it is not a simulator.

## Build

```bash
pebble build
pebble install --emulator gabbro
```
