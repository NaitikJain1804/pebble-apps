#!/usr/bin/env python3
"""Host-side preview of the Rhodium watchface, for checking layout without an SDK.

    python3 tools/preview.py 11 18 [outdir]

Renders one PNG per target platform. The layout numbers are parsed out of
src/c/main.c so the preview cannot drift from the watchface; the drawing order
below mirrors main.c by hand. System fonts are stood in for with DejaVu at the
same pixel height, so glyph widths are close but not exact — this is a design
check for fit, position and colour, not a substitute for the emulator.
"""

import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
MAIN_C = ROOT / "src" / "c" / "main.c"

# Platform name -> (width, height, round?, colour?)
PLATFORMS = {
    "aplite": (144, 168, False, False),
    "basalt": (144, 168, False, True),
    "chalk": (180, 180, True, True),
    "diorite": (144, 168, False, False),
    "emery": (200, 228, False, True),
    "flint": (144, 168, False, True),
    "gabbro": (260, 260, True, True),
}

COLOR = {
    "DIAL": (255, 170, 170),        # GColorMelon
    "DIAL_DEEP": (255, 85, 85),     # GColorSunsetOrange
    "METAL": (255, 255, 255),       # polished rhodium
    "METAL_SHADE": (170, 170, 170),  # GColorLightGray
    "METAL_EDGE": (85, 85, 85),      # GColorDarkGray
    "NUMERAL": (0, 0, 0),
    "TICK": (255, 255, 255),
    "TICK_EDGE": (85, 85, 85),
}
MONO = {
    "DIAL": (255, 255, 255),
    "DIAL_DEEP": (0, 0, 0),
    "METAL": (255, 255, 255),
    "METAL_SHADE": (255, 255, 255),
    "METAL_EDGE": (0, 0, 0),
    "NUMERAL": (0, 0, 0),
    "TICK": (0, 0, 0),
    "TICK_EDGE": (0, 0, 0),
}

# DejaVu stands in for the Pebble system fonts. The number in a Pebble font key
# is its line height in pixels, which is what PIL's size argument means too.
FONT_FILES = {
    "FONT_KEY_DROID_SERIF_28_BOLD": ("/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf", 24),
    "FONT_KEY_GOTHIC_18_BOLD": ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16),
}

TIER_FIELDS = ["min_dim", "hour_r", "minute_r", "pill_w", "pill_h", "shaft_w",
               "tick_len", "tick_w", "text_dy", "minute_pips", "font"]


def load_tiers():
    src = MAIN_C.read_text()
    block = src.split("LAYOUT_TABLE_BEGIN")[1].split("LAYOUT_TABLE_END")[0]
    tiers = []
    for row in re.findall(r"\{([^{}]*)\}", block):
        values = [v.strip() for v in row.split(",")]
        tier = dict(zip(TIER_FIELDS, values))
        for key in TIER_FIELDS[:-1]:
            tier[key] = int(tier[key])
        tiers.append(tier)
    return tiers


def pick_tier(tiers, min_dim):
    for tier in tiers:
        if min_dim >= tier["min_dim"]:
            return tier
    return tiers[-1]


def polar(centre, angle_deg, r):
    from math import cos, radians, sin
    a = radians(angle_deg)
    # Pebble's integer trig truncates; close enough for a layout check.
    return (int(centre[0] + sin(a) * r), int(centre[1] - cos(a) * r))


def draw_line(d, a, b, color, width):
    d.line([a, b], fill=color, width=width)
    # Pebble's thick lines have rounded-ish ends; fake the joint at both ends.
    if width > 2:
        r = width // 2
        for p in (a, b):
            d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=color)


def render(platform, hour, minute, tiers):
    w, h, is_round, is_color = PLATFORMS[platform]
    c = COLOR if is_color else MONO
    img = Image.new("RGB", (w, h), c["DIAL"])
    d = ImageDraw.Draw(img)

    centre = (w // 2, h // 2)
    radius = min(w, h) // 2
    tier = pick_tier(tiers, radius * 2)

    # --- dial -------------------------------------------------------------
    if tier["minute_pips"]:
        for i in range(60):
            if i % 5 == 0:
                continue
            p = polar(centre, i * 6, radius - 6)
            d.ellipse([p[0] - 1, p[1] - 1, p[0] + 1, p[1] + 1], fill=c["DIAL_DEEP"])
    else:
        d.ellipse([centre[0] - radius + 6, centre[1] - radius + 6,
                   centre[0] + radius - 6, centre[1] + radius - 6],
                  outline=c["DIAL_DEEP"], width=1)

    for i in range(12):
        angle = i * 30
        length = tier["tick_len"] if i % 3 == 0 else tier["tick_len"] * 2 // 3
        outer_r = radius - 4
        outer = polar(centre, angle, outer_r)
        inner = polar(centre, angle, outer_r - length)
        draw_line(d, inner, outer, c["TICK_EDGE"], tier["tick_w"] + 2)
        draw_line(d, inner, outer, c["TICK"], tier["tick_w"])

    # --- hands ------------------------------------------------------------
    def shaft(angle_deg, r):
        from math import cos, radians, sin
        tip = polar(centre, angle_deg, r)
        draw_line(d, centre, tip, c["METAL_EDGE"], tier["shaft_w"] + 2)
        draw_line(d, centre, tip, c["METAL"], tier["shaft_w"])
        off = max(1, tier["shaft_w"] // 3)
        a = radians(angle_deg)
        dx, dy = int(cos(a) * off), int(sin(a) * off)
        draw_line(d, (centre[0] + dx, centre[1] + dy), (tip[0] + dx, tip[1] + dy),
                  c["METAL_SHADE"], 1)

    def plaque(angle_deg, r, text):
        tip = polar(centre, angle_deg, r)
        pill = [tip[0] - tier["pill_w"] // 2, tip[1] - tier["pill_h"] // 2,
                tip[0] + tier["pill_w"] // 2, tip[1] + tier["pill_h"] // 2]
        corner = tier["pill_h"] // 2
        d.rounded_rectangle(pill, radius=corner, fill=c["METAL"], outline=c["METAL_EDGE"])
        d.rounded_rectangle([pill[0] + 1, pill[1] + 1, pill[2] - 1, pill[3] - 1],
                            radius=corner - 1, outline=c["METAL_SHADE"])

        path, size = FONT_FILES[tier["font"]]
        font = ImageFont.truetype(path, size)
        d.text((tip[0], tip[1] + tier["text_dy"]), text, font=font,
               fill=c["NUMERAL"], anchor="mm")

    hour_angle, minute_angle = (hour % 12) * 30, minute * 6
    shaft(hour_angle, tier["hour_r"])
    shaft(minute_angle, tier["minute_r"])

    cap_r = tier["shaft_w"] // 2 + 3
    d.ellipse([centre[0] - cap_r, centre[1] - cap_r, centre[0] + cap_r, centre[1] + cap_r],
              fill=c["METAL_EDGE"])
    d.ellipse([centre[0] - cap_r + 1, centre[1] - cap_r + 1,
               centre[0] + cap_r - 1, centre[1] + cap_r - 1], fill=c["METAL"])
    d.ellipse([centre[0] - 1, centre[1] - 1, centre[0] + 1, centre[1] + 1],
              fill=c["DIAL_DEEP"])

    plaque(hour_angle, tier["hour_r"], str(hour))
    plaque(minute_angle, tier["minute_r"], "%02d" % minute)

    # Anything a round screen would cut away is tinted, so clipping is obvious.
    if is_round:
        for y in range(h):
            for x in range(w):
                if (x - centre[0]) ** 2 + (y - centre[1]) ** 2 > radius ** 2:
                    img.putpixel((x, y), (40, 40, 60))
    return img


def main():
    hour = int(sys.argv[1]) if len(sys.argv) > 1 else 11
    minute = int(sys.argv[2]) if len(sys.argv) > 2 else 18
    outdir = Path(sys.argv[3]) if len(sys.argv) > 3 else ROOT / "preview"
    outdir.mkdir(parents=True, exist_ok=True)

    tiers = load_tiers()
    for platform in PLATFORMS:
        img = render(platform, hour, minute, tiers)
        path = outdir / f"{platform}.png"
        img.save(path)
        print(path)


if __name__ == "__main__":
    main()
