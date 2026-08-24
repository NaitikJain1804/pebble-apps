// Host-side Poco stand-in: rasterises the real planets.js scene to a PNG, so
// the watchface can be eyeballed without an SDK or emulator.
//
//   node tools/preview.mjs <hour> <minute> <second> <out.png>
//
// The primitives here match Poco's: integer coordinates, filled discs, no
// antialiasing. Area outside the round bezel is tinted so clipping shows up.
import { writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { makePalette, drawScene } from "../src/embeddedjs/planets.js";

const W = 260, H = 260;

class FakePoco {
    constructor() {
        this.width = W;
        this.height = H;
        this.buf = new Uint8Array(W * H * 3);
    }
    makeColor(r, g, b) { return [r, g, b]; }
    begin() {}
    end() {}
    px(x, y, c) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        const i = (y * W + x) * 3;
        this.buf[i] = c[0]; this.buf[i + 1] = c[1]; this.buf[i + 2] = c[2];
    }
    fillRectangle(c, x, y, w, h) {
        for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) this.px(i, j, c);
    }
    drawCircle(c, cx, cy, r) {
        if (r < 0) return;
        const r2 = r * r;
        for (let j = cy - r; j <= cy + r; j++) {
            for (let i = cx - r; i <= cx + r; i++) {
                const dx = i - cx, dy = j - cy;
                if (dx * dx + dy * dy <= r2) this.px(i, j, c);
            }
        }
    }
    drawLine(x1, y1, x2, y2, c, thickness = 1) {
        const t = Math.max(1, thickness | 0);
        const steps = Math.max(Math.abs(x2 - x1), Math.abs(y2 - y1));
        for (let s = 0; s <= steps; s++) {
            const x = Math.round(x1 + ((x2 - x1) * s) / steps);
            const y = Math.round(y1 + ((y2 - y1) * s) / steps);
            for (let o = 0; o < t; o++) this.px(x, y - ((t - 1) >> 1) + o, c);
        }
    }
}

// Mask off the corners the round bezel hides, so clipping shows up in preview.
function maskRound(p) {
    const R = 130;
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
        const dx = x - 130 + 0.5, dy = y - 130 + 0.5;
        if (dx * dx + dy * dy > R * R) {
            const i = (y * W + x) * 3;
            p.buf[i] = 40; p.buf[i + 1] = 0; p.buf[i + 2] = 40;
        }
    }
}

// Minimal PNG encoder: scale up with nearest-neighbour, then one deflated
// IDAT of filter-0 scanlines.
function writePNG(path, buf, scale) {
    const w = W * scale, h = H * scale;
    const raw = Buffer.alloc(h * (w * 3 + 1));
    let o = 0;
    for (let y = 0; y < h; y++) {
        raw[o++] = 0;
        const sy = (y / scale) | 0;
        for (let x = 0; x < w; x++) {
            const i = (sy * W + ((x / scale) | 0)) * 3;
            raw[o++] = buf[i]; raw[o++] = buf[i + 1]; raw[o++] = buf[i + 2];
        }
    }
    const chunk = (type, data) => {
        const len = Buffer.alloc(4);
        len.writeUInt32BE(data.length);
        const body = Buffer.concat([Buffer.from(type, "ascii"), data]);
        const crc = Buffer.alloc(4);
        crc.writeUInt32BE(crc32(body) >>> 0);
        return Buffer.concat([len, body, crc]);
    };
    const ihdr = Buffer.alloc(13);
    ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4);
    ihdr[8] = 8; ihdr[9] = 2;
    writeFileSync(path, Buffer.concat([
        Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
        chunk("IHDR", ihdr),
        chunk("IDAT", deflateSync(raw)),
        chunk("IEND", Buffer.alloc(0)),
    ]));
}

const CRC_TABLE = (() => {
    const t = new Int32Array(256);
    for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
        t[n] = c;
    }
    return t;
})();

function crc32(b) {
    let c = ~0;
    for (let i = 0; i < b.length; i++) c = CRC_TABLE[(c ^ b[i]) & 0xff] ^ (c >>> 8);
    return ~c;
}

const [hh, mm, ss, out] = process.argv.slice(2);
const render = new FakePoco();
drawScene(render, makePalette(render), new Date(2026, 0, 1, +hh, +mm, +ss));
maskRound(render);
writePNG(out ?? "preview.png", render.buf, 2);
