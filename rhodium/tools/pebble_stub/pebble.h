// Minimal stand-in for the Pebble SDK header: enough declarations to
// syntax-check main.c on the host. Not part of the project.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef STUB_COLOR
#define PBL_COLOR 1
#endif
#define ARRAY_LENGTH(a) (sizeof(a)/sizeof((a)[0]))
#define TRIG_MAX_ANGLE 0x10000
#define TRIG_MAX_RATIO 0xffff

typedef struct { uint8_t argb; } GColor;
#define GColorMelon        ((GColor){0})
#define GColorSunsetOrange ((GColor){1})
#define GColorWhite        ((GColor){2})
#define GColorLightGray    ((GColor){3})
#define GColorDarkGray     ((GColor){4})
#define GColorBlack        ((GColor){5})

typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
#define GPoint(x, y) ((GPoint){(int16_t)(x), (int16_t)(y)})
#define GRect(x, y, w, h) ((GRect){{(int16_t)(x), (int16_t)(y)}, {(int16_t)(w), (int16_t)(h)}})
typedef struct { int16_t top, right, bottom, left; } GEdgeInsets;
#define GEdgeInsets(v) ((GEdgeInsets){(v), (v), (v), (v)})

typedef enum { GCornerNone = 0, GCornersAll = 1 } GCornerMask;
typedef enum { GTextOverflowModeFill } GTextOverflowMode;
typedef enum { GTextAlignmentCenter } GTextAlignment;
typedef enum { MINUTE_UNIT } TimeUnits;
typedef uint32_t AnimationProgress;

typedef struct GContext GContext;
typedef struct Layer Layer;
typedef struct Window Window;
typedef void *GFont;

typedef struct { void (*load)(Window *); void (*unload)(Window *); } WindowHandlers;
typedef struct { void (*change)(AnimationProgress, void *); } UnobstructedAreaHandlers;

int32_t sin_lookup(int32_t a);
int32_t cos_lookup(int32_t a);
void graphics_context_set_fill_color(GContext *, GColor);
void graphics_context_set_stroke_color(GContext *, GColor);
void graphics_context_set_text_color(GContext *, GColor);
void graphics_context_set_stroke_width(GContext *, uint8_t);
void graphics_fill_rect(GContext *, GRect, uint16_t, GCornerMask);
void graphics_fill_circle(GContext *, GPoint, uint16_t);
void graphics_draw_circle(GContext *, GPoint, uint16_t);
void graphics_draw_line(GContext *, GPoint, GPoint);
void graphics_draw_round_rect(GContext *, GRect, uint16_t);
void graphics_draw_text(GContext *, const char *, GFont, GRect, GTextOverflowMode, GTextAlignment, void *);
GSize graphics_text_layout_get_content_size(const char *, GFont, GRect, GTextOverflowMode, GTextAlignment);
GRect grect_inset(GRect, GEdgeInsets);
GPoint grect_center_point(const GRect *);
GFont fonts_get_system_font(const char *);
#define FONT_KEY_DROID_SERIF_28_BOLD "RESOURCE_ID_DROID_SERIF_28_BOLD"
#define FONT_KEY_GOTHIC_18_BOLD "RESOURCE_ID_GOTHIC_18_BOLD"

Layer *layer_create(GRect);
void layer_destroy(Layer *);
GRect layer_get_bounds(Layer *);
GRect layer_get_unobstructed_bounds(Layer *);
void layer_set_update_proc(Layer *, void (*)(Layer *, GContext *));
void layer_add_child(Layer *, Layer *);
void layer_mark_dirty(Layer *);
Window *window_create(void);
void window_destroy(Window *);
Layer *window_get_root_layer(Window *);
void window_set_background_color(Window *, GColor);
void window_set_window_handlers(Window *, WindowHandlers);
void window_stack_push(Window *, bool);
void tick_timer_service_subscribe(TimeUnits, void (*)(struct tm *, TimeUnits));
void tick_timer_service_unsubscribe(void);
void unobstructed_area_service_subscribe(UnobstructedAreaHandlers, void *);
void unobstructed_area_service_unsubscribe(void);
void app_event_loop(void);
typedef struct { uint32_t num_points; GPoint *points; } GPathInfo;
typedef struct GPath GPath;
GPath *gpath_create(GPathInfo *);
void gpath_destroy(GPath *);
void gpath_draw_filled(GContext *, GPath *);
void gpath_draw_outline(GContext *, GPath *);
