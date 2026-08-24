// Orrery watchface — scene drawing.
//
// Pure drawing code: it takes a Poco `render` and a palette, so it can also be
// run under a host-side rasterizer for previewing. All shading is computed in a
// local frame whose +x axis points at the sun, so every planet is lit from the
// centre of the screen.

const DEG = Math.PI / 180;

// Orbit radii and body sizes, in pixels, for a 260x260 round screen.
// Spans (orbit +/- r) are chosen so that no two bodies touch at conjunction and
// Earth still clears the round bezel: sun+corona 37, Mercury 40-60,
// Venus 66-90, Earth 94-122, bezel 130.
const SUN_R = 30;
const MERCURY = { orbit: 50, r: 10 };
const VENUS = { orbit: 78, r: 12 };
const EARTH = { orbit: 108, r: 14 };

export function makePalette(render) {
    // Every colour is on the 64-colour Pebble grid (channels 0/85/170/255) so
    // nothing shifts when the firmware quantises it.
    return {
        black: render.makeColor(0, 0, 0),
        orbit: render.makeColor(85, 85, 85),

        coronaFar: render.makeColor(85, 0, 0),
        coronaNear: render.makeColor(170, 85, 0),
        sunLimb: render.makeColor(255, 85, 0),
        sunOuter: render.makeColor(255, 170, 0),
        sunMid: render.makeColor(255, 170, 85),
        sunInner: render.makeColor(255, 255, 85),
        sunHot: render.makeColor(255, 255, 170),
        sunCore: render.makeColor(255, 255, 255),
        spotUmbra: render.makeColor(170, 0, 0),
        spotPenumbra: render.makeColor(255, 85, 0),

        rockShadow: render.makeColor(85, 85, 85),
        rockBase: render.makeColor(170, 170, 170),
        rockLit: render.makeColor(255, 255, 255),
        craterFloor: render.makeColor(85, 85, 85),
        craterRim: render.makeColor(255, 255, 255),

        // Pale cream rather than orange, so Venus never reads as a second sun.
        venusShadow: render.makeColor(170, 170, 85),
        venusBase: render.makeColor(255, 255, 170),
        venusLit: render.makeColor(255, 255, 255),
        venusCloud: render.makeColor(255, 255, 255),
        venusBand: render.makeColor(255, 170, 85),

        oceanShadow: render.makeColor(0, 0, 170),
        ocean: render.makeColor(0, 85, 255),
        oceanLit: render.makeColor(85, 170, 255),
        land: render.makeColor(0, 170, 0),
        landDry: render.makeColor(170, 170, 0),
        ice: render.makeColor(255, 255, 255),
        cloud: render.makeColor(255, 255, 255),
    };
}

// --- primitives -------------------------------------------------------------

function disc(render, color, cx, cy, r) {
    render.drawCircle(color, cx | 0, cy | 0, r | 0);
}

// Horizontal band across a disc, drawn one row at a time so each row stops
// exactly at the limb — surface detail never spills outside the silhouette.
function band(render, color, cx, cy, r, dy, thickness) {
    const top = dy - ((thickness - 1) >> 1);
    for (let i = 0; i < thickness; i++) {
        const row = top + i;
        const inner = r * r - row * row;
        if (inner <= 1) continue;
        const half = Math.sqrt(inner) | 0;
        if (half < 1) continue;
        const y = (cy + row) | 0;
        render.drawLine((cx - half) | 0, y, (cx + half) | 0, y, color, 1);
    }
}

// A ring: fill the outer disc, then punch it out with the background. The
// punch-out clears everything inside it, so rings must be drawn outermost
// first.
function ring(render, color, bg, cx, cy, r, thickness) {
    disc(render, color, cx, cy, r);
    disc(render, bg, cx, cy, r - thickness);
}

// --- bodies -----------------------------------------------------------------

// Sunspot placement is fixed rather than random: the sun should look the same
// from one second to the next.
const SUNSPOTS = [
    { x: -9, y: 6, r: 4 },
    { x: 7, y: -11, r: 3 },
    { x: 13, y: 8, r: 3 },
];

// Faculae — bright mottling that keeps the photosphere from reading as a flat
// gradient.
const FACULAE = [
    { x: -14, y: -8, r: 3 },
    { x: 2, y: 14, r: 3 },
    { x: 16, y: -3, r: 2 },
    { x: -5, y: -16, r: 2 },
    { x: -18, y: 4, r: 2 },
];

function drawSun(render, p, cx, cy) {
    disc(render, p.coronaFar, cx, cy, SUN_R + 7);
    disc(render, p.coronaNear, cx, cy, SUN_R + 4);

    disc(render, p.sunLimb, cx, cy, SUN_R);
    disc(render, p.sunOuter, cx, cy, SUN_R - 2);
    disc(render, p.sunMid, cx, cy, SUN_R - 5);
    disc(render, p.sunInner, cx, cy, SUN_R - 9);
    disc(render, p.sunHot, cx, cy, SUN_R - 15);
    disc(render, p.sunCore, cx, cy, SUN_R - 22);

    for (let i = 0; i < FACULAE.length; i++) {
        const f = FACULAE[i];
        disc(render, p.sunHot, cx + f.x, cy + f.y, f.r);
    }
    for (let i = 0; i < SUNSPOTS.length; i++) {
        const s = SUNSPOTS[i];
        disc(render, p.spotPenumbra, cx + s.x, cy + s.y, s.r);
        disc(render, p.spotUmbra, cx + s.x, cy + s.y, s.r - 1);
    }
}

// Craters in the sun-facing local frame: {x toward the sun, y across it}.
// Few and large — at this size a dense field just reads as noise.
const CRATERS = [
    { x: -3, y: -4, r: 4 },
    { x: 4, y: 2, r: 3 },
    { x: 2, y: -5, r: 3 },
    { x: -3, y: 4, r: 3 },
];

function drawMercury(render, p, cx, cy, lx, ly) {
    const r = MERCURY.r;
    // Perpendicular to the sun direction, so craters keep their spacing as the
    // planet goes round.
    const px = -ly, py = lx;

    disc(render, p.rockShadow, cx, cy, r);
    disc(render, p.rockBase, cx + lx * 2, cy + ly * 2, r - 1);
    disc(render, p.rockLit, cx + lx * 4, cy + ly * 4, r - 5);

    for (let i = 0; i < CRATERS.length; i++) {
        const c = CRATERS[i];
        const x = cx + lx * c.x + px * c.y;
        const y = cy + ly * c.x + py * c.y;
        disc(render, p.craterRim, x, y, c.r);
        disc(render, p.craterFloor, x - lx, y - ly, c.r - 1);
    }
}

// Venus has no visible surface — only soft mottling in an unbroken sulphuric
// cloud deck, so its detail is swirls rather than bands.
const VENUS_CLOUDS = [
    { x: -4, y: -3, r: 3, bright: false },
    { x: 1, y: 4, r: 3, bright: true },
    { x: 4, y: -3, r: 3, bright: true },
    { x: -2, y: 4, r: 2, bright: false },
    { x: 2, y: -1, r: 3, bright: false },
    { x: -3, y: 0, r: 2, bright: true },
];

function drawVenus(render, p, cx, cy, lx, ly) {
    const r = VENUS.r;
    const px = -ly, py = lx;

    disc(render, p.venusShadow, cx, cy, r);
    disc(render, p.venusBase, cx + lx * 2, cy + ly * 2, r - 1);
    // Small highlight only — a large one swamps the cloud detail.
    disc(render, p.venusLit, cx + lx * 3, cy + ly * 3, r - 8);

    for (let i = 0; i < VENUS_CLOUDS.length; i++) {
        const c = VENUS_CLOUDS[i];
        const x = cx + lx * c.x + px * c.y;
        const y = cy + ly * c.x + py * c.y;
        disc(render, c.bright ? p.venusCloud : p.venusBand, x, y, c.r);
        disc(render, c.bright ? p.venusLit : p.venusBase, x + lx, y + ly, c.r - 2);
    }
}

// Continents in the sun-facing local frame.
const CONTINENTS = [
    { x: -1, y: -5, r: 4 },   // northern landmass
    { x: 1, y: -1, r: 4 },    // joins it southward
    { x: 2, y: 4, r: 3 },     // southern landmass
    { x: -6, y: 2, r: 3 },    // trailing continent
    { x: 6, y: -3, r: 2 },    // leading continent
];

function drawEarth(render, p, cx, cy, lx, ly) {
    const r = EARTH.r;
    const px = -ly, py = lx;

    disc(render, p.oceanShadow, cx, cy, r);
    disc(render, p.ocean, cx + lx * 2, cy + ly * 2, r - 1);
    disc(render, p.oceanLit, cx + lx * 4, cy + ly * 4, r - 7);

    for (let i = 0; i < CONTINENTS.length; i++) {
        const c = CONTINENTS[i];
        const x = cx + lx * c.x + px * c.y;
        const y = cy + ly * c.x + py * c.y;
        disc(render, p.land, x, y, c.r);
        disc(render, p.landDry, x + lx, y + ly, c.r - 2);
    }

    // Polar caps: narrow enough to taper toward the pole instead of reading as
    // a bar across the disc.
    band(render, p.ice, cx, cy, r - 1, -12, 3);
    band(render, p.ice, cx, cy, r - 1, 12, 3);

    // Weather systems, as discrete cloud knots rather than streaks.
    disc(render, p.cloud, cx - 5, cy - 7, 2);
    disc(render, p.cloud, cx + 6, cy + 5, 2);
    disc(render, p.cloud, cx + 2, cy + 9, 1);
}

// --- scene ------------------------------------------------------------------

// Clock convention: 12 o'clock is straight up, motion is clockwise.
function angleOf(fraction) {
    return (fraction * 360 - 90) * DEG;
}

export function drawScene(render, p, now) {
    const cx = render.width >> 1;
    const cy = render.height >> 1;

    const seconds = now.getSeconds();
    const minutes = now.getMinutes();
    const hours = now.getHours() % 12;

    const aMercury = angleOf(seconds / 60);
    const aVenus = angleOf((minutes + seconds / 60) / 60);
    const aEarth = angleOf((hours + minutes / 60) / 12);

    render.begin();
    render.fillRectangle(p.black, 0, 0, render.width, render.height);

    ring(render, p.orbit, p.black, cx, cy, EARTH.orbit, 1);
    ring(render, p.orbit, p.black, cx, cy, VENUS.orbit, 1);
    ring(render, p.orbit, p.black, cx, cy, MERCURY.orbit, 1);

    drawSun(render, p, cx, cy);

    // Outermost first, so an inner planet passing behind never clips an outer
    // one's disc.
    place(render, p, drawEarth, cx, cy, EARTH.orbit, aEarth);
    place(render, p, drawVenus, cx, cy, VENUS.orbit, aVenus);
    place(render, p, drawMercury, cx, cy, MERCURY.orbit, aMercury);

    render.end();
}

function place(render, p, drawBody, cx, cy, orbit, angle) {
    const ca = Math.cos(angle);
    const sa = Math.sin(angle);
    // Unit vector from the planet back toward the sun — the light direction.
    drawBody(render, p, cx + orbit * ca, cy + orbit * sa, -ca, -sa);
}
