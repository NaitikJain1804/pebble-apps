#!/usr/bin/env python3
"""Render the sun and planets to Pebble bitmap resources.

Each body is rendered as an orthographic sphere: real equirectangular NASA-
derived maps are projected onto the sphere, lit, supersampled 8x for a smooth
limb, then quantised to Pebble's 64-colour grid (channels 0/85/170/255).

Sprites are square with black corners. The watchface draws them on a black
background and the layout keeps sprites from overlapping, so the corners are
invisible and no alpha channel is needed.

    python3 tools/render_bodies.py <texture-dir> <output-dir>

Texture maps come from planetpixelemporium.com (via jeromeetienne/threex.planets)
and are derived from NASA imagery.
"""
import sys
import os
import numpy as np
from PIL import Image

SS = 8  # supersampling factor


def load(path, size=None):
    im = Image.open(path)
    if size:
        im = im.resize(size, Image.LANCZOS)
    return np.asarray(im).astype(np.float64) / 255.0


def find_map(tex_dir, name, kind):
    """Texture names are inconsistent: mercurymap.jpg but earthmap1k.jpg."""
    for suffix in ("", "1k"):
        path = os.path.join(tex_dir, "{}{}{}.jpg".format(name, kind, suffix))
        if os.path.exists(path):
            return path
    return None


def sphere_grid(px):
    """Unit-sphere normals for a px*SS square, plus the inside-disc mask."""
    n = px * SS
    # Sample pixel centres over [-1, 1]; the disc fills the sprite minus a 1px
    # margin so the limb never touches the sprite edge.
    span = px / (px - 2.0)
    t = (np.arange(n) + 0.5) / n * 2.0 - 1.0
    x = t[None, :] * span
    y = t[:, None] * span
    r2 = x * x + y * y
    mask = r2 <= 1.0
    z = np.sqrt(np.clip(1.0 - r2, 0.0, None))
    return x, y, z, mask, r2


def uv_from_normal(x, y, z, tilt_deg, lon0_deg):
    """Texture coordinates for each surface point, with an axial tilt."""
    t = np.radians(tilt_deg)
    # Axial tilt leans the pole within the image plane. Rotating about the
    # screen-x axis instead tips the pole toward the camera, which turns the
    # view into a look down at the arctic.
    rx = x * np.cos(t) - y * np.sin(t)
    ry = x * np.sin(t) + y * np.cos(t)
    lat = np.arcsin(np.clip(ry, -1.0, 1.0))
    lon = np.arctan2(rx, z) + np.radians(lon0_deg)
    # Equirectangular maps put longitude 0 at the middle of the image, not the
    # left edge: without the half-turn, "20E" lands in the mid-Pacific.
    u = (0.5 + lon / (2 * np.pi)) % 1.0
    v = 0.5 - lat / np.pi
    return u, v


def sample(tex, u, v):
    h, w = tex.shape[:2]
    xi = np.clip((u * w).astype(np.int32), 0, w - 1)
    yi = np.clip((v * h).astype(np.int32), 0, h - 1)
    return tex[yi, xi]


def bump_normal(bump, u, v, x, y, z, strength):
    """Perturb the surface normal by the local slope of a bump map."""
    h, w = bump.shape
    du = 1.0 / w
    dv = 1.0 / h
    gx = sample(bump, (u + du) % 1.0, v) - sample(bump, (u - du) % 1.0, v)
    gy = sample(bump, u, np.clip(v + dv, 0, 1)) - sample(bump, u, np.clip(v - dv, 0, 1))
    nx = x - strength * gx
    ny = y - strength * gy
    nz = z
    ln = np.sqrt(nx * nx + ny * ny + nz * nz) + 1e-9
    return nx / ln, ny / ln, nz / ln


def downsample(img, px):
    """Box-filter SS x SS blocks — this is what antialiases the limb."""
    return img.reshape(px, SS, px, SS, 3).mean(axis=(1, 3))


def to_sprite(rgb, mask, px):
    """Composite onto black and reduce to sprite resolution."""
    out = np.where(mask[..., None], rgb, 0.0)
    return np.clip(downsample(out, px), 0.0, 1.0)


# --- bodies -----------------------------------------------------------------

def render_planet(tex_dir, name, px, tilt, lon0, bump_strength, light, ambient,
                  gamma, stops, contrast=1.0, lift=0.0):
    """A single-hue body: Mercury or Venus.

    The map's brightness drives a designed colour ramp. Grading the photograph
    directly leaves these two muddy once only four levels per channel survive,
    because neither has enough colour of its own to survive the reduction.
    """
    x, y, z, mask, _ = sphere_grid(px)
    n = px * SS

    u, v = uv_from_normal(x, y, z, tilt, lon0)
    u = np.broadcast_to(u, (n, n))
    v = np.broadcast_to(v, (n, n))
    base = sample(load(find_map(tex_dir, name, "map")), u, v)

    nx, ny, nz = bump_normal(load(find_map(tex_dir, name, "bump")),
                             u, v, x, y, z, bump_strength)

    lx, ly, lz = light
    lam = np.clip(nx * lx + ny * ly + nz * lz, 0.0, None)
    lit = ambient + (1.0 - ambient) * lam ** gamma
    t = np.clip(base.mean(axis=2) * contrast + lift, 0.0, 1.0) * lit
    return to_sprite(ramp(np.clip(t, 0.0, 1.0), stops), mask, px)


# Per-material shade tiers, all exact palette colours. Earth is drawn by
# picking one of these rather than by multiplying a colour by a light value:
# with four levels per channel, arbitrary products land on colours like pink
# and teal that belong to no material.
SHADES = {
    "ocean": [(0.00, 0.00, 0.67), (0.00, 0.33, 0.67), (0.33, 0.67, 1.00)],
    "veg": [(0.00, 0.33, 0.00), (0.00, 0.67, 0.33), (0.33, 0.67, 0.33)],
    "arid": [(0.67, 0.33, 0.00), (1.00, 0.67, 0.33), (1.00, 0.67, 0.33)],
    "ice": [(0.67, 0.67, 0.67), (1.00, 1.00, 1.00), (1.00, 1.00, 1.00)],
}


def mode_downsample(index, px, depth):
    """Downsample by majority vote instead of averaging.

    Averaging blends across material boundaries, and the blend quantises to a
    colour from neither side. Taking the most common material in each block
    keeps coastlines clean.
    """
    blocks = index.reshape(px, SS, px, SS).transpose(0, 2, 1, 3).reshape(px * px, SS * SS)
    counts = np.zeros((px * px, depth), dtype=np.int32)
    for d in range(depth):
        counts[:, d] = (blocks == d).sum(axis=1)
    return counts.argmax(axis=1).reshape(px, px)


def render_earth(tex_dir, px, tilt, lon0, light, ambient, gamma, cloud_cover):
    """Earth, drawn by classifying the map rather than grading it.

    Ocean, vegetation, desert and ice are four distinct materials. Grading the
    photograph as one image cannot keep them apart at 32px on a four-level
    palette — whatever makes the ocean read blue drags the continents with it.
    Deciding the material per pixel and assigning it a designed colour keeps
    them separate.
    """
    x, y, z, mask, _ = sphere_grid(px)
    n = px * SS
    u, v = uv_from_normal(x, y, z, tilt, lon0)
    u = np.broadcast_to(u, (n, n))
    v = np.broadcast_to(v, (n, n))
    m = sample(load(find_map(tex_dir, "earth", "map")), u, v)

    red, green, blue = m[..., 0], m[..., 1], m[..., 2]
    lum = m.mean(axis=2)

    lat = np.abs((0.5 - v) * 180.0)
    ocean = (blue > red * 1.12) & (blue > green * 1.02)
    # Ice only where it belongs: bright ground at high latitude. A brightness
    # test alone also catches deserts and cloud-tops.
    ice = (lum > 0.55) & (lat > 58.0)

    vegetated = green >= red * 0.98

    # earthcloudmaptrans is the transparency mask; earthcloudmap is the cloud
    # image itself and is bright almost everywhere, which whites out the globe.
    clouds = sample(load(os.path.join(tex_dir, "earthcloudmaptrans.jpg")), u, v)
    if clouds.ndim == 3:
        clouds = clouds.mean(axis=2)
    # cloud_cover is the fraction of the globe the densest clouds cover.
    cloudy = clouds > np.quantile(clouds, 1.0 - cloud_cover)

    material = np.where(ocean, 0, np.where(vegetated, 1, 2))
    material = np.where(ice, 3, material)
    material = np.where(cloudy, 3, material)

    # Lighting picks a shade tier rather than scaling the colour.
    nx, ny, nz = bump_normal(load(find_map(tex_dir, "earth", "bump")),
                             u, v, x, y, z, 0.12)
    lx, ly, lz = light
    lam = np.clip(nx * lx + ny * ly + nz * lz, 0.0, None)
    lit = ambient + (1.0 - ambient) * lam ** gamma
    # Keep the lit tier rare: a broad bright zone reads as a painted lens on
    # the ocean rather than as illumination.
    tier = np.where(lit < 0.62, 0, 1)
    # Shelf seas are genuinely brighter than open ocean.
    tier = np.where(ocean & (lum > 0.33), 2, tier)
    tier = np.where((1.0 - z) > 0.88, np.minimum(tier, 1), tier)

    index = material * 3 + tier
    palette = np.array([SHADES[k][i] for k in ("ocean", "veg", "arid", "ice")
                        for i in range(3)])

    small = mode_downsample(index, px, len(palette))
    rgb = palette[small]
    # Coverage of the disc in each block antialiases the limb against the sky.
    coverage = mask.astype(np.float64).reshape(px, SS, px, SS).mean(axis=(1, 3))
    return np.clip(rgb * coverage[..., None], 0.0, 1.0)


# Sunspot groups as (latitude, longitude, umbra radius, penumbra radius), all
# in degrees. Placed by hand: the texture map has no spots big enough to
# survive at this size, and spots are what make the sun read as the sun.
SPOT_GROUPS = [
    (18, 28, 4.5, 10.0),
    (12, 41, 2.8, 6.8),
    (-22, 8, 3.8, 9.0),
    (-14, 62, 2.6, 6.2),
    (30, 74, 2.2, 5.4),
]


def sunspots(lat, lon):
    """Darkening from spot groups, in sphere coordinates so they foreshorten
    toward the limb the way real spots do."""
    total = np.zeros(lat.shape)
    for slat, slon, umbra, penumbra in SPOT_GROUPS:
        a, b = np.radians(slat), np.radians(slon)
        # Great-circle distance from the spot centre.
        d = np.arccos(np.clip(
            np.sin(a) * np.sin(lat) + np.cos(a) * np.cos(lat) * np.cos(lon - b),
            -1.0, 1.0))
        d = np.degrees(d)
        pen = np.clip((penumbra - d) / (penumbra - umbra), 0.0, 1.0) * 0.26
        umb = np.clip((umbra - d) / umbra * 3.0, 0.0, 1.0) * 0.52
        total = np.maximum(total, pen + umb)
    return total


def render_sun(tex_dir, px, disc_px):
    """Emissive disc with real solar limb darkening, plus a corona glow."""
    n = px * SS
    span = px / disc_px
    t = (np.arange(n) + 0.5) / n * 2.0 - 1.0
    x = t[None, :] * span
    y = t[:, None] * span
    r = np.sqrt(x * x + y * y)
    disc = r <= 1.0
    z = np.sqrt(np.clip(1.0 - r * r, 0.0, None))

    surface = load(os.path.join(tex_dir, "sunmap.jpg"))
    u, v = uv_from_normal(x, y, z, 0.0, 40.0)
    lat = (0.5 - v) * np.pi
    # uv_from_normal centres longitude 0 at u=0.5; undo that to get true
    # longitude, or the spot groups land on the far side of the sun.
    lon = (u - 0.5) * 2.0 * np.pi
    # Magnify the map: at 56px across, granulation at true scale averages away
    # to nothing. Showing a smaller patch keeps it as visible surface texture.
    zoom = 0.30
    uz = np.broadcast_to(0.52 + (u - 0.52) * zoom, (n, n))
    vz = np.broadcast_to(0.50 + (v - 0.50) * zoom, (n, n))
    tex = sample(surface, uz, vz)

    # The photosphere is close to uniform brightness. Building it as a broad
    # radial gradient forces the quantiser to dither across the whole disc,
    # which reads as noise; a mostly flat disc with real sunspots does not.
    lum = tex.mean(axis=2)
    granulation = (lum - 0.62) * 0.05
    # Sit the flat part of the disc on a ramp stop, so the quantiser has
    # nothing to dither across most of the photosphere.
    t = 0.92 + granulation - sunspots(lat, lon)
    # Classic limb-darkening law I(mu) = 1 - u(1 - mu), plus a hotter centre.
    t = t * (1.0 - 0.30 * (1.0 - z)) + 0.04 * np.clip(1.0 - r * 1.6, 0.0, 1.0)
    # Ramp stops sit on exact palette colours, so most of the disc quantises
    # cleanly and dithering only has to smooth the transitions between them.
    rgb = ramp(np.clip(t, 0.0, 1.0), [
        (0.00, (0.333, 0.000, 0.000)),
        (0.26, (0.667, 0.000, 0.000)),
        (0.46, (1.000, 0.333, 0.000)),
        (0.63, (1.000, 0.667, 0.000)),
        (0.80, (1.000, 1.000, 0.333)),
        (0.92, (1.000, 1.000, 0.667)),
        (1.00, (1.000, 1.000, 1.000)),
    ])

    # Corona: falls off outside the disc, warm and asymmetric enough to read as
    # a real atmosphere rather than a ring.
    d = np.clip(r - 1.0, 0.0, None)
    glow = np.exp(-d / 0.055) * 0.95 + np.exp(-d / 0.15) * 0.16
    ang = np.arctan2(y, x)
    glow *= 1.0 + 0.16 * np.sin(ang * 5.0) + 0.09 * np.sin(ang * 9.0 + 1.3)
    # Force the glow to reach true black well inside the sprite edge. Left to
    # fade asymptotically, the tail dithers into a visible speckled square.
    glow *= np.clip((0.30 - d) / 0.12, 0.0, 1.0)
    corona = np.clip(glow, 0.0, 1.0)[..., None] * np.array([1.0, 0.42, 0.06])

    out = np.where(disc[..., None], rgb, corona)
    out = np.clip(downsample(out, px), 0.0, 1.0)
    # Anything this dark would only dither into stray dots against the sky.
    out[out.max(axis=2) < 0.09] = 0.0
    return out


# --- output -----------------------------------------------------------------

LEVELS = np.array([0, 85, 170, 255], dtype=np.float64)


def ramp(t, stops):
    """Map a 0..1 brightness onto a designed colour ramp.

    Grading a photographic map channel-by-channel cannot keep a body on a
    believable palette once only four levels per channel survive. Driving a
    hand-picked ramp with the map's brightness does: the texture still supplies
    the detail, but every value lands somewhere intentional.
    """
    pos = np.array([s[0] for s in stops])
    cols = np.array([s[1] for s in stops], dtype=np.float64)
    out = np.empty(t.shape + (3,))
    for c in range(3):
        out[..., c] = np.interp(t, pos, cols[:, c])
    return out


def quantise(arr, dither):
    """Reduce to the 64-colour Pebble grid.

    Dithering is a per-body choice. On the sun's broad radial gradient,
    undithered output contours into visible rings; on a 20-30px textured
    planet, error diffusion just reads as salt-and-pepper noise.
    """
    work = arr * 255.0
    if not dither:
        return LEVELS[np.abs(work[..., None] - LEVELS).argmin(axis=-1)].astype(np.uint8)

    h, w, _ = work.shape
    for yy in range(h):
        for xx in range(w):
            old = work[yy, xx].copy()
            new = LEVELS[np.abs(LEVELS[None, :] - old[:, None]).argmin(axis=1)]
            work[yy, xx] = new
            err = old - new
            if xx + 1 < w:
                work[yy, xx + 1] += err * 7 / 16
            if yy + 1 < h:
                if xx > 0:
                    work[yy + 1, xx - 1] += err * 3 / 16
                work[yy + 1, xx] += err * 5 / 16
                if xx + 1 < w:
                    work[yy + 1, xx + 1] += err * 1 / 16
    return np.clip(work, 0, 255).astype(np.uint8)


def save(arr, path, dither=False):
    Image.fromarray(quantise(arr, dither), "RGB").save(path)
    print("wrote", path, arr.shape[1], "x", arr.shape[0])


def main():
    tex_dir, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)

    save(render_sun(tex_dir, 64, 48), os.path.join(out_dir, "sun.png"),
         dither=True)

    # Light slightly off-axis: enough to raise crater relief, not enough to cut
    # a terminator across the disc.
    # Mercury is nearly colourless: pull the map's brown cast out so it reads
    # as grey rock, and lean on bump relief for the craters.
    save(render_planet(tex_dir, "mercury", 20, 0, 200, 0.70,
                       (-0.45, 0.36, 0.82), 0.50, 0.85,
                       contrast=1.05, lift=0.13, stops=[
                           (0.00, (0.13, 0.12, 0.12)),
                           (0.40, (0.44, 0.43, 0.41)),
                           (0.70, (0.72, 0.71, 0.68)),
                           (1.00, (1.00, 0.99, 0.96)),
                       ]),
         os.path.join(out_dir, "mercury.png"))

    # Venus in visible light is featureless cream, not the orange of the map.
    save(render_planet(tex_dir, "venus", 26, 0, 90, 0.30,
                       (-0.30, 0.26, 0.92), 0.20, 0.70,
                       contrast=0.80, lift=0.26, stops=[
                           (0.00, (0.333, 0.333, 0.000)),
                           (0.28, (0.667, 0.667, 0.333)),
                           (0.52, (1.000, 1.000, 0.667)),
                           (1.00, (1.000, 1.000, 1.000)),
                       ]),
         os.path.join(out_dir, "venus.png"))

    # Longitude 20E puts Africa and Europe on the visible face.
    save(render_earth(tex_dir, 30, 23.4, 20, (-0.30, 0.28, 0.91), 0.30, 0.70,
                      cloud_cover=0.17),
         os.path.join(out_dir, "earth.png"))


if __name__ == "__main__":
    main()
