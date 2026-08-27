# Rhodium

An analogue watchface whose hands spell out the time they point at. At 11:18 the
hour hand points at the 11 baton and carries **ELEVEN** engraved along it; the
minute hand points at 18 and carries **EIGHTEEN**.

Salmon dial (`GColorMelon`), rhodium-plated hands and applied hour batons.

## Written in C, not Alloy

This face targets **every** Pebble platform: aplite, basalt, chalk, diorite,
emery, flint, gabbro. Alloy only runs on emery and gabbro, so C is the only
option.

## The finish

Flat 64-colour fills look cheap unless the light is consistent. Every metal
part — hands, batons, centre cap — is drawn the same way by `draw_applied()`:

1. a warm shadow (`GColorSunsetOrange`, the dial one tone down) offset down-right,
   so the part floats above the dial rather than sitting in it;
2. the body in `GColorLightGray`;
3. the half of the body facing the light in `GColorWhite`, split down the part's
   own axis, which is what reads as a bevel;
4. a `GColorDarkGray` cut edge around it.

The key light is at the top left, so `lit_side()` picks the highlighted half
from the part's angle. Change the light direction there and everything follows.

The dial itself is plain: sixty hairline minute ticks pinned to the edge of the
glass, longer every five, and a doubled baton at twelve so the dial has an up.

## The dial follows the case

`dial_radius()` returns where a ray from the centre meets the edge of the dial —
a constant on the round platforms, a ray/rectangle intersection on the
rectangular ones. Markers and ticks therefore sit along the case on every
Pebble, instead of on a circle inscribed in a rectangle.

## Rotated lettering

The time is spelled out — ELEVEN, EIGHTEEN, TWENTY THREE — not written in
digits. The SDK draws text upright only, so the words cannot be system-font
text if they are to run along the hands. Each letter is a stroked outline on a
small grid (`GLYPH_E`..`GLYPH_Z`; only the seventeen letters that spell numbers
exist), drawn as line segments transformed through the hand's own rotated frame,
always at a one-pixel stroke — anything heavier closes the counters at this size.

Readability comes from the flip: on the left half of the dial the word is turned
end for end, so the letters stand up rather than reading upside down, and the
word always reads left to right. A word longer than the space left on the hand
is nudged inboard rather than running off the tip.

The hand width is driven by the lettering (`letter_h = hand_w - 2`), so
narrowing `G_HAND_W` shrinks the words with it.

## Two design decisions worth knowing

**The hour hand snaps to its baton.** A conventional hour hand creeps toward the
next hour; here it would then read "11" while sitting almost on the 12. Snapping
keeps hand and numeral in agreement, which is the point of the face.

**Bodies first, then both words.** When the hands align the minute hand covers
the hour hand, so the hour word is drawn last, on top of whatever it lands
on. It stays readable because both hands are the same metal.

## Proportions

Everything scales off R, half the shorter screen dimension — percentages with
pixel floors, in the `GEOMETRY_BEGIN` block of `src/c/main.c`. There are no
per-platform layout tiers.

## Preview without an SDK

The Pebble SDK cannot be downloaded from this repo's build environment, so
layout is checked host-side:

```bash
python3 tools/preview.py 11 18 preview   # needs pillow; one PNG per platform
```

It parses the geometry constants and the letter outlines and number words out of `main.c` — those
cannot drift — but mirrors the drawing order and the formulas by hand. A design
check for shape, fit and colour; the emulator is still the authority. Committed
PNGs in `preview/` show 11:18 on every platform, with the area outside a round
bezel tinted so clipping shows.

To catch C typos without the SDK:

```bash
gcc -fsyntax-only -Wall -Itools/pebble_stub src/c/main.c                 # mono build
gcc -fsyntax-only -Wall -DSTUB_COLOR -Itools/pebble_stub src/c/main.c    # colour build
```

`tools/pebble_stub/pebble.h` declares only what `main.c` uses — syntax and types,
nothing else.

## Build

```bash
pebble build
pebble install --emulator gabbro
```
