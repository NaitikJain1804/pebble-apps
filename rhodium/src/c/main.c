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
  #define COLOR_ENGRAVE   GColorBlack        // the numerals cut into the hands
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
#define G_HAND_W       18
#define G_HAND_W_MIN   13  // px
#define G_HAND_TAIL     9  // % of R, the counterweight behind the centre
#define G_BATON_LEN    15
#define G_BATON_W       6
#define G_BATON_W_MIN   5  // px
#define G_BATON_INSET   7
#define G_TICK_LEN      5
#define G_TICK_LEN_MIN  3  // px
#define G_TICK_INSET    3
#define G_TICK_IN_MIN   2  // px
#define G_TEXT_POS     62  // % of hand length
#define G_DIGIT_GAP     2  // px
// GEOMETRY_END

#define PCT(value, pct) (((value) * (pct)) / 100)

static int at_least(int value, int floor) {
  return value < floor ? floor : value;
}

// ---------------------------------------------------------------------------
// Numerals
//
// The numbers rotate with their hand, so they cannot be system-font text — the
// SDK only draws text upright. Each digit is a stroked outline on a 10x16 grid,
// drawn as line segments through the hand's own rotated frame. Format per
// digit: a stroke length, that many x,y pairs, repeated, terminated by 0.
// ---------------------------------------------------------------------------

// DIGITS_BEGIN (tools/preview.py parses these)
static const int8_t DIGIT_0[] = {11, 2,4, 1,7, 1,11, 2,14, 5,15, 8,14, 9,11, 9,7, 8,4, 5,3, 2,4, 0};
static const int8_t DIGIT_1[] = {3, 1,6, 5,3, 5,15, 0};
static const int8_t DIGIT_2[] = {9, 1,6, 2,4, 5,3, 8,4, 9,7, 8,10, 2,14, 1,15, 9,15, 0};
static const int8_t DIGIT_3[] = {11, 1,5, 3,3, 7,3, 9,5, 8,8, 5,9, 8,10, 9,12, 7,15, 3,15, 1,13, 0};
static const int8_t DIGIT_4[] = {3, 8,3, 1,12, 9,12, 2, 8,3, 8,15, 0};
static const int8_t DIGIT_5[] = {9, 8,3, 2,3, 2,8, 5,7, 8,9, 9,12, 7,15, 3,15, 1,13, 0};
static const int8_t DIGIT_6[] = {12, 8,4, 5,3, 2,5, 1,9, 1,12, 3,15, 6,15, 8,13, 8,11, 6,9, 3,9, 1,11, 0};
static const int8_t DIGIT_7[] = {3, 1,3, 9,3, 4,15, 0};
static const int8_t DIGIT_8[] = {7, 5,3, 2,4, 2,7, 5,9, 8,7, 8,4, 5,3, 7, 5,9, 2,11, 2,14, 5,15, 8,14, 8,11, 5,9, 0};
static const int8_t DIGIT_9[] = {12, 2,14, 5,15, 8,13, 9,9, 9,6, 7,3, 4,3, 2,5, 2,7, 4,9, 7,9, 9,7, 0};
// DIGITS_END

static const int8_t *const DIGITS[10] = {
  DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4,
  DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9,
};

// The strokes above occupy x 1..9 and y 3..15 of the grid; mapping from those
// bounds rather than the whole grid makes the digits fill the numeral height.
#define DIGIT_X0 1
#define DIGIT_Y0 3
#define DIGIT_W_UNITS 8
#define DIGIT_H_UNITS 12

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
  int numeral_h;
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

// The number engraved along a hand, rotated into the hand's frame. Digits are
// flipped end for end on the left half of the dial so they never read upside
// down — the same trick as lettering on a tyre wall.
static void draw_numerals(GContext *ctx, const Geom *g, int32_t angle, int along,
                          const char *text) {
  int digit_w = g->numeral_h * DIGIT_W_UNITS / DIGIT_H_UNITS;
  int advance = digit_w + G_DIGIT_GAP;
  int count = (int)strlen(text);
  int start = -(advance * count - G_DIGIT_GAP) / 2;
  bool flip = sin_lookup(angle) < 0;

  GPoint origin = frame_point(g->centre, angle, along, 0);
  graphics_context_set_stroke_color(ctx, COLOR_ENGRAVE);
  graphics_context_set_stroke_width(ctx, g->stroke);

  for (int i = 0; i < count; i++) {
    const int8_t *digit = DIGITS[text[i] - '0'];
    int base = start + i * advance;

    while (*digit) {
      int points = *digit++;
      GPoint previous = GPoint(0, 0);
      for (int p = 0; p < points; p++) {
        // Grid to hand frame: x runs along the hand, y across it.
        int u = base + ((digit[p * 2] - DIGIT_X0) * digit_w) / DIGIT_W_UNITS;
        int v = ((digit[p * 2 + 1] - DIGIT_Y0) * g->numeral_h) / DIGIT_H_UNITS
                - g->numeral_h / 2;
        GPoint at = frame_point(origin, angle, flip ? -u : u, flip ? v : -v);
        if (p > 0) {
          graphics_draw_line(ctx, previous, at);
        }
        previous = at;
      }
      digit += points * 2;
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
  g.numeral_h = g.hand_w - 5;
  g.stroke = (g.r >= 90) ? 2 : 1;
  g.shadow = (g.r >= 110) ? 2 : 1;

  draw_dial(ctx, &g, full);

  char hour_text[3];
  char minute_text[3];
  snprintf(hour_text, sizeof(hour_text), "%d", s_hour);
  snprintf(minute_text, sizeof(minute_text), "%02d", s_minute);

  // Both hands sit exactly on the marker they name: the hour hand on the hour
  // baton, not part-way to the next one, so hand and numeral always agree.
  int32_t hour_angle = (s_hour % 12) * TRIG_MAX_ANGLE / 12;
  int32_t minute_angle = s_minute * TRIG_MAX_ANGLE / 60;
  int hour_len = PCT(g.r, G_HOUR_LEN);
  int minute_len = PCT(g.r, G_MINUTE_LEN);

  // Hands, then cap, then both numbers: when the hands line up the minute hand
  // covers the hour hand, and the hour number has to survive on top of it.
  draw_hand(ctx, &g, hour_angle, hour_len);
  draw_hand(ctx, &g, minute_angle, minute_len);
  draw_cap(ctx, &g);
  draw_numerals(ctx, &g, hour_angle, PCT(hour_len, G_TEXT_POS), hour_text);
  draw_numerals(ctx, &g, minute_angle, PCT(minute_len, G_TEXT_POS), minute_text);
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
