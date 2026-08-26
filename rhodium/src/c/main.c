// Rhodium — an analogue face whose hands spell out the time they point at.
//
// The hour hand carries the hour on a pill-shaped plaque near its tip; the
// minute hand carries the minute. Both numbers stay upright (the SDK cannot
// rotate text), so the face reads at a glance from any hand position.
//
// Salmon dial, rhodium-plated hands. Builds for every Pebble platform; the
// layout table below picks sizes from the shortest screen dimension.

#include <pebble.h>

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------

#ifdef PBL_COLOR
  #define COLOR_DIAL        GColorMelon        // soft salmon
  #define COLOR_DIAL_DEEP   GColorSunsetOrange // deeper salmon, chapter ring
  #define COLOR_METAL       GColorWhite        // polished rhodium
  #define COLOR_METAL_SHADE GColorLightGray    // bevel, brushed shade
  #define COLOR_METAL_EDGE  GColorDarkGray     // outline
  #define COLOR_NUMERAL     GColorBlack        // engraved numerals
  #define COLOR_TICK        COLOR_METAL        // markers, also rhodium
  #define COLOR_TICK_EDGE   COLOR_METAL_EDGE
#else
  #define COLOR_DIAL        GColorWhite
  #define COLOR_DIAL_DEEP   GColorBlack
  #define COLOR_METAL       GColorWhite
  #define COLOR_METAL_SHADE GColorWhite
  #define COLOR_METAL_EDGE  GColorBlack
  #define COLOR_NUMERAL     GColorBlack
  // On a white dial the markers have to be solid, or they read as outlines.
  #define COLOR_TICK        GColorBlack
  #define COLOR_TICK_EDGE   GColorBlack
#endif

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

typedef struct {
  int min_dim;      // applies when the shorter screen side is at least this
  int hour_r;       // distance from centre to the hour plaque
  int minute_r;     // ... and to the minute plaque
  int pill_w;       // plaque size
  int pill_h;
  int shaft_w;      // hand shaft width
  int tick_len;     // hour tick length
  int tick_w;       // hour tick width
  int text_dy;      // vertical nudge for the numerals
  int minute_pips;  // draw the 60-minute track?
  const char *font;
} Tier;

// LAYOUT_TABLE_BEGIN (tools/preview.py parses this block)
static const Tier TIERS[] = {
  // gabbro 260x260
  { 240, 44, 92, 52, 34, 9, 14, 5, -2, 1, FONT_KEY_DROID_SERIF_28_BOLD },
  // emery 200x228
  { 190, 32, 72, 48, 32, 7, 11, 4, -2, 1, FONT_KEY_DROID_SERIF_28_BOLD },
  // chalk 180x180
  { 170, 30, 62, 36, 26, 6,  9, 3, -1, 1, FONT_KEY_GOTHIC_18_BOLD },
  // aplite / basalt / diorite / flint 144x168, and Quick View
  {   0, 24, 52, 32, 22, 5,  8, 3, -1, 0, FONT_KEY_GOTHIC_18_BOLD },
};
// LAYOUT_TABLE_END

static const Tier *pick_tier(int min_dim) {
  for (unsigned i = 0; i < ARRAY_LENGTH(TIERS); i++) {
    if (min_dim >= TIERS[i].min_dim) {
      return &TIERS[i];
    }
  }
  return &TIERS[ARRAY_LENGTH(TIERS) - 1];
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static Window *s_window;
static Layer *s_canvas;

static int s_hour;    // 1..12
static int s_minute;  // 0..59

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static GPoint polar(GPoint centre, int32_t angle, int r) {
  return GPoint(centre.x + (int16_t)((sin_lookup(angle) * r) / TRIG_MAX_RATIO),
                centre.y - (int16_t)((cos_lookup(angle) * r) / TRIG_MAX_RATIO));
}

static void draw_dial(GContext *ctx, GRect bounds, GPoint centre, int radius,
                      const Tier *tier) {
  graphics_context_set_fill_color(ctx, COLOR_DIAL);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Minute track: one deeper-salmon pip per minute, with the hour positions
  // left to the rhodium ticks. Small screens get a plain hairline instead —
  // sixty pips at that size is noise.
  if (tier->minute_pips) {
    graphics_context_set_fill_color(ctx, COLOR_DIAL_DEEP);
    for (int i = 0; i < 60; i++) {
      if (i % 5 == 0) {
        continue;
      }
      graphics_fill_circle(ctx, polar(centre, i * TRIG_MAX_ANGLE / 60, radius - 6), 1);
    }
  } else {
    graphics_context_set_stroke_color(ctx, COLOR_DIAL_DEEP);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_circle(ctx, centre, radius - 6);
  }

  // Hour ticks in rhodium, doubled in length at the quarters.
  for (int i = 0; i < 12; i++) {
    int32_t angle = i * TRIG_MAX_ANGLE / 12;
    int len = (i % 3 == 0) ? tier->tick_len : tier->tick_len * 2 / 3;
    int outer_r = radius - 4;
    GPoint outer = polar(centre, angle, outer_r);
    GPoint inner = polar(centre, angle, outer_r - len);

    graphics_context_set_stroke_width(ctx, tier->tick_w + 2);
    graphics_context_set_stroke_color(ctx, COLOR_TICK_EDGE);
    graphics_draw_line(ctx, inner, outer);

    graphics_context_set_stroke_width(ctx, tier->tick_w);
    graphics_context_set_stroke_color(ctx, COLOR_TICK);
    graphics_draw_line(ctx, inner, outer);
  }
}

// A rhodium shaft: dark outline, polished body, and a brushed shade line
// along one edge so the metal reads as a bevelled bar rather than a stripe.
static void draw_shaft(GContext *ctx, GPoint centre, int32_t angle, int len,
                       int width) {
  GPoint tip = polar(centre, angle, len);

  graphics_context_set_stroke_color(ctx, COLOR_METAL_EDGE);
  graphics_context_set_stroke_width(ctx, width + 2);
  graphics_draw_line(ctx, centre, tip);

  graphics_context_set_stroke_color(ctx, COLOR_METAL);
  graphics_context_set_stroke_width(ctx, width);
  graphics_draw_line(ctx, centre, tip);

  // Offset perpendicular to the shaft: (cos, sin) is the normal of (sin, -cos).
  int off = width / 3;
  if (off < 1) {
    off = 1;
  }
  int16_t dx = (int16_t)((cos_lookup(angle) * off) / TRIG_MAX_RATIO);
  int16_t dy = (int16_t)((sin_lookup(angle) * off) / TRIG_MAX_RATIO);
  graphics_context_set_stroke_color(ctx, COLOR_METAL_SHADE);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(centre.x + dx, centre.y + dy),
                     GPoint(tip.x + dx, tip.y + dy));
}

// The plaque at the end of a hand, with its numeral engraved upright.
static void draw_plaque(GContext *ctx, GPoint at, const char *text,
                        const Tier *tier) {
  GRect pill = GRect(at.x - tier->pill_w / 2, at.y - tier->pill_h / 2,
                     tier->pill_w, tier->pill_h);
  int corner = tier->pill_h / 2;

  graphics_context_set_fill_color(ctx, COLOR_METAL);
  graphics_fill_rect(ctx, pill, corner, GCornersAll);

  graphics_context_set_stroke_color(ctx, COLOR_METAL_EDGE);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, pill, corner);

  graphics_context_set_stroke_color(ctx, COLOR_METAL_SHADE);
  graphics_draw_round_rect(ctx, grect_inset(pill, GEdgeInsets(1)), corner - 1);

  GFont font = fonts_get_system_font(tier->font);
  GSize size = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, tier->pill_w, tier->pill_h * 2),
      GTextOverflowModeFill, GTextAlignmentCenter);
  GRect box = GRect(pill.origin.x, at.y - size.h / 2 + tier->text_dy,
                    tier->pill_w, size.h + 4);

  graphics_context_set_text_color(ctx, COLOR_NUMERAL);
  graphics_draw_text(ctx, text, font, box, GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

static void draw_cap(GContext *ctx, GPoint centre, const Tier *tier) {
  int r = tier->shaft_w / 2 + 3;
  graphics_context_set_fill_color(ctx, COLOR_METAL_EDGE);
  graphics_fill_circle(ctx, centre, r);
  graphics_context_set_fill_color(ctx, COLOR_METAL);
  graphics_fill_circle(ctx, centre, r - 1);
  graphics_context_set_fill_color(ctx, COLOR_DIAL_DEEP);
  graphics_fill_circle(ctx, centre, 1);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect full = layer_get_bounds(layer);
  GRect bounds = layer_get_unobstructed_bounds(layer);
  GPoint centre = grect_center_point(&bounds);
  int radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2;
  const Tier *tier = pick_tier(radius * 2);

  // The dial fills the whole layer; only the hands respect the Quick View area.
  draw_dial(ctx, full, centre, radius, tier);

  char hour_text[3];
  char minute_text[3];
  snprintf(hour_text, sizeof(hour_text), "%d", s_hour);
  snprintf(minute_text, sizeof(minute_text), "%02d", s_minute);

  // Both hands sit exactly on the marker they name: the hour hand on the hour
  // tick, not part-way to the next one, so hand and numeral always agree.
  int32_t hour_angle = (s_hour % 12) * TRIG_MAX_ANGLE / 12;
  int32_t minute_angle = s_minute * TRIG_MAX_ANGLE / 60;

  // Shafts and cap first, then both plaques: when the hands line up, the
  // minute shaft would otherwise run straight through the hour numeral.
  draw_shaft(ctx, centre, hour_angle, tier->hour_r, tier->shaft_w);
  draw_shaft(ctx, centre, minute_angle, tier->minute_r, tier->shaft_w);
  draw_cap(ctx, centre, tier);
  draw_plaque(ctx, polar(centre, hour_angle, tier->hour_r), hour_text, tier);
  draw_plaque(ctx, polar(centre, minute_angle, tier->minute_r), minute_text, tier);
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
