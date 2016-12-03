#include "pebble.h"
#include "pebble_fonts.h"
#include "main.h"

static Window *s_main_window;
static GBitmap *s_bitmap;
static BitmapLayer *s_background_layer;
static GPath *s_seconds_handle_path;
static GPath *s_minutes_handle_path;
static GPath *s_hours_handle_path;
static Layer *s_hands_layer;
static Layer *s_calories_layer;
static TextLayer *s_calories_label;
static int minutes = 0;

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  // try saving the battery by not drawing at minute only when in quite time
  if (quiet_time_is_active()) {
      if (units_changed & MINUTE_UNIT) {
	  layer_mark_dirty(window_get_root_layer(s_main_window));
      }
  } else {
      layer_mark_dirty(window_get_root_layer(s_main_window));
  }
}

static void hands_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const GPoint center = grect_center_point(&bounds);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  // try saving the battery by not drawing the second hand when in quite time
  if (!quiet_time_is_active()) {
      // Draw second hand
      graphics_context_set_fill_color(ctx, GColorBlack);
      gpath_rotate_to(s_seconds_handle_path, TRIG_MAX_ANGLE * t->tm_sec / 60);
      gpath_draw_outline(ctx, s_seconds_handle_path);
  }

  // Draw mintues hand
  gpath_rotate_to(s_minutes_handle_path, TRIG_MAX_ANGLE * t->tm_min / 60);
  gpath_draw_filled(ctx, s_minutes_handle_path);

  // Draw hours hand
  gpath_rotate_to(s_hours_handle_path, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hours_handle_path);

  // Draw center circle
  graphics_draw_circle(ctx, center, 5); // Draw the outline of a circle
  graphics_fill_circle(ctx, center, 5); // Fill a circle

  // Draw dot in the middle
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 1, bounds.size.h / 2 - 1, 3, 3), 0, GCornerNone);

  // Verify if 1 minute passed to update -- try to save battery
  if (t->tm_min != minutes) {
      minutes = t->tm_min;
      static char s_value_buffer[8];
//      int value = health_service_sum_today(HealthMetricRestingKCalories) + health_service_sum_today(HealthMetricActiveKCalories);
//      snprintf(s_value_buffer, sizeof(s_value_buffer), "%d", value);
      int seconds_active = health_service_sum_today(HealthMetricActiveSeconds);
      int hours_active = seconds_active / (60 * 60);
      int minutes_active = (seconds_active / 60) - (hours_active * 60);
      snprintf(s_value_buffer, sizeof(s_value_buffer), "%.1d:%.2d", hours_active, minutes_active);
      text_layer_set_text(s_calories_label, s_value_buffer);

      //APP_LOG(APP_LOG_LEVEL_DEBUG, "cals update %d", t->tm_sec);
  }
}

static void push_background (Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  GPoint center = grect_center_point(&bounds);

  // Prepare background image layer
  s_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BACKGROUND);
  s_background_layer = bitmap_layer_create(GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT));
  bitmap_layer_set_bitmap(s_background_layer, s_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));

  // Init hand paths
  s_seconds_handle_path = gpath_create(&SECOND_HAND_POINTS);
  s_minutes_handle_path = gpath_create(&MINUTE_HAND_POINTS);
  s_hours_handle_path = gpath_create(&HOUR_HAND_POINTS);
  gpath_move_to(s_seconds_handle_path, center);
  gpath_move_to(s_minutes_handle_path, center);
  gpath_move_to(s_hours_handle_path, center);

  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);

  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(window_layer, s_hands_layer);

  /* Calories */
  s_calories_layer = layer_create(bounds);
  //layer_set_update_proc(s_calories_layer, calories_update_proc);
  layer_add_child(window_layer, s_calories_layer);

  // Calories label
  s_calories_label = text_layer_create(GRect((PBL_DISPLAY_WIDTH / 2) - 30, (PBL_DISPLAY_HEIGHT - 60), 62, 25));
  text_layer_set_background_color(s_calories_label, GColorClear);
  text_layer_set_text_color(s_calories_label, GColorBlack);
  text_layer_set_font(s_calories_label, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_calories_label, GTextAlignmentCenter);

  layer_add_child(s_calories_layer, text_layer_get_layer(s_calories_label));
}

static void main_window_load(Window *window) {
  // Draw the background
  push_background(window);
}

static void main_window_unload(Window *window) {
  gbitmap_destroy(s_bitmap);
  bitmap_layer_destroy(s_background_layer);
  layer_destroy(s_hands_layer);
  layer_destroy(s_calories_layer);
  text_layer_destroy(s_calories_label);
  gpath_destroy(s_seconds_handle_path);
  gpath_destroy(s_minutes_handle_path);
  gpath_destroy(s_hours_handle_path);
  tick_timer_service_unsubscribe();
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
