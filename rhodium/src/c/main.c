// Rhodium — an analogue face whose hands spell out the time they point at.
//
// The hour hand points at the hour marker and carries that number engraved
// along its length; the minute hand does the same with the minute. At 11:18
// the hour hand reads 11 and the minute hand reads 18.
//
// Salmon dial, rhodium-plated hands and applied markers: every metal part is
// filled in two tones with a warm shadow beneath it, lit consistently from the
// top left, which is what gives flat 64-colour graphics a machined finish.
//
// The dial follows the case: a circle on the round platforms, a rectangle on
// the rectangular ones, so markers always sit at the edge of the glass.

#include <pebble.h>

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------

#ifdef PBL_COLOR
  #define COLOR_DIAL      GColorMelon        // salmon
  #define COLOR_SHADOW    GColorSunsetOrange // the dial in shadow, one tone down
  #define COLOR_METAL     GColorLightGray    // rhodium in ambient light
  #define COLOR_METAL_LIT GColorWhite        // ... and where it catches the light
  #define COLOR_METAL_CUT GColorDarkGray     // outline, engraving
  #define COLOR_TRACK     GColorDarkGray     // minute hairlines
  #define COLOR_ENGRAVE   GColorBlack        // the lettering cut into the hands
#else
  #define COLOR_DIAL      GColorWhite
  #define COLOR_SHADOW    GColorBlack
  #define COLOR_METAL     GColorWhite
  #define COLOR_METAL_LIT GColorWhite
  #define COLOR_METAL_CUT GColorBlack
  #define COLOR_TRACK     GColorBlack
  #define COLOR_ENGRAVE   GColorBlack
#endif

// ---------------------------------------------------------------------------
// Proportions
//
// Everything scales off R, half the shorter screen dimension, so the same face
// lands on a 144x168 Pebble 2 and a 260x260 Round 2. Percentages, with pixel
// floors where a percentage would fall below what the display can resolve.
// ---------------------------------------------------------------------------

// GEOMETRY_BEGIN (tools/preview.py parses these)
#define G_MINUTE_LEN   80  // % of R
#define G_HOUR_LEN     52
#define G_HAND_W       10
#define G_HAND_W_MIN    9  // px
#define G_HAND_TAIL     9  // % of R, the counterweight behind the centre
#define G_BATON_LEN    15
#define G_BATON_W       6
#define G_BATON_W_MIN   5  // px
#define G_BATON_INSET   7
#define G_TICK_LEN      5
#define G_TICK_LEN_MIN  3  // px
#define G_TICK_INSET    3
#define G_TICK_IN_MIN   2  // px
#define G_TEXT_POS     58  // % of hand length, where the word sits
#define G_LETTER_GAP    1  // px
// GEOMETRY_END

#define PCT(value, pct) (((value) * (pct)) / 100)

static int at_least(int value, int floor) {
  return value < floor ? floor : value;
}

// ---------------------------------------------------------------------------
// Lettering
//
// The words rotate with their hand, so they cannot be system-font text — the
// SDK only draws text upright. Each letter is a stroked outline on a grid of
// x 1..7 by y 3..15, drawn as line segments through the hand's own rotated
// frame. Format: a stroke length, that many x,y pairs, repeated, then 0.
//
// Only the seventeen letters that spell out numbers are here.
// ---------------------------------------------------------------------------

// GLYPHS_BEGIN (tools/preview.py parses these)
static const int8_t GLYPH_E[] = {4, 7,3, 1,3, 1,15, 7,15, 2, 1,9, 5,9, 0};
static const int8_t GLYPH_F[] = {3, 7,3, 1,3, 1,15, 2, 1,9, 5,9, 0};
static const int8_t GLYPH_G[] = {9, 7,5, 4,3, 2,5, 1,9, 2,13, 4,15, 6,14, 7,11, 4,11, 0};
static const int8_t GLYPH_H[] = {2, 1,3, 1,15, 2, 7,3, 7,15, 2, 1,9, 7,9, 0};
static const int8_t GLYPH_I[] = {2, 1,3, 7,3, 2, 4,3, 4,15, 2, 1,15, 7,15, 0};
static const int8_t GLYPH_L[] = {3, 1,3, 1,15, 7,15, 0};
static const int8_t GLYPH_N[] = {4, 1,15, 1,3, 7,15, 7,3, 0};
static const int8_t GLYPH_O[] = {7, 4,3, 1,6, 1,12, 4,15, 7,12, 7,6, 4,3, 0};
static const int8_t GLYPH_R[] = {6, 1,15, 1,3, 5,3, 7,5, 5,8, 1,8, 2, 4,8, 7,15, 0};
static const int8_t GLYPH_S[] = {8, 7,4, 4,3, 2,4, 2,7, 6,10, 6,13, 4,15, 1,14, 0};
static const int8_t GLYPH_T[] = {2, 1,3, 7,3, 2, 4,3, 4,15, 0};
static const int8_t GLYPH_U[] = {5, 1,3, 1,12, 4,15, 7,12, 7,3, 0};
static const int8_t GLYPH_V[] = {3, 1,3, 4,15, 7,3, 0};
static const int8_t GLYPH_W[] = {5, 1,3, 2,15, 4,8, 6,15, 7,3, 0};
static const int8_t GLYPH_X[] = {2, 1,3, 7,15, 2, 7,3, 1,15, 0};
static const int8_t GLYPH_Y[] = {3, 1,3, 4,9, 7,3, 2, 4,9, 4,15, 0};
static const int8_t GLYPH_Z[] = {4, 1,3, 7,3, 1,15, 7,15, 0};
// GLYPHS_END

// The strokes occupy x 1..7 and y 3..15; mapping from those bounds rather than
// the whole grid makes the letters fill the height they are given.
#define GLYPH_X0 1
#define GLYPH_Y0 3
#define GLYPH_W_UNITS 6
#define GLYPH_H_UNITS 12

static const int8_t *glyph(char c) {
  switch (c) {
    case 'E': return GLYPH_E;
    case 'F': return GLYPH_F;
    case 'G': return GLYPH_G;
    case 'H': return GLYPH_H;
    case 'I': return GLYPH_I;
    case 'L': return GLYPH_L;
    case 'N': return GLYPH_N;
    case 'O': return GLYPH_O;
    case 'R': return GLYPH_R;
    case 'S': return GLYPH_S;
    case 'T': return GLYPH_T;
    case 'U': return GLYPH_U;
    case 'V': return GLYPH_V;
    case 'W': return GLYPH_W;
    case 'X': return GLYPH_X;
    case 'Y': return GLYPH_Y;
    case 'Z': return GLYPH_Z;
    default: return NULL;  // the space between TWENTY and ONE
  }
}

// WORDS_BEGIN (tools/preview.py parses these)
static const char *const UNITS[20] = {
  "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT",
  "NINE", "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN",
  "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN",
};
static const char *const TENS[6] = {
  "", "", "TWENTY", "THIRTY", "FORTY", "FIFTY",
};
// WORDS_END

// 0..59 spelled out: "EIGHTEEN", "TWENTY THREE", "FORTY".
static void spell(char *out, size_t size, int value) {
  if (value < 20) {
    snprintf(out, size, "%s", UNITS[value]);
  } else if (value % 10 == 0) {
    snprintf(out, size, "%s", TENS[value / 10]);
  } else {
    snprintf(out, size, "%s %s", TENS[value / 10], UNITS[value % 10]);
  }
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static Window *s_window;
static Layer *s_canvas;

static int s_hour;    // 1..12
static int s_minute;  // 0..59

typedef struct {
  GPoint centre;
  int half_w;     // to the dial edge, horizontally
  int half_h;
  int r;          // the smaller of the two: everything scales off this
  int hand_w;
  int letter_h;
  int stroke;     // engraving weight
  int shadow;     // how far the metal floats above the dial
} Geom;

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

// Where the ray at this angle meets the edge of the dial. On a rectangular
// screen that is farther away at the corners than at the sides, which is what
// makes the markers sit along the case rather than on an inscribed circle.
static int dial_radius(const Geom *g, int32_t angle) {
#ifdef PBL_ROUND
  (void)angle;
  return g->r;
#else
  int32_t s = sin_lookup(angle);
  int32_t c = cos_lookup(angle);
  if (s < 0) { s = -s; }
  if (c < 0) { c = -c; }

  int32_t r = g->r * 4;  // any bound past the corner distance
  if (s > 0) {
    int32_t limit = (g->half_w * TRIG_MAX_RATIO) / s;
    if (limit < r) { r = limit; }
  }
  if (c > 0) {
    int32_t limit = (g->half_h * TRIG_MAX_RATIO) / c;
    if (limit < r) { r = limit; }
  }
  return (int)r;
#endif
}

// A point at (along, across) in the frame of a hand or marker at this angle:
// `along` runs outward from the centre, `across` to its right.
static GPoint frame_point(GPoint origin, int32_t angle, int along, int across) {
  int32_t s = sin_lookup(angle);
  int32_t c = cos_lookup(angle);
  return GPoint(origin.x + (int16_t)((along * s + across * c) / TRIG_MAX_RATIO),
                origin.y + (int16_t)((across * s - along * c) / TRIG_MAX_RATIO));
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static void fill_poly(GContext *ctx, GPoint *points, int count, GColor color) {
  GPathInfo info = { .num_points = (uint32_t)count, .points = points };
  GPath *path = gpath_create(&info);
  if (!path) {
    return;
  }
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void outline_poly(GContext *ctx, GPoint *points, int count, GColor color) {
  GPathInfo info = { .num_points = (uint32_t)count, .points = points };
  GPath *path = gpath_create(&info);
  if (!path) {
    return;
  }
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
}

static void offset_poly(GPoint *dst, const GPoint *src, int count, int by) {
  for (int i = 0; i < count; i++) {
    dst[i] = GPoint(src[i].x + by, src[i].y + by);
  }
}

// Which side of a part at this angle faces the light. The key is top left, so
// the lit edge is the one whose outward normal points up and to the left.
static int lit_side(int32_t angle) {
  return (cos_lookup(angle) + sin_lookup(angle) > 0) ? -1 : 1;
}

// A piece of applied metal: warm shadow beneath, body in two tones split down
// its axis, cut edge around it. `shape` is given as (along, across) pairs and
// must run from the lit edge to the shaded edge so the halves come out right.
static void draw_applied(GContext *ctx, const Geom *g, GPoint origin,
                         int32_t angle, const int *shape, int count) {
  GPoint body[8];
  GPoint half[8];
  GPoint shadow[8];
  int lit = lit_side(angle);

  for (int i = 0; i < count; i++) {
    body[i] = frame_point(origin, angle, shape[i * 2], shape[i * 2 + 1] * lit);
    // The lit half is the shape with its shaded edge folded onto the axis.
    int across = shape[i * 2 + 1] * lit;
    half[i] = frame_point(origin, angle, shape[i * 2],
                          (across * lit > 0) ? 0 : across);
  }

  offset_poly(shadow, body, count, g->shadow);
  fill_poly(ctx, shadow, count, COLOR_SHADOW);
  fill_poly(ctx, body, count, COLOR_METAL);
  fill_poly(ctx, half, count, COLOR_METAL_LIT);
  outline_poly(ctx, body, count, COLOR_METAL_CUT);
}

static void draw_baton(GContext *ctx, const Geom *g, int32_t angle, int across) {
  int outer = dial_radius(g, angle) - at_least(PCT(g->r, G_TICK_INSET), G_TICK_IN_MIN)
              - at_least(PCT(g->r, G_TICK_LEN), G_TICK_LEN_MIN)
              - PCT(g->r, G_BATON_INSET) / 2;
  int inner = outer - PCT(g->r, G_BATON_LEN);
  int w = at_least(PCT(g->r, G_BATON_W), G_BATON_W_MIN);

  // Lit edge first, then round the far end, so draw_applied can split it.
  const int shape[] = {
    inner, -w / 2,
    outer, -w / 2,
    outer, w / 2,
    inner, w / 2,
  };
  GPoint origin = frame_point(g->centre, angle, 0, across);
  draw_applied(ctx, g, origin, angle, shape, 4);
}

static void draw_dial(GContext *ctx, const Geom *g, GRect bounds) {
  graphics_context_set_fill_color(ctx, COLOR_DIAL);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Minute track: sixty hairlines pinned to the edge of the dial.
  int tick_len = at_least(PCT(g->r, G_TICK_LEN), G_TICK_LEN_MIN);
  int tick_inset = at_least(PCT(g->r, G_TICK_INSET), G_TICK_IN_MIN);
  graphics_context_set_stroke_color(ctx, COLOR_TRACK);
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i < 60; i++) {
    int32_t angle = i * TRIG_MAX_ANGLE / 60;
    int outer = dial_radius(g, angle) - tick_inset;
    int len = (i % 5 == 0) ? tick_len * 3 / 2 : tick_len;
    graphics_draw_line(ctx, frame_point(g->centre, angle, outer - len, 0),
                       frame_point(g->centre, angle, outer, 0));
  }

  // Applied hour batons, doubled at twelve so the dial has an up.
  for (int i = 0; i < 12; i++) {
    int32_t angle = i * TRIG_MAX_ANGLE / 12;
    if (i == 0) {
      int w = at_least(PCT(g->r, G_BATON_W), G_BATON_W_MIN);
      draw_baton(ctx, g, angle, -w);
      draw_baton(ctx, g, angle, w);
    } else {
      draw_baton(ctx, g, angle, 0);
    }
  }
}

static void draw_hand(GContext *ctx, const Geom *g, int32_t angle, int len) {
  int w = g->hand_w;
  int tail = PCT(g->r, G_HAND_TAIL);
  const int shape[] = {
    -tail,        -w / 2,
    len * 88 / 100, -w / 2,
    len,          0,
    len * 88 / 100, w / 2,
    -tail,        w / 2,
  };
  draw_applied(ctx, g, g->centre, angle, shape, 5);
}

// The word engraved along a hand, rotated into the hand's frame and flipped
// end for end on the left half of the dial, so the letters always stand up
// rather than reading upside down. The word is nudged inboard if it would
// otherwise run off the tip.
static void draw_word(GContext *ctx, const Geom *g, int32_t angle, int len,
                      const char *text) {
  int letter_w = (g->letter_h * GLYPH_W_UNITS) / GLYPH_H_UNITS;
  int advance = letter_w + G_LETTER_GAP;
  int count = (int)strlen(text);
  int total = advance * count - G_LETTER_GAP;

  int along = PCT(len, G_TEXT_POS);
  int furthest = PCT(len, 92) - total / 2;
  if (along > furthest) {
    along = furthest;
  }
  int start = -total / 2;
  bool flip = sin_lookup(angle) < 0;

  GPoint origin = frame_point(g->centre, angle, along, 0);
  graphics_context_set_stroke_color(ctx, COLOR_ENGRAVE);
  // Hairline lettering: a heavier stroke at this size closes the counters.
  graphics_context_set_stroke_width(ctx, 1);

  for (int i = 0; i < count; i++) {
    const int8_t *strokes = glyph(text[i]);
    if (!strokes) {
      continue;
    }
    int base = start + i * advance;
    while (*strokes) {
      int points = *strokes++;
      GPoint previous = GPoint(0, 0);
      for (int n = 0; n < points; n++) {
        // Grid to hand frame: x runs along the hand, y across it.
        int u = base + ((strokes[n * 2] - GLYPH_X0) * letter_w) / GLYPH_W_UNITS;
        int v = ((strokes[n * 2 + 1] - GLYPH_Y0) * g->letter_h) / GLYPH_H_UNITS
                - g->letter_h / 2;
        GPoint at = frame_point(origin, angle, flip ? -u : u, flip ? v : -v);
        if (n > 0) {
          graphics_draw_line(ctx, previous, at);
        }
        previous = at;
      }
      strokes += points * 2;
    }
  }
}

static void draw_cap(GContext *ctx, const Geom *g) {
  int r = g->hand_w / 2 + 1;
  graphics_context_set_fill_color(ctx, COLOR_SHADOW);
  graphics_fill_circle(ctx, GPoint(g->centre.x + g->shadow, g->centre.y + g->shadow), r);
  graphics_context_set_fill_color(ctx, COLOR_METAL_CUT);
  graphics_fill_circle(ctx, g->centre, r);
  graphics_context_set_fill_color(ctx, COLOR_METAL_LIT);
  graphics_fill_circle(ctx, GPoint(g->centre.x - 1, g->centre.y - 1), r - 3);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect full = layer_get_bounds(layer);
  GRect bounds = layer_get_unobstructed_bounds(layer);

  Geom g;
  g.centre = grect_center_point(&bounds);
  g.half_w = bounds.size.w / 2 - 2;
  g.half_h = bounds.size.h / 2 - 2;
  g.r = (g.half_w < g.half_h) ? g.half_w : g.half_h;
  g.hand_w = at_least(PCT(g.r, G_HAND_W), G_HAND_W_MIN);
  g.letter_h = g.hand_w - 2;
  g.stroke = (g.r >= 90) ? 2 : 1;
  g.shadow = (g.r >= 110) ? 2 : 1;

  draw_dial(ctx, &g, full);

  char hour_text[16];
  char minute_text[16];
  spell(hour_text, sizeof(hour_text), s_hour);
  spell(minute_text, sizeof(minute_text), s_minute);

  // Both hands sit exactly on the marker they name: the hour hand on the hour
  // baton, not part-way to the next one, so hand and word always agree.
  int32_t hour_angle = (s_hour % 12) * TRIG_MAX_ANGLE / 12;
  int32_t minute_angle = s_minute * TRIG_MAX_ANGLE / 60;
  int hour_len = PCT(g.r, G_HOUR_LEN);
  int minute_len = PCT(g.r, G_MINUTE_LEN);

  // Hands, then cap, then both words: when the hands line up the minute hand
  // covers the hour hand, and the hour word has to survive on top of it.
  draw_hand(ctx, &g, hour_angle, hour_len);
  draw_hand(ctx, &g, minute_angle, minute_len);
  draw_cap(ctx, &g);
  draw_word(ctx, &g, hour_angle, hour_len, hour_text);
  draw_word(ctx, &g, minute_angle, minute_len, minute_text);
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

static void read_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_hour = t->tm_hour % 12;
  if (s_hour == 0) {
    s_hour = 12;
  }
  s_minute = t->tm_min;
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  read_time();
  layer_mark_dirty(s_canvas);
}

static void unobstructed_change(AnimationProgress progress, void *context) {
  layer_mark_dirty(s_canvas);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init(void) {
  read_time();

  s_window = window_create();
  window_set_background_color(s_window, COLOR_DIAL);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers){
    .change = unobstructed_change,
  }, NULL);
}

static void deinit(void) {
  unobstructed_area_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
