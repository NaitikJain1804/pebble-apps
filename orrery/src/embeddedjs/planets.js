// Orrery watchface — scene layout.
//
// The bodies are pre-rendered bitmap resources (see tools/render_bodies.py);
// this module only places them. It takes a Poco `render` rather than importing
// one, so tools/preview.mjs can run the same layout code on a desktop.

const DEG = Math.PI / 180;

// Orbit radii for a 260x260 round screen. Sprites are square with black
// corners and are drawn on a black sky, so the corners are invisible — but
// only while no sprite's corner reaches another body's visible disc. A sprite
// of half-size h reaches h*sqrt(2) at its corners, not h, so the clearances
// below are diagonal ones: sun visible radius 30, then Mercury at 45
// (45 - 10*1.41 = 31), Venus at 72, Earth at 106, whose disc ends at 120
// against a 130 bezel.
const MERCURY_ORBIT = 45;
const VENUS_ORBIT = 72;
const EARTH_ORBIT = 106;

// Resource ids are 1-based indices into package.json's resources.media, in
// declaration order.
const SUN = 1;
const MERCURY = 2;
const VENUS = 3;
const EARTH = 4;

export function loadBodies(Poco) {
    return {
        sun: new Poco.PebbleBitmap(SUN),
        mercury: new Poco.PebbleBitmap(MERCURY),
        venus: new Poco.PebbleBitmap(VENUS),
        earth: new Poco.PebbleBitmap(EARTH),
    };
}

// Clock convention: 12 o'clock is straight up, motion is clockwise.
function angleOf(fraction) {
    return (fraction * 360 - 90) * DEG;
}

function place(render, bitmap, cx, cy, orbit, angle) {
    const x = cx + orbit * Math.cos(angle) - bitmap.width / 2;
    const y = cy + orbit * Math.sin(angle) - bitmap.height / 2;
    render.drawBitmap(bitmap, x | 0, y | 0);
}

export function drawScene(render, black, bodies, now) {
    const cx = render.width >> 1;
    const cy = render.height >> 1;

    const seconds = now.getSeconds();
    const minutes = now.getMinutes();
    const hours = now.getHours() % 12;

    render.begin();
    render.fillRectangle(black, 0, 0, render.width, render.height);

    render.drawBitmap(bodies.sun,
        cx - (bodies.sun.width >> 1),
        cy - (bodies.sun.height >> 1));

    place(render, bodies.earth, cx, cy, EARTH_ORBIT,
        angleOf((hours + minutes / 60) / 12));
    place(render, bodies.venus, cx, cy, VENUS_ORBIT,
        angleOf((minutes + seconds / 60) / 60));
    place(render, bodies.mercury, cx, cy, MERCURY_ORBIT,
        angleOf(seconds / 60));

    render.end();
}
