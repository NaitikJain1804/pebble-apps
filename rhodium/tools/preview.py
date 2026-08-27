#!/usr/bin/env python3
"""Host-side preview of the Rhodium watchface, for checking layout without an SDK.

    python3 tools/preview.py 11 18 [outdir]

Renders one PNG per platform. The proportions and the digit outlines are parsed
out of src/c/main.c so they cannot drift; the drawing order and the formulas
that apply the proportions are mirrored here by hand. A design check for fit,
shape and colour — the emulator is still the authority.
"""

import re
import sys
from math import cos, radians, sin
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
MAIN_C = ROOT / "src" / "c" / "main.c"

# name -> (w, h, round?, colour?)
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
    "DIAL": (255, 170, 170),      # GColorMelon
    "SHADOW": (255, 85, 85),      # GColorSunsetOrange
    "METAL": (170, 170, 170),     # GColorLightGray
    "METAL_LIT": (255, 255, 255),
    "METAL_CUT": (85, 85, 85),    # GColorDarkGray
    "TRACK": (85, 85, 85),
    "ENGRAVE": (0, 0, 0),
}
MONO = {
    "DIAL": (255, 255, 255),
    "SHADOW": (0, 0, 0),
    "METAL": (255, 255, 255),
    "METAL_LIT": (255, 255, 255),
    "METAL_CUT": (0, 0, 0),
    "TRACK": (0, 0, 0),
    "ENGRAVE": (0, 0, 0),
}

DIGIT_X0, DIGIT_Y0, DIGIT_W_UNITS, DIGIT_H_UNITS = 1, 3, 8, 12


def load_source():
    src = MAIN_C.read_text()
    geom = {m[0]: int(m[1]) for m in re.findall(
        r"#define (G_\w+)\s+(-?\d+)",
        src.split("GEOMETRY_BEGIN")[1].split("GEOMETRY_END")[0])}
    digits = {}
    block = src.split("DIGITS_BEGIN")[1].split("DIGITS_END")[0]
    for name, body in re.findall(r"DIGIT_(\d)\[\] = \{([^}]*)\}", block):
        values = [int(v) for v in body.split(",")]
        strokes, i = [], 0
        while values[i]:
            n = values[i]
            strokes.append([(values[i + 1 + 2 * p], values[i + 2 + 2 * p]) for p in range(n)])
            i += 1 + 2 * n
        digits[int(name)] = strokes
    return geom, digits


def at_least(v, floor):
    return max(v, floor)


def pct(v, p):
    return int(v * p / 100)


class Face:
    def __init__(self, platform, geom, digits):
        self.w, self.h, self.round, is_color = PLATFORMS[platform]
        self.c = COLOR if is_color else MONO
        self.g, self.digits = geom, digits
        self.img = Image.new("RGB", (self.w, self.h), self.c["DIAL"])
        self.d = ImageDraw.Draw(self.img)

        self.centre = (self.w // 2, self.h // 2)
        self.half_w = self.w // 2 - 2
        self.half_h = self.h // 2 - 2
        self.r = min(self.half_w, self.half_h)
        self.hand_w = at_least(pct(self.r, geom["G_HAND_W"]), geom["G_HAND_W_MIN"])
        self.numeral_h = self.hand_w - 5
        self.stroke = 2 if self.r >= 90 else 1
        self.shadow = 2 if self.r >= 110 else 1

    def dial_radius(self, deg):
        if self.round:
            return self.r
        s, c = abs(sin(radians(deg))), abs(cos(radians(deg)))
        r = self.r * 4
        if s > 0:
            r = min(r, self.half_w / s)
        if c > 0:
            r = min(r, self.half_h / c)
        return int(r)

    def frame(self, origin, deg, along, across):
        s, c = sin(radians(deg)), cos(radians(deg))
        return (int(origin[0] + along * s + across * c),
                int(origin[1] + across * s - along * c))

    def lit_side(self, deg):
        return -1 if cos(radians(deg)) + sin(radians(deg)) > 0 else 1

    def applied(self, origin, deg, shape):
        lit = self.lit_side(deg)
        body, half = [], []
        for along, across in shape:
            across *= lit
            body.append(self.frame(origin, deg, along, across))
            half.append(self.frame(origin, deg, along, 0 if across * lit > 0 else across))
        shadow = [(x + self.shadow, y + self.shadow) for x, y in body]
        self.d.polygon(shadow, fill=self.c["SHADOW"])
        self.d.polygon(body, fill=self.c["METAL"])
        self.d.polygon(half, fill=self.c["METAL_LIT"])
        self.d.line(body + [body[0]], fill=self.c["METAL_CUT"], width=1)

    def baton(self, deg, across):
        g = self.g
        outer = (self.dial_radius(deg)
                 - at_least(pct(self.r, g["G_TICK_INSET"]), g["G_TICK_IN_MIN"])
                 - at_least(pct(self.r, g["G_TICK_LEN"]), g["G_TICK_LEN_MIN"])
                 - pct(self.r, g["G_BATON_INSET"]) // 2)
        inner = outer - pct(self.r, g["G_BATON_LEN"])
        w = at_least(pct(self.r, g["G_BATON_W"]), g["G_BATON_W_MIN"])
        shape = [(inner, -w // 2), (outer, -w // 2), (outer, w // 2), (inner, w // 2)]
        self.applied(self.frame(self.centre, deg, 0, across), deg, shape)

    def dial(self):
        g = self.g
        tick_len = at_least(pct(self.r, g["G_TICK_LEN"]), g["G_TICK_LEN_MIN"])
        inset = at_least(pct(self.r, g["G_TICK_INSET"]), g["G_TICK_IN_MIN"])
        for i in range(60):
            deg = i * 6
            outer = self.dial_radius(deg) - inset
            length = tick_len * 3 // 2 if i % 5 == 0 else tick_len
            self.d.line([self.frame(self.centre, deg, outer - length, 0),
                         self.frame(self.centre, deg, outer, 0)],
                        fill=self.c["TRACK"], width=1)
        w = at_least(pct(self.r, g["G_BATON_W"]), g["G_BATON_W_MIN"])
        for i in range(12):
            if i == 0:
                self.baton(0, -w)
                self.baton(0, w)
            else:
                self.baton(i * 30, 0)

    def hand(self, deg, length):
        w, tail = self.hand_w, pct(self.r, self.g["G_HAND_TAIL"])
        shape = [(-tail, -w // 2), (length * 88 // 100, -w // 2), (length, 0),
                 (length * 88 // 100, w // 2), (-tail, w // 2)]
        self.applied(self.centre, deg, shape)

    def numerals(self, deg, along, text):
        digit_w = self.numeral_h * DIGIT_W_UNITS // DIGIT_H_UNITS
        advance = digit_w + self.g["G_DIGIT_GAP"]
        start = -(advance * len(text) - self.g["G_DIGIT_GAP"]) // 2
        flip = sin(radians(deg)) < 0
        origin = self.frame(self.centre, deg, along, 0)
        for i, ch in enumerate(text):
            base = start + i * advance
            for stroke in self.digits[int(ch)]:
                pts = []
                for x, y in stroke:
                    u = base + (x - DIGIT_X0) * digit_w // DIGIT_W_UNITS
                    v = ((y - DIGIT_Y0) * self.numeral_h // DIGIT_H_UNITS
                         - self.numeral_h // 2)
                    pts.append(self.frame(origin, deg, -u if flip else u, v if flip else -v))
                self.d.line(pts, fill=self.c["ENGRAVE"], width=self.stroke)

    def cap(self):
        r = self.hand_w // 2 + 1
        cx, cy = self.centre
        for (x, y), rr, col in (((cx + self.shadow, cy + self.shadow), r, "SHADOW"),
                                ((cx, cy), r, "METAL_CUT"),
                                ((cx - 1, cy - 1), r - 3, "METAL_LIT")):
            self.d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=self.c[col])

    def render(self, hour, minute):
        self.dial()
        hour_len = pct(self.r, self.g["G_HOUR_LEN"])
        minute_len = pct(self.r, self.g["G_MINUTE_LEN"])
        ha, ma = (hour % 12) * 30, minute * 6
        self.hand(ha, hour_len)
        self.hand(ma, minute_len)
        self.cap()
        self.numerals(ha, pct(hour_len, self.g["G_TEXT_POS"]), str(hour))
        self.numerals(ma, pct(minute_len, self.g["G_TEXT_POS"]), "%02d" % minute)

        if self.round:  # tint what a round bezel would cut away
            cx, cy = self.centre
            rr = min(self.w, self.h) // 2
            for y in range(self.h):
                for x in range(self.w):
                    if (x - cx) ** 2 + (y - cy) ** 2 > rr ** 2:
                        self.img.putpixel((x, y), (40, 40, 60))
        return self.img


def main():
    hour = int(sys.argv[1]) if len(sys.argv) > 1 else 11
    minute = int(sys.argv[2]) if len(sys.argv) > 2 else 18
    outdir = Path(sys.argv[3]) if len(sys.argv) > 3 else ROOT / "preview"
    outdir.mkdir(parents=True, exist_ok=True)
    geom, digits = load_source()
    for platform in PLATFORMS:
        path = outdir / f"{platform}.png"
        Face(platform, geom, digits).render(hour, minute).save(path)
        print(path)


if __name__ == "__main__":
    main()
