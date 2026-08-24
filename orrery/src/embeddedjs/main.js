// Orrery — the inner solar system as a clock.
//
// Mercury marks seconds, Venus minutes, Earth hours, all orbiting the sun at
// the centre of a black sky.

import Poco from "commodetto/Poco";
import { makePalette, drawScene } from "planets";

const render = new Poco(screen);
const palette = makePalette(render);

function tick() {
    drawScene(render, palette, new Date());
}

tick();

// Mercury is the seconds hand, so this face redraws every second by design.
setInterval(tick, 1000);
