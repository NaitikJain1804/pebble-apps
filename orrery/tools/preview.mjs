// Host-side Poco stand-in: runs the real planets.js layout against the real
// bitmap resources and writes a PNG, so the watchface can be eyeballed without
// an SDK or emulator.
//
//   node tools/preview.mjs <hour> <minute> <second> <out.png>
//
// Poco semantics are mirrored: integer coordinates, no antialiasing, bitmaps
// blitted opaquely. Area outside the round bezel is tinted so clipping shows.
import { readFileSync, writeFileSync } from "node:fs";
import { deflateSync, inflateSync } from "node:zlib";
import { loadBodies, drawScene } from "../src/embeddedjs/planets.js";

const W = 260, H = 260;
const IMG_DIR = new URL("../resources/img/", import.meta.url);
const RESOURCES = ["sun.png", "mercury.png", "venus.png", "earth.png"];

// --- minimal PNG decode (8-bit truecolour, the format render_bodies emits) ---

function readPNG(path) {
    const buf = readFileSync(path);
    let pos = 8, width = 0, height = 0, colorType = 0;
    const idat = [];
    while (pos < buf.length) {
        const len = buf.readUInt32BE(pos);
        const type = buf.toString("ascii", pos + 4, pos + 8);
        const data = buf.subarray(pos + 8, pos + 8 + len);
        if (type === "IHDR") {
            width = data.readUInt32BE(0);
            height = data.readUInt32BE(4);
            colorType = data[9];
            if (data[8] !== 8 || (colorType !== 2 && colorType !== 6)) {
                throw new Error(`${path}: expected 8-bit RGB/RGBA PNG`);
            }
        } else if (type === "IDAT") {
            idat.push(data);
        } else if (type === "IEND") {
            break;
        }
        pos += 12 + len;
    }
    const bpp = colorType === 6 ? 4 : 3;
    const raw = inflateSync(Buffer.concat(idat));
    const stride = width * bpp;
    const out = Buffer.alloc(height * stride);
    for (let y = 0; y < height; y++) {
        const filter = raw[y * (stride + 1)];
        const line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1));
        for (let i = 0; i < stride; i++) {
            const a = i >= bpp ? out[y * stride + i - bpp] : 0;
            const b = y > 0 ? out[(y - 1) * stride + i] : 0;
            const c = i >= bpp && y > 0 ? out[(y - 1) * stride + i - bpp] : 0;
            let v = line[i];
            if (filter === 1) v += a;
            else if (filter === 2) v += b;
            else if (filter === 3) v += (a + b) >> 1;
            else if (filter === 4) {
                const p = a + b - c;
                const pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
                v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
            }
            out[y * stride + i] = v & 0xff;
        }
    }
    return { width, height, bpp, data: out };
}

// --- Poco stand-in ----------------------------------------------------------

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
    drawBitmap(bmp, x, y) {
        const { width, height, bpp, data } = bmp;
        for (let j = 0; j < height; j++) {
            for (let i = 0; i < width; i++) {
                const s = (j * width + i) * bpp;
                this.px(x + i, y + j, [data[s], data[s + 1], data[s + 2]]);
            }
        }
    }
}

// Stands in for Poco.PebbleBitmap(resourceId), 1-based into resources.media.
const PocoShim = {
    PebbleBitmap: class {
        constructor(id) {
            return readPNG(new URL(RESOURCES[id - 1], IMG_DIR));
        }
    },
};

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

// --- PNG encode -------------------------------------------------------------

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
drawScene(render, render.makeColor(0, 0, 0), loadBodies(PocoShim),
    new Date(2026, 0, 1, +hh, +mm, +ss));
maskRound(render);
writePNG(out ?? "preview.png", render.buf, 2);
