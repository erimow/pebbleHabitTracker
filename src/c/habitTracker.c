#include <pebble.h>
#include <stdbool.h>


static Window *s_window;
//static TextLayer *s_text_layer;
static MenuLayer *s_menu_layer;
static ActionBarLayer *s_action_bar_layer;
static DictationSession *s_dictation_session;
static GBitmap *checkImage;
static GBitmap *uncheckImage;
static GBitmap *whitecheckImage;
static GBitmap *whiteuncheckImage;
static GBitmap *doneImage;
static GBitmap *trashImage;
static GBitmap *clockImage;
static uint16_t dayofyear, prev_dayofyear;

#define BUFFER_LENGTH 32
#define MAX_HABITS 8

typedef struct {
  char name[BUFFER_LENGTH];
  bool done;
} habit;

static habit trackerList[MAX_HABITS] = {
{"Take Pills", false},
{"Eat Dinner", false},
{"Wake Up", false}
};

static uint8_t num_habits = 3; 
static uint8_t additional_layers = 1;//add one for the +

// #define NUM_ITEMS (sizeof(trackerList) / sizeof(trackerList[0]))
#define PERSIST_KEY_NUMHABITS 21
#define PERSIST_KEY_BASE 88
#define PERSIST_KEY_DAY 120

static void save_state(void) {
  for (int i = 0; i < num_habits; i++) {
    // persist_write_bool(PERSIST_KEY_BASE + i, trackerList[i].done);
    persist_write_data(PERSIST_KEY_BASE+i, &trackerList[i], sizeof(habit));
  }
  persist_write_int(PERSIST_KEY_NUMHABITS, num_habits);
  persist_write_int(PERSIST_KEY_DAY, dayofyear);
}

static void load_state(void) {
  if (persist_exists(PERSIST_KEY_NUMHABITS)){
    num_habits=persist_read_int(PERSIST_KEY_NUMHABITS);
  }
  for (int i = 0; i < num_habits; i++) { //if day has not changed then pull habit data
    if (persist_exists(PERSIST_KEY_BASE + i)) {
      persist_read_data(PERSIST_KEY_BASE + i, &trackerList[i], sizeof(habit));
    }
  }
  if (persist_exists(PERSIST_KEY_DAY)){
    prev_dayofyear = persist_read_int(PERSIST_KEY_DAY);
    if (prev_dayofyear != dayofyear){
    APP_LOG(APP_LOG_LEVEL_INFO, "Day has changed! Resetting habits!");
      for (uint8_t i = 0; i < num_habits; i++) {
        trackerList[i].done = false;
        // persist_write_data(PERSIST_KEY_BASE + i, false);
      }
      return; //break out of function if the day has changed
    }
  }
}

 static int get_day_of_year() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  return t->tm_yday;  // 0–365
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (units_changed & DAY_UNIT) {
    // Reset all habits at the start of a new day
    for (uint8_t i = 0; i < num_habits; i++) {
      trackerList[i].done = false;
      
    }

    // Refresh the list UI
    layer_mark_dirty(window_get_root_layer(s_window));
    APP_LOG(APP_LOG_LEVEL_INFO, "Habits reset at midnight");
  }
}

static void dictation_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void* context){
  
if (status == DictationSessionStatusSuccess){
    if (num_habits!=MAX_HABITS){
      trackerList[num_habits].done = false;
      snprintf(trackerList[num_habits].name, sizeof(char)*BUFFER_LENGTH, "%s", transcription);
      num_habits++;
      save_state();

      menu_layer_reload_data(s_menu_layer);
      layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
    }
  }
  
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return num_habits+additional_layers; // +1 for plus layer |
}

static void menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  if (cell_index->row != 0){
  if (menu_cell_layer_is_highlighted(cell_layer))
            menu_cell_basic_draw(ctx, cell_layer, trackerList[cell_index->row-additional_layers].name, trackerList[cell_index->row-additional_layers].done ? "Done!" : "Not Done",trackerList[cell_index->row-additional_layers].done ? whitecheckImage : whiteuncheckImage );
  else
            menu_cell_basic_draw(ctx, cell_layer, trackerList[cell_index->row-additional_layers].name, trackerList[cell_index->row-additional_layers].done ? "Done!" : "Not Done",trackerList[cell_index->row-additional_layers].done ? checkImage : uncheckImage );
  }else{
    menu_cell_title_draw(ctx, cell_layer, "+");
  }

}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  int row = cell_index->row;
  if (row!=0)
    trackerList[row-additional_layers].done = !trackerList[row-additional_layers].done; // Toggle checked state
  else{
    //if the plus is selected
    if (num_habits!=MAX_HABITS){
      dictation_session_start(s_dictation_session); //start dictation session | this will then call my callback
    }//else tell user they can add no more habits
  }
  // vibes_short_pulse(); sends a short vibration pulse
  save_state(); //save after changing
  layer_mark_dirty(menu_layer_get_layer(menu_layer)); // Refresh display
 // APP_LOG(APP_LOG_LEVEL_DEBUG, "button pressed\n");
}



static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) { // delete
  uint8_t row = menu_layer_get_selected_index(s_menu_layer).row;
  for (int i = row-1; i<num_habits-1; i++){
    trackerList[i] = trackerList[i+1];
  }
  num_habits--;
  save_state(); //save just cuz
  if (row-1 > num_habits)menu_layer_set_selected_next(s_menu_layer, MenuRowAlignNone, true, true);
  action_bar_layer_remove_from_window(s_action_bar_layer);
  menu_layer_set_click_config_onto_window(s_menu_layer, s_window);
  menu_layer_reload_data(s_menu_layer);
  layer_mark_dirty(menu_layer_get_layer(s_menu_layer)); // Refresh display
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context){
  action_bar_layer_remove_from_window(s_action_bar_layer);
  menu_layer_set_click_config_onto_window(s_menu_layer, s_window);
}

void action_bar_click_config_provider(void *context) { //
  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) prv_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) prv_select_click_handler);
}

static void menu_select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context){ //PULLS UP ACTION BAR
  if (cell_index->row != 0){
    action_bar_layer_add_to_window(s_action_bar_layer, s_window);
    action_bar_layer_set_click_config_provider(s_action_bar_layer,
                                             action_bar_click_config_provider);
  }
}

// static void prv_click_config_provider(void *context) {
//   // window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
//   // window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
//   // window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
// }

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // s_text_layer = text_layer_create(GRect(0, 72, bounds.size.w, 20));
  // text_layer_set_text(s_text_layer, "Press a button");
  // text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  // layer_add_child(window_layer, text_layer_get_layer(s_text_layer));


  checkImage = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHECK);
  uncheckImage = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_UNCHECK);
  whitecheckImage = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHECKWHITE);
  whiteuncheckImage = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_UNCHECKWHITE);
  doneImage = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DONE);
  trashImage = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRASH);
  clockImage = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CLOCK);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = menu_get_num_rows_callback,
    .draw_row = menu_draw_row_callback,
    .select_long_click = menu_select_long_callback,
    .select_click = menu_select_callback
  });

  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

  s_action_bar_layer = action_bar_layer_create();
  action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_UP, clockImage, true);
  action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_DOWN, trashImage, true);
  action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_SELECT, doneImage, true);
}

static void prv_window_unload(Window *window) {
  // text_layer_destroy(s_text_layer);
  gbitmap_destroy(checkImage);
  gbitmap_destroy(uncheckImage);
  gbitmap_destroy(whitecheckImage);
  gbitmap_destroy(whiteuncheckImage);
  gbitmap_destroy(doneImage);
  gbitmap_destroy(trashImage);
  gbitmap_destroy(clockImage);
  action_bar_layer_destroy(s_action_bar_layer);
}

static void prv_init(void) {
  dayofyear = get_day_of_year();
  load_state();
  tick_timer_service_subscribe(DAY_UNIT, tick_handler);
  s_dictation_session = dictation_session_create(BUFFER_LENGTH, dictation_callback, NULL);
  s_window = window_create();
  // window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  dictation_session_destroy(s_dictation_session);
  menu_layer_destroy(s_menu_layer);
  window_destroy(s_window);
  tick_timer_service_unsubscribe();
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
