#include <pebble.h>
#include "rain-or-shine.h"

//#define SCREENSHOT 1

//#undef APP_LOG
//#define APP_LOG(...)

//#define BGCOLOR GColorCobaltBlue
#define BGCOLOR GColorBlack
#define FGCOLOR GColorWhite

#define KEY_WEATHER_TYPE 0
#define WEATHER_CODE_KEY 1
#define WEATHER_TEMPERATURE_KEY 2
#define WEATHER_SUNRISE_KEY 3
#define WEATHER_SUNSET_KEY 4
#define WEATHER_CITY_KEY 5
#define WEATHER_TIMESTAMP_KEY 6
#define KEY_READY 7
#define WEATHER_DESCRIPTION 8
#define WEATHER_CITYCODE_KEY 9
#define WEATHER_CURRENT_LOCATION_KEY 10
#define UTC_OFFSET_KEY 11

#define SETTINGS_KEY 20

#define CITIES_KEY 30

#define KEY_CURRENT_WEATHER_DATA 100
#define KEY_HOURLY_WEATHER_DATA 200
#define KEY_DAILY_WEATHER_DATA 300

// DEFAULTS TO 1800 10800
#define WEATHER_INTERVAL_SECS 1800
#define WEATHER_VALID_SECS 10800

// DEFAULTS TO 14400 21600
#define FORECAST_INTERVAL_SECS 10800
#define FORECAST_VALID_SECS 21600

#define MAX_CITIES 10
#define MAX_SEARCH_CITIES 10

Window *main_window, *menu_window, *forecast_window, *startup_window, *city_window;

static Layer *simple_bg_layer, *current_weather_layer, *weather_layer, *forecast_weather_layer,
             *current_weather_indicator_up_layer, *current_weather_indicator_down_layer,
             *forecast_weather_indicator_up_layer, *forecast_weather_indicator_down_layer;

static TextLayer *city_textlayer, *icon_textlayer, *temperature_textlayer, *description_textlayer,
                 *hourly_time_textlayer[11], *hourly_icon_textlayer[11], *hourly_temperature_textlayer[11],
                 *daily_time_textlayer[6], *daily_icon_textlayer[6], *daily_temperature_textlayer[6],
                 *daily_night_temperature_textlayer[6], *title_anim1_textlayer, *icon_anim1_textlayer, 
                 *icon_anim2_textlayer, *city_menu_mainheader_textlayer;

static ScrollLayer *current_weather_scrolllayer, *forecast_weather_scrolllayer;

static GFont font_roboto_light_18, font_roboto_regular_18, font_roboto_light_36, font_weather_icons_18,
             font_weather_icons_30, font_roboto_light_14, font_roboto_regular_14, font_weather_icons_14;

static ContentIndicator *current_weather_indicator, *forecast_weather_indicator ;

static MenuLayer *action_menulayer, *city_menulayer;

static BitmapLayer *spinner_bitmaplayer;
static GBitmapSequence *spinner_sequence = NULL;
static GBitmap *spinner_bitmap = NULL;

static AppTimer *spinner_animation_timer, *startup_animation_timer;

static DictationSession *dictation_session;
static char city_transcription[128];

time_t startup_animation_start_timestamp;

struct city {
  uint32_t city_id;
  char name[64];
  char tz[32];
};

typedef struct city CityList;

static CityList cities[MAX_CITIES] = { {0, "Current location", ""}, {5128581, "New York", "America/New_York"}, {5391959, "San Francisco", "America/Los_Angeles"}, { 2756039, "Ermelo", "Europe/Amsterdam"} };
static CityList cities_empty;

struct citySearch {
  uint32_t city_id;
  char name[64];
  char label[128];
  char tz[32];
};

typedef struct citySearch CitySearchList;

static CitySearchList city_search_results[MAX_SEARCH_CITIES];
static CitySearchList city_search_results_empty;

struct weather {
  time_t timestamp;
  int16_t temperature;
  uint16_t code;
  time_t sunrise;
  time_t sunset;
  uint32_t city_id;
  char city[40];
  char description[40];
  uint32_t last_try;
};

typedef struct weather WeatherInfo;

static WeatherInfo current_weather[MAX_CITIES] = { { 0, 999, 0, 0, 0, 0, "", "", 0 } };
static const WeatherInfo current_weather_empty;

struct forecast {
 time_t timestamp;
 int16_t temperature;
 uint16_t code;
};

typedef struct forecast Forecast;

struct forecast_hourly {
  time_t timestamp;
  uint32_t city_id;
  int32_t utcoffset;
  Forecast items[30];
};

typedef struct forecast_hourly ForecastHourly;
  
static ForecastHourly current_hourly[MAX_CITIES];
static const ForecastHourly current_hourly_empty;

struct forecast_per_day {
 time_t timestamp;
 int16_t temperature_min;
 int16_t temperature_max;  
 uint16_t code;
};

typedef struct forecast_per_day ForecastPerDay;

struct forecast_daily {
  time_t timestamp;
  uint32_t city_id;
  int32_t utcoffset;
  ForecastPerDay items[6];
};

typedef struct forecast_daily ForecastDaily;
  
static ForecastDaily current_daily[MAX_CITIES];
static const ForecastDaily current_daily_empty;

static bool ready = 0;

static AppTimer *scrolling_timer, *spinner_timer;

static uint8_t spinner_calls = 0;
static uint8_t spinner_running = 0;

struct settings {
  bool celsius;
  uint32_t current_city;
};
typedef struct settings Settings;

static Settings app_settings;

#define MENU_ITEMS_TYPE_ADDCITY 0
#define MENU_ITEMS_TYPE_TEMPERATUREUNITS 1
#define MENU_ITEMS_TYPE_FORECAST 2
#define MENU_ITEMS_TYPE_CITY 3
#define MENU_ITEMS_TYPE_DELETECITY 4

struct menu_items {
  char title[128];
  char subtitle[25];
  uint8_t type;
  uint32_t city_item;
};
typedef struct menu_items MenuItems;
  
static MenuItems action_menu_items[MAX_CITIES+3];
static const MenuItems action_menu_item_empty;

struct city_menu {
  char title[128];
  MenuItems items[10];
};
typedef struct city_menu CityMenu;
  
static CityMenu city_menu[2];
static const CityMenu city_menu_section_empty;

static int16_t convert_temp(int16_t temperature) {
  if ( app_settings.celsius == true ) {
    temperature = (temperature - 32) * 0.5556;
  }
  
  return(temperature);
}

static void log_cities() {
  for(int i=0; i<MAX_CITIES; i++) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "log_cities(): city_id=%lu name=%s tz=%s", cities[i].city_id, cities[i].name, cities[i].tz);
  }
}

static void read_cities() {
  for(int i=0; i<MAX_CITIES; i++) {
    if ( persist_exists(CITIES_KEY + i) ) {
      persist_read_data(CITIES_KEY + i, &cities[i], sizeof(cities[i]));
    }
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "read_cities(): read cities");
}

static void save_cities() {
  for(int i=0; i<MAX_CITIES; i++) {
    persist_write_data(CITIES_KEY + i, &cities[i], sizeof(cities[i]));
  }
}

static void delete_city(uint8_t item) {
  
  uint8_t cnt = 0;
  for(int i=0; i<MAX_CITIES; i++) {
    if ( i > item ) {
      cities[cnt] = cities[i];
      current_weather[cnt] = current_weather[i];
      current_daily[cnt] = current_daily[i];
      current_hourly[cnt] = current_hourly[i];
    }

    if ( i != item ) {
      cnt++;
    }
  }

  cities[MAX_CITIES-1] = cities_empty;
  current_weather[MAX_CITIES-1] = current_weather_empty;
  current_daily[MAX_CITIES-1] = current_daily_empty;
  current_hourly[MAX_CITIES-1] = current_hourly_empty;
}

static void read_settings() {
  if ( persist_exists(SETTINGS_KEY) ) {
    persist_read_data(SETTINGS_KEY, &app_settings, sizeof(app_settings));
  } else {
    if ( clock_is_24h_style() ) {
      app_settings.celsius = true;
    } else {
      app_settings.celsius = false;
    }
    
    app_settings.current_city = 0;
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "read_settings(): celsius=%d, current_city=%lu", app_settings.celsius, app_settings.current_city);
}

static void save_settings() {
  persist_write_data(SETTINGS_KEY, &app_settings, sizeof(app_settings));
}

static void spinner_timeout_handler(void *context) {
  spinner_calls = 0;
}

static void spinner_timer_handler(void *context) {
  
  if ( spinner_calls == 0 ) {
      
    if (spinner_sequence) {
      gbitmap_sequence_destroy(spinner_sequence);
      spinner_sequence = NULL;
    }
    if (spinner_bitmap) {
      gbitmap_destroy(spinner_bitmap);
      spinner_bitmap = NULL;
    }
    
    if (spinner_animation_timer) {
      app_timer_cancel(spinner_animation_timer);
      spinner_animation_timer = NULL;
    }
    
    if ( current_weather_indicator_up_layer ) {
      if ( layer_get_window(current_weather_indicator_up_layer)) {
        layer_set_hidden(current_weather_indicator_up_layer, false);
      }
    }
    
    if ( spinner_bitmaplayer ) {
      if ( layer_get_window(bitmap_layer_get_layer(spinner_bitmaplayer)) ) {
        layer_set_hidden(bitmap_layer_get_layer(spinner_bitmaplayer), true);
      }
    }
    
    spinner_timer = NULL;
    
  } else {
    
    if ( ! spinner_sequence ) {
      layer_set_hidden(current_weather_indicator_up_layer, true);
      
      // Create sequence
      spinner_sequence = gbitmap_sequence_create_with_resource(RESOURCE_ID_LOADING_SPINNER_12);
  
      // Create GBitmap
      spinner_bitmap = gbitmap_create_blank(gbitmap_sequence_get_bitmap_size(spinner_sequence), GBitmapFormat8Bit);
      
      app_timer_register(60000, spinner_timeout_handler, NULL);
      layer_set_hidden(bitmap_layer_get_layer(spinner_bitmaplayer), false);
    }
    
    uint32_t next_delay;

    if ( spinner_sequence && spinner_bitmap && spinner_bitmaplayer ) {

      // Advance to the next APNG frame
      if(gbitmap_sequence_update_bitmap_next_frame(spinner_sequence, spinner_bitmap, &next_delay)) {
        bitmap_layer_set_bitmap(spinner_bitmaplayer, spinner_bitmap);
        layer_mark_dirty(bitmap_layer_get_layer(spinner_bitmaplayer));

        // Timer for that delay
        spinner_timer = app_timer_register(next_delay, spinner_timer_handler, NULL);
      } else {
        // Start again
        gbitmap_sequence_restart(spinner_sequence);
      }
    }
  }
}

static void stop_spinner() {
  
  if ( spinner_calls > 0 ) {
    spinner_calls--;
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "stop_spinner(): spinner_calls=%d", spinner_calls);
}

static void start_spinner() {
  
  spinner_calls++;
  
  if ( ! spinner_timer ) {
    spinner_timer = app_timer_register(1, spinner_timer_handler, NULL);
  }
    
  APP_LOG(APP_LOG_LEVEL_DEBUG, "start_spinner(): spinner_calls=%d", spinner_calls);
}

static void stop_startup_animation() {
  if ( startup_animation_timer ) {
    app_timer_cancel(startup_animation_timer);
    startup_animation_timer = NULL;
    window_stack_pop(true);
    light_enable_interaction();
  }
}

static void start_startup_animation(void *data) {
#define STARTUP_ANIM_MOVE_PX 5
#define STARTUP_ANIM_SPEED_MS 50
#define STARTUP_ANIM_TIMEOUT_SECS 15
  
  if ( ! startup_animation_start_timestamp ) {
    startup_animation_start_timestamp = time(NULL);
  }
  
  if ( current_weather[app_settings.current_city].timestamp > 0 || 
       time(NULL) > startup_animation_start_timestamp + STARTUP_ANIM_TIMEOUT_SECS )
  {
    stop_startup_animation();
  } else {
  
    //Layer *window_layer = window_get_root_layer(startup_window);
    GRect bounds = layer_get_bounds(text_layer_get_layer(icon_anim1_textlayer));
      
    GRect anim1_frame = layer_get_frame(text_layer_get_layer(icon_anim1_textlayer));
    if ( anim1_frame.origin.x > 0 - bounds.size.w ) {
      anim1_frame.origin.x = anim1_frame.origin.x - STARTUP_ANIM_MOVE_PX;
    } else {
      anim1_frame.origin.x = bounds.size.w - STARTUP_ANIM_MOVE_PX;
    }
    layer_set_frame(text_layer_get_layer(icon_anim1_textlayer), anim1_frame);
    
    GRect anim2_frame = layer_get_frame(text_layer_get_layer(icon_anim2_textlayer));
    if ( anim2_frame.origin.x > 0 - bounds.size.w ) {
      anim2_frame.origin.x = anim2_frame.origin.x - STARTUP_ANIM_MOVE_PX;
    } else {
      anim2_frame.origin.x = bounds.size.w - STARTUP_ANIM_MOVE_PX;
    }
    layer_set_frame(text_layer_get_layer(icon_anim2_textlayer), anim2_frame);
    
    startup_animation_timer = app_timer_register(STARTUP_ANIM_SPEED_MS, start_startup_animation, NULL);
  }
}

static void startup_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  const char *icons = " \U0000F00B \U0000f005 \U0000f003 \U0000f00d";
  
  title_anim1_textlayer = text_layer_create(GRect(0, bounds.size.h / 2 - 50, bounds.size.w, 30));
  text_layer_set_font(title_anim1_textlayer, font_roboto_light_18);
  text_layer_set_text_color(title_anim1_textlayer, FGCOLOR);
  text_layer_set_background_color(title_anim1_textlayer, GColorClear);
  text_layer_set_text_alignment(title_anim1_textlayer, GTextAlignmentCenter);
  text_layer_set_text(title_anim1_textlayer, "Rain or Shine");
  layer_add_child(window_layer, text_layer_get_layer(title_anim1_textlayer));
  
  icon_anim1_textlayer = text_layer_create(GRect(0, bounds.size.h / 2 - 10, 210, 40));
  text_layer_set_font(icon_anim1_textlayer, font_weather_icons_30);
  text_layer_set_text_color(icon_anim1_textlayer, FGCOLOR);
  text_layer_set_background_color(icon_anim1_textlayer, GColorClear);
  text_layer_set_text(icon_anim1_textlayer, icons);
  layer_add_child(window_layer, text_layer_get_layer(icon_anim1_textlayer));
    
  GRect anim1_frame = layer_get_frame(text_layer_get_layer(icon_anim1_textlayer));
  
  icon_anim2_textlayer = text_layer_create(GRect(anim1_frame.size.w, bounds.size.h / 2 - 10, anim1_frame.size.w, 40));
  text_layer_set_font(icon_anim2_textlayer, font_weather_icons_30);
  text_layer_set_text_color(icon_anim2_textlayer, FGCOLOR);
  text_layer_set_background_color(icon_anim2_textlayer, GColorClear);
  text_layer_set_text(icon_anim2_textlayer, icons);
  layer_add_child(window_layer, text_layer_get_layer(icon_anim2_textlayer));
  
  start_startup_animation(NULL);
}

static void startup_window_unload() {
  text_layer_destroy(icon_anim1_textlayer);
  text_layer_destroy(icon_anim2_textlayer);
}

static void close_startup_window(void *data) {
#define STARTUP_CHECK_MS 50
#define STARTUP_TIMEOUT_SECS 1
if ( current_weather[app_settings.current_city].timestamp > 0 || 
       time(NULL) > startup_animation_start_timestamp + STARTUP_TIMEOUT_SECS )
  {
    window_stack_pop(false);
  } else {
    app_timer_register(STARTUP_CHECK_MS, close_startup_window, NULL);
  }
}

static void show_startup_animation() {
  startup_window = window_create();
  //window_set_window_handlers(startup_window, (WindowHandlers) {
  //  .load = startup_window_load,
  //  .unload = startup_window_unload,
  //});
  window_set_background_color(startup_window, BGCOLOR);
  window_stack_push(startup_window, true);
  
  startup_animation_start_timestamp = time(NULL);
  app_timer_register(STARTUP_CHECK_MS, close_startup_window, NULL);
}


static void log_weather(uint8_t item) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "log_weather(): code = %u, temperature = %u, sunrise = %u, sunset = %u", (int) current_weather[item].code,
              (int) current_weather[item].temperature, (int) current_weather[item].sunrise, (int) current_weather[item].sunset);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "log_weather(): city = %s (%u), timestamp = %u", current_weather[item].city, (int) current_weather[item].city_id, (int) current_weather[item].timestamp);
}

static void save_weather() {
  
  int i;
  for(i=0; i<MAX_CITIES; i++) {
    if ( strlen(cities[i].name) > 0 ) {
      persist_write_data(KEY_CURRENT_WEATHER_DATA + i, &current_weather[i], sizeof(current_weather[i]));
    }
  }
  
   APP_LOG(APP_LOG_LEVEL_DEBUG, "save_weather(): wrote %d cities", i + 1);
}

static void read_weather() {
  
  int i;
  for(i=0; i < MAX_CITIES; i++) {
    persist_read_data(KEY_CURRENT_WEATHER_DATA + i, &current_weather[i], sizeof(current_weather[i]));
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "read_weather(): current weather city=%d size=%d", i, size);
    if ( current_weather[i].timestamp ) {
      if ( (time_t) time(NULL) > current_weather[i].timestamp + WEATHER_VALID_SECS ) {
        current_weather[i].timestamp = 0;

        APP_LOG(APP_LOG_LEVEL_DEBUG, "read_weather(): current weather stale for %lu", current_weather[i].city_id);
      }
      APP_LOG(APP_LOG_LEVEL_DEBUG, "read_weather(): read weather for city %lu", current_weather[i].city_id);
    } else {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "read_weather(): done!");
      i = MAX_CITIES;
    }
  }
}

static void log_hourly_weather(uint8_t item) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "log_hourly_weather(): city = %u, timestamp = %u", (int) current_hourly[item].city_id, (int) current_hourly[item].timestamp);
  for(uint8_t i=0; i < 10; i++) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "log_hourly_weather(): ts = %u, code = %u, temperature = %u", 
            (int) current_hourly[item].items[i].timestamp, (int) current_hourly[item].items[i].code, 
            (int) current_hourly[item].items[i].temperature);
  }
}

static void save_hourly_weather() {

  int i;
  for(i=0; i<MAX_CITIES; i++) {
    if ( strlen(cities[i].name) > 0 ) {
      persist_write_data(KEY_HOURLY_WEATHER_DATA + i, &current_hourly[i], sizeof(current_hourly[i]));
    }
  }
   APP_LOG(APP_LOG_LEVEL_DEBUG, "save_hourly_weather(): saved %d cities", i);
}

static void read_hourly_weather() {
  
  int i;
  for(i=0; i < MAX_CITIES; i++) {
    
    if ( persist_exists(KEY_HOURLY_WEATHER_DATA + i) ) {
      persist_read_data(KEY_HOURLY_WEATHER_DATA +i, &current_hourly[i], sizeof(current_hourly[i]));
      
      if ( (time_t) time(NULL) > current_hourly[i].timestamp + FORECAST_VALID_SECS ) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "read_hourly_weather(): hourly forecast stale for city %lu (%d)", current_hourly[i].city_id, i);
        current_hourly[i] = current_hourly_empty;      
      } else {
       log_hourly_weather(i);   
      }
    }
  }
}

static void log_daily_weather(uint8_t item) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "log_daily_weather(): city = %u, timestamp = %u", (int) current_daily[item].city_id, (int) current_daily[item].timestamp);
  for(uint8_t i=0; i < 6; i++) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "log_daily_weather(): ts = %u, code = %u, temp_min = %u, temp_max = %u", (int) current_daily[item].items[i].timestamp, (int) current_daily[item].items[i].code, (int) current_daily[item].items[i].temperature_min, (int) current_daily[item].items[i].temperature_max);
  }
}

static void save_daily_weather() {
  
  int i;
  for(i=0; i<MAX_CITIES; i++) {
    if ( strlen(cities[i].name) > 0 ) {
      persist_write_data(KEY_DAILY_WEATHER_DATA + i, &current_daily[i], sizeof(current_daily[i]));
    }
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "save_daily_weather(): saved %d cities", i);
}

static void read_daily_weather() {
  
  int i;
  for(i=0; i < MAX_CITIES; i++) {
    
    if ( persist_exists(KEY_DAILY_WEATHER_DATA + i) ) {
      persist_read_data(KEY_DAILY_WEATHER_DATA +i, &current_daily[i], sizeof(current_daily[i]));
      
      if ( (time_t) time(NULL) > current_daily[i].timestamp + FORECAST_VALID_SECS ) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "read_daily_weather(): daily forecast stale for city %lu (%d)", current_daily[i].city_id, i);
        current_daily[i] = current_daily_empty;      
      } else {
       log_daily_weather(i);   
      }
    }
  }
}

static uint8_t getCurrentWeatherItem(uint32_t city_id) {
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "getCurrentWeatherItem() find item for city id %lu", city_id);
  
  for(int i = 0; i < MAX_CITIES; i++) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "getCurrentWeatherItem(): evaluating city_id %lu (%s) to %lu", cities[i].city_id, cities[i].name, city_id);
    if ( strlen(cities[i].name) > 0 ) {
      if ( cities[i].city_id == city_id ) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "getCurrentWeatherItem() found item %d for city id %lu", i, city_id);
        return(i);
      }
    }
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "getCurrentWeatherItem() did not find item for city id %lu", city_id);
  
  return(0);
}

static bool fetchCurrentWeather(void) {
  
  static bool weather_to_fetch = false;
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fetchCurrentWeather(): send request");

  DictionaryIterator *out;
  AppMessageResult result = app_message_outbox_begin(&out);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchCurrentWeather(): error app_message_outbox_begin: %d", result);
    return false;
  }

  int value = 1;
  dict_write_int(out, KEY_WEATHER_TYPE, &value, sizeof(int), true);
  for(int i=0; i<MAX_CITIES; i++) {
    if(strlen(cities[i].name) > 0) {
      if ( i > 0 && (time_t) time(NULL) < current_weather[i].timestamp + WEATHER_INTERVAL_SECS ) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "fetchCurrentWeather(): weather current for %lu (%d), ignore request",
                cities[i].city_id, i);
      } else {
        APP_LOG(APP_LOG_LEVEL_INFO, "fetchCurrentWeather(): request city %lu", cities[i].city_id);
        dict_write_int(out, i+100, &cities[i].city_id, sizeof(uint32_t), true);
        weather_to_fetch = true;
      }
    }
  }

  result = app_message_outbox_send();
  if(result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchCurrentWeather(): error app_message_outbox_send");
    return false;
  }
  
  if ( weather_to_fetch == true ) {
    dict_write_int(out, KEY_WEATHER_TYPE, &value, sizeof(int), true);
    start_spinner();
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchCurrentWeather(): no weather to fetch");
  }
  
  return true;
}

static bool fetchHourlyWeather(void) {
  
  static bool weather_to_fetch = false;

  DictionaryIterator *out;
  AppMessageResult result = app_message_outbox_begin(&out);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchHourlyWeather(): error app_message_outbox_begin: %d", result);
    return false;
  }

  int value = 2;
  for(int i=0; i<MAX_CITIES; i++) {
    if(strlen(cities[i].name)) {

      if ( (time_t) time(NULL) < current_hourly[i].timestamp + FORECAST_INTERVAL_SECS ) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "fetchHourlyWeather(): hourly forecast current for %lu (%d), ignore request",
                cities[i].city_id, i);
      } else {                  
        APP_LOG(APP_LOG_LEVEL_INFO, "fetchHourlyWeather(): request city %lu", cities[i].city_id);
        static char buffer[100];
        snprintf(buffer, sizeof(buffer), "%lu,%s", cities[i].city_id, cities[i].tz);
        dict_write_cstring(out, i+100, (const char*) &buffer);
        weather_to_fetch = true;
      }
    }
  }
      
  if ( weather_to_fetch == true ) {
    dict_write_int(out, KEY_WEATHER_TYPE, &value, sizeof(int), true);
    start_spinner();
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchHourlyWeather(): no weather to fetch");
  }
  
  result = app_message_outbox_send();
  if(result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchHourlyWeather(): error app_message_outbox_send: %d", result);
    return false;
  }
      
  return true;
}

static bool fetchDailyWeather(void) {
  
  static bool weather_to_fetch = false;
  
  DictionaryIterator *out;
  AppMessageResult result = app_message_outbox_begin(&out);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchDailyWeather(): error app_message_outbox_begin: %d", result);
    return false;
  }
  
  int value = 3;
  for(int i=0; i<MAX_CITIES; i++) {
    if(strlen(cities[i].name)) {

      if ( (time_t) time(NULL) < current_daily[i].timestamp + FORECAST_INTERVAL_SECS ) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "fetchDailyWeather(): daily forecast current for %lu (%d), ignore request",
                cities[i].city_id, i);
      } else {                  
        APP_LOG(APP_LOG_LEVEL_INFO, "fetchDailyWeather(): request city %lu", cities[i].city_id);
        static char buffer[100];
        snprintf(buffer, sizeof(buffer), "%lu,%s", cities[i].city_id, cities[i].tz);
        dict_write_cstring(out, i+100, (const char*) &buffer);
        weather_to_fetch = true;
      }
    }
  }
  
  if ( weather_to_fetch == true ) {
    dict_write_int(out, KEY_WEATHER_TYPE, &value, sizeof(int), true);
    start_spinner();
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchDailyWeather(): no weather to fetch");
  }

  result = app_message_outbox_send();
  if(result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchDailyWeather(): error app_message_outbox_send: %d", result);
    return false;
  }
  
  return true;
}

static void initFetchCurrentWeather(void *data) {
  
  static uint8_t retries = 1;
  
  if ( connection_service_peek_pebble_app_connection() ) {
    if ( ready == 0 ) {
      APP_LOG(APP_LOG_LEVEL_INFO, "initFetchCurrentWeather(): JS not ready, wait 250ms");
      app_timer_register(retries * 250, initFetchCurrentWeather, NULL);
    } else {
      bool result = fetchCurrentWeather();
      if ( !result ) {
        APP_LOG(APP_LOG_LEVEL_INFO, "initFetchCurrentWeather(): request failed, trying again in 250ms");
        app_timer_register(retries * 250, initFetchCurrentWeather, NULL);
      }
    }
    
    retries++;
  }
}

static void initFetchHourlyWeather(void *data) {
  
  static uint8_t retries = 1;
  
  if ( connection_service_peek_pebble_app_connection() ) {
    if ( ready == 0 ) {
      APP_LOG(APP_LOG_LEVEL_INFO, "initFetchHourlyWeather(): JS not ready, wait 250ms");
      app_timer_register(retries * 500, initFetchHourlyWeather, NULL);
    } else {
      bool result = fetchHourlyWeather();
      if ( !result ) {
        APP_LOG(APP_LOG_LEVEL_INFO, "initFetchHourlyWeather(): request failed, trying again in 250ms");
        app_timer_register(retries * 500, initFetchHourlyWeather, NULL);
      }
    }
    
    retries++;
  }
}

static void initFetchDailyWeather(void *data) {
  
  static uint8_t retries = 1;

  if ( connection_service_peek_pebble_app_connection() ) {
    if ( ready == 0 ) {
      APP_LOG(APP_LOG_LEVEL_INFO, "initFetchDailyWeather(): JS not ready, wait 250ms");
      app_timer_register(retries * 750, initFetchDailyWeather, NULL);
    } else {
      bool result = fetchDailyWeather();
      if ( !result ) {
        APP_LOG(APP_LOG_LEVEL_INFO, "initFetchDailyWeather(): request failed, trying again in 250ms");
        app_timer_register(retries * 750, initFetchDailyWeather, NULL);
      }
    }
  
    retries++;
  }
}
  
void get_weather_icon(uint16_t code, time_t timestamp, char* ascii, int buffersize) {

  time_t sunrise = current_weather[app_settings.current_city].sunrise;
  time_t sunset = current_weather[app_settings.current_city].sunset;

  if ( sunrise - timestamp < 0 && timestamp > sunset ) {
    sunrise = sunrise + ( 24 * 60 * 60 );
  }

  if ( sunset - timestamp < 0 && timestamp > sunrise ) {
    sunset = sunset + ( 24 * 60 * 60 );
  }
  
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "get_weather_icon() timestamp=%lu, sunrise=%lu, sunset=%lu", timestamp, sunrise, sunset);

  int item = 0;
  for(int c = 0; c < 83; ++c) {
    if ( WEATHER_DAY_ICONS_CODE_DEF[c].code == code ) {
      item = c;
    }
  }
  
  if ( timestamp == (time_t) 0 || ( timestamp > sunrise && timestamp < sunset ) ) {
    strncpy(ascii, WEATHER_DAY_ICONS_CODE_DEF[item].ascii, buffersize-1);
  } else {
    strncpy(ascii, WEATHER_NIGHT_ICONS_CODE_DEF[item].ascii, buffersize-1);
  }
}

static void bg_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, BGCOLOR);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void current_weather_update_proc(Layer *layer, GContext *ctx) {
  
  uint8_t item = app_settings.current_city;
  
//  APP_LOG(APP_LOG_LEVEL_DEBUG, "current_weather_update_proc(): showing city %lu (%d)", current_weather[item].city_id, item);
  
  time_t now = time(NULL);
  
 // snprintf(current_weather.city, sizeof(current_weather.city), "Hastings on the Hudson");

  // 2x LONG
  //if ( item == 0 ) {
    //snprintf(current_weather[item].city, sizeof(current_weather[item].city), "Magallanes y la Antártica Chilena");
      //snprintf(current_weather[item].city, sizeof(current_weather[item].city), "\U000027A4 Magallanes");
    //snprintf(current_weather[item].description, sizeof(current_weather[item].description), "light intensity drizzle");
  //}
  
  time_t timestamp;
  int16_t temperature;
  int16_t code;
  static char description[40];
  
  timestamp = current_weather[item].timestamp;
  temperature = current_weather[item].temperature;
  code = current_weather[item].code;
  strncpy(description, current_weather[item].description, sizeof(description));
  
  if ( timestamp == 0 && current_hourly[item].timestamp != 0 ) {
    timestamp = current_hourly[item].timestamp;
    for(int i=0; i < 10; i++) {
      if ( current_hourly[item].items[i].timestamp >= now && current_hourly[item].items[i].timestamp <= now + 3600 ) {
        temperature = current_hourly[item].items[i].temperature;
        code = current_hourly[item].items[i].code;
        break;
      }
    }
  }
  
  if ( timestamp > 0 ) {
    
    text_layer_set_text(city_textlayer, current_weather[item].city);
    //GSize size = text_layer_get_content_size(city_textlayer);
    //GRect bounds = layer_get_bounds(city_layer);
//    bounds.origin.y = bounds.size.h - 37 - size.h;
    //bounds.origin.y = 36 - ( size.h / 2 );
    //layer_set_bounds(city_layer, bounds);

    static char main_icon[16];
    get_weather_icon(code, now, main_icon, sizeof(main_icon));
    text_layer_set_text(icon_textlayer, main_icon); 

    static char temperature_text[8];
    snprintf(temperature_text, sizeof(temperature_text), "%d°", convert_temp(temperature));
    text_layer_set_text(temperature_textlayer, temperature_text);

    text_layer_set_text(description_textlayer, description);
    
    #ifdef SCREENSHOT 
    text_layer_set_text(city_textlayer, "New York");
    text_layer_set_text(icon_textlayer, "\U0000f00d"); 
    text_layer_set_text(temperature_textlayer, "72°");
    text_layer_set_text(description_textlayer, "clear sky");
    #endif
    
    GSize citytext_size = text_layer_get_content_size(city_textlayer);
    GSize descriptiontext_size = text_layer_get_content_size(description_textlayer);
    GRect weather_bounds = layer_get_bounds(weather_layer);
    weather_bounds.origin.y = weather_bounds.origin.y + ( (citytext_size.h - 18) / 2);
    weather_bounds.origin.y = ( (citytext_size.h - 18) / 2);
    layer_set_bounds(weather_layer, weather_bounds);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "current_weather_update_proc(): weather_bounds=%d", weather_bounds.origin.y);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "current_weather_update_proc(): city_textlayer=%d", text_layer_get_content_size(city_textlayer).h);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "current_weather_update_proc(): icon_textlayer=%d", text_layer_get_content_size(icon_textlayer).h);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "current_weather_update_proc(): temperature_textlayer=%d", text_layer_get_content_size(temperature_textlayer).h);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "current_weather_update_proc(): description_textlayer=%d", text_layer_get_content_size(description_textlayer).h);
 
    time_t sunrise = current_weather[item].sunrise;
    time_t sunset = current_weather[item].sunset;

    if ( sunrise - now < 0 ) {
      sunrise = sunrise + ( 24 * 60 * 60 );
    }

    if ( sunset - now < 0 ) {
      sunset = sunset + ( 24 * 60 * 60 );
    }
        
    static char text[11][32];
    static char temperature_per_hour[11][8];
    static char icon[11][16];
    uint8_t line = 0;
    for(int i=0; i < 9; i++) {

      timestamp = 0;
      if ( current_hourly[item].items[i].timestamp ) {
        timestamp = current_hourly[item].items[i].timestamp;
      }
      
      struct tm *t_sunrise;
      struct tm *t_sunset;
    
      if ( item == 0 ) {
        t_sunrise = localtime(&sunrise);
      } else {
        time_t sunrise2 = sunrise + current_hourly[item].utcoffset;
        t_sunrise = gmtime(&sunrise2);
      }
      if ( timestamp - sunrise > 0 && timestamp - sunrise < 10800 ) {
      
        if ( clock_is_24h_style() ) {
          strftime(text[line], sizeof(text[line]), "%k:%M", t_sunrise);
        } else {
          strftime(text[line], sizeof(text[line]), "%l:%M%p", t_sunrise);
        }
        text_layer_set_text(hourly_time_textlayer[line], text[line]);
        get_weather_icon(998, timestamp, icon[line], sizeof(icon[line]));
        text_layer_set_text(hourly_icon_textlayer[line], icon[line]);
        text_layer_set_text(hourly_temperature_textlayer[line], "sunrise");
        
        line++;
      }
      
      if ( item == 0 ) {
        t_sunset = localtime(&sunset);
      } else {
        time_t sunset2 = sunset + current_hourly[item].utcoffset;
        t_sunset = gmtime(&sunset2);
      }
      
      if (  timestamp - sunset > 0 && timestamp - sunset < 10800 ) {
        
        if ( clock_is_24h_style() ) {
          strftime(text[line], sizeof(text[line]), "%k:%M", t_sunset);
        } else {
          strftime(text[line], sizeof(text[line]), "%l:%M%p", t_sunset);
        }
        text_layer_set_text(hourly_time_textlayer[line], text[line]);
        get_weather_icon(999, timestamp, icon[line], sizeof(icon[line]));
        text_layer_set_text(hourly_icon_textlayer[line], icon[line]);
        text_layer_set_text(hourly_temperature_textlayer[line], "sunset");

        line++;
      }

      if ( timestamp > now ) {
                
        struct tm *t = localtime(&timestamp);
        if ( item > 0 ) {
          time_t timestamp2 = timestamp + current_hourly[item].utcoffset;
          t = gmtime(&timestamp2);
        }
        
        if ( t->tm_hour < 13 ) {
          if ( t->tm_hour == 12 && ! clock_is_24h_style() ) {
            snprintf(text[line], sizeof(text[line]), "noon");
          } else {
            if ( t->tm_hour == 0  && ! clock_is_24h_style() ) {
              snprintf(text[line], sizeof(text[line]), "midnight");
            } else {
              if ( clock_is_24h_style() ) {
                snprintf(text[line], sizeof(text[line]), "%d:00", t->tm_hour);                
              } else {
                snprintf(text[line], sizeof(text[line]), "%dAM", t->tm_hour);
              }
            }
          }

        } else {
          if ( clock_is_24h_style() ) {
            snprintf(text[line], sizeof(text[line]), "%d:00", t->tm_hour);
          } else {
            snprintf(text[line], sizeof(text[line]), "%dPM", t->tm_hour - 12);      
          }
        }
        text_layer_set_text(hourly_time_textlayer[line], text[line]);

        get_weather_icon(current_hourly[item].items[i].code, timestamp, icon[line], sizeof(icon[line]));
        text_layer_set_text(hourly_icon_textlayer[line], icon[line]);

        snprintf(temperature_per_hour[line], sizeof(temperature_per_hour[line]), "%d°", convert_temp(current_hourly[item].items[i].temperature));
        text_layer_set_text(hourly_temperature_textlayer[line], temperature_per_hour[line]);
        
        #ifdef SCREENSHOT 
        text_layer_set_text(hourly_icon_textlayer[line], "\U0000f00d");
        text_layer_set_text(hourly_temperature_textlayer[line], "72°");
        #endif
        
        line++;
        if ( line > 10 ) {
          break;
        }

      } else {
        if ( timestamp == 0 ) {
          if ( line < 11 ) {
            text_layer_set_text(hourly_time_textlayer[line], "- -");
            text_layer_set_text(hourly_temperature_textlayer[line], "- -");
          }
          
          line++;
          if ( line > 10 ) {
            break;
          }
        }
      }
    }
  } else {
    text_layer_set_text(city_textlayer, "- - - -");
    //GSize size = text_layer_get_content_size(city_textlayer);
    //GRect bounds = layer_get_bounds(city_layer);
    //bounds.origin.y = 48 - size.h;
    //layer_set_bounds(city_layer, bounds);
    
    text_layer_set_text(temperature_textlayer, "- -");

    text_layer_set_text(description_textlayer, "- - - - -");
    
    for(int i=0; i < 10; i++) {
      text_layer_set_text(hourly_time_textlayer[i], "- -");
      text_layer_set_text(hourly_temperature_textlayer[i], "- -");
    }
  }
  
  #if defined(PBL_ROUND)
  graphics_context_set_fill_color(ctx, FGCOLOR);
  graphics_fill_circle(ctx, GPoint(180,90), 12);
  #endif
}

static void forecast_weather_update_proc(Layer *layer, GContext *ctx) {
  
  uint8_t item = app_settings.current_city;

  if ( current_daily[item].timestamp > 0 ) {
  
    static char weekdays[][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
      
    time_t now = time(NULL);
    struct tm *t_now = localtime(&now);
    int my_current_yday = t_now->tm_yday;
    
    static char text[6][32];
    static char temperature_per_day[6][8];
    static char temperature_per_night[6][8];
    static char icon[6][16];
    int line = 0;
    for(int i=0; i < 6; i++) {
      
      if ( current_daily[item].items[i].timestamp ) {
        time_t item_now = now + current_daily[item].utcoffset;
        struct tm *t_item_now = gmtime(&item_now);
        int item_current_yday = t_item_now->tm_yday;
        
        time_t timestamp = current_daily[item].items[i].timestamp;
        struct tm *t = localtime(&timestamp);
        if ( item > 0 ) {
          time_t timestamp2 = timestamp + current_daily[item].utcoffset;
          t = gmtime(&timestamp2);
        }
                    
        if ( t->tm_yday >= item_current_yday ) {
          if ( line == 0 && my_current_yday == t->tm_yday ) {
            snprintf(text[line], sizeof(text[line]), "Today");
          } else {
            snprintf(text[line], sizeof(text[line]), weekdays[t->tm_wday]);
          }
        
          text_layer_set_text(daily_time_textlayer[line], text[line]);
          
          get_weather_icon(current_daily[item].items[i].code, (time_t) 0, icon[line], sizeof(icon[line]));
          text_layer_set_text(daily_icon_textlayer[line], icon[line]);
    
          snprintf(temperature_per_day[line], sizeof(temperature_per_day[line]), "%d", convert_temp(current_daily[item].items[i].temperature_max));
          text_layer_set_text(daily_temperature_textlayer[line], temperature_per_day[line]);
          
          snprintf(temperature_per_night[line], sizeof(temperature_per_night[line]), "%d°", convert_temp(current_daily[item].items[i].temperature_min));
          text_layer_set_text(daily_night_temperature_textlayer[line], temperature_per_night[line]);
          
          line++;
        }
      } else {
        text_layer_set_text(daily_time_textlayer[line], "- -");
        text_layer_set_text(daily_temperature_textlayer[line], "- -");
        text_layer_set_text(daily_night_temperature_textlayer[line], "- -");
        
        line++;
      }
    }
    } else {
    for(int i=0; i < 6; i++) {
      text_layer_set_text(daily_time_textlayer[i], "- -");
      text_layer_set_text(daily_temperature_textlayer[i], "- -");
      text_layer_set_text(daily_night_temperature_textlayer[i], "- -");
    }
  }
}

void scroll_down()
{
  GPoint pos = scroll_layer_get_content_offset(current_weather_scrolllayer);
  
  if ( pos.y > -180 ) {
    scroll_layer_set_content_offset(current_weather_scrolllayer, GPoint(0, -180), true);
  } else {
    scroll_layer_set_content_offset(current_weather_scrolllayer, GPoint(0, pos.y - 42), true);
  }
  
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "scroll_down(): pos=%d", pos.y);
}

void scroll_down_repeat() {
  scrolling_timer = app_timer_register(70, scroll_down_repeat, NULL);
  scroll_down();
}

void scroll_up() {
  GPoint pos = scroll_layer_get_content_offset(current_weather_scrolllayer);
  
  if ( pos.y >= -180 ) {
    scroll_layer_set_content_offset(current_weather_scrolllayer, GPoint(0, 0), true);
  } else {
    scroll_layer_set_content_offset(current_weather_scrolllayer, GPoint(0, pos.y + 42), true);
  }
  
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "scroll_up(): pos=%d", pos.y);
}

void scroll_up_repeat() {
  scrolling_timer = app_timer_register(70, scroll_up_repeat, NULL);
  scroll_up();
}

void scroll_top() {
  scroll_layer_set_content_offset(current_weather_scrolllayer, GPoint(0, 0), true);
}

void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  scroll_down();
}

void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  scroll_down_repeat();
}

void down_long_release_handler(ClickRecognizerRef recognizer, void *context) {
  app_timer_cancel(scrolling_timer);
}

void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  scroll_up();
}

void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  scroll_up_repeat();
}

void up_long_release_handler(ClickRecognizerRef recognizer, void *context) {
  app_timer_cancel(scrolling_timer);
}

void back_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  GPoint pos = scroll_layer_get_content_offset(current_weather_scrolllayer);
  
  if ( pos.y == 0 ) {
    window_stack_pop_all(true);
  } else {
    scroll_top();
  }
}

static void forecast_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  forecast_weather_layer = layer_create(bounds);
  layer_set_update_proc(forecast_weather_layer, forecast_weather_update_proc);
  layer_add_child(window_layer, forecast_weather_layer);
  
  forecast_weather_scrolllayer = scroll_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  scroll_layer_set_click_config_onto_window(forecast_weather_scrolllayer, window);
  scroll_layer_set_shadow_hidden(forecast_weather_scrolllayer, true); 
  layer_add_child(forecast_weather_layer, scroll_layer_get_layer(forecast_weather_scrolllayer));
  
  forecast_weather_indicator = scroll_layer_get_content_indicator(forecast_weather_scrolllayer);

  forecast_weather_indicator_up_layer = layer_create(GRect(bounds.origin.x, bounds.origin.y, 
                                      bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
  forecast_weather_indicator_down_layer = layer_create(GRect(0, bounds.size.h - STATUS_BAR_LAYER_HEIGHT, 
                                        bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
  layer_add_child(window_layer, forecast_weather_indicator_up_layer);
  layer_add_child(window_layer, forecast_weather_indicator_down_layer);

  const ContentIndicatorConfig up_config_forecast = (ContentIndicatorConfig) {
    .layer = forecast_weather_indicator_up_layer,
    .times_out = false,
    .alignment = GAlignCenter,
    .colors = {
      .foreground = FGCOLOR,
      .background = BGCOLOR
    }
  };
  content_indicator_configure_direction(forecast_weather_indicator, ContentIndicatorDirectionUp, 
                                        &up_config_forecast);

  const ContentIndicatorConfig down_config_forecast = (ContentIndicatorConfig) {
    .layer = forecast_weather_indicator_down_layer,
    .times_out = false,
    .alignment = GAlignCenter,
    .colors = {
      .foreground = FGCOLOR,
      .background = BGCOLOR
    }
  };
  content_indicator_configure_direction(forecast_weather_indicator, ContentIndicatorDirectionDown, 
                                        &down_config_forecast);
  
  int line;
  for(int i=0; i<6; i++) {

#if defined(PBL_RECT)  
    line = bounds.size.h * 0.5 + ( i * 35 ) - 15;
#else
    line = bounds.size.h * 0.5 + ( i * 42 ) - 15;
#endif
    
    daily_time_textlayer[i] = text_layer_create(GRect(0, line, bounds.size.w / 2 - 25, 30));
#if defined(PBL_RECT)
    text_layer_set_font(daily_time_textlayer[i], font_roboto_light_14);
#else
    text_layer_set_font(daily_time_textlayer[i], font_roboto_light_18);
#endif
    text_layer_set_text_color(daily_time_textlayer[i], FGCOLOR);
    text_layer_set_background_color(daily_time_textlayer[i], GColorClear);
    text_layer_set_text_alignment(daily_time_textlayer[i], GTextAlignmentRight);
    scroll_layer_add_child(forecast_weather_scrolllayer, text_layer_get_layer(daily_time_textlayer[i]));
    
    daily_icon_textlayer[i] = text_layer_create(GRect(bounds.size.w / 2 - 15, line, bounds.size.w, 30));
#if defined(PBL_RECT)
    text_layer_set_font(daily_icon_textlayer[i], font_weather_icons_14);
#else
    text_layer_set_font(daily_icon_textlayer[i], font_weather_icons_18);
#endif    
    text_layer_set_text_color(daily_icon_textlayer[i], FGCOLOR);
    text_layer_set_background_color(daily_icon_textlayer[i], GColorClear);
    text_layer_set_text_alignment(daily_icon_textlayer[i], GTextAlignmentLeft);
    scroll_layer_add_child(forecast_weather_scrolllayer, text_layer_get_layer(daily_icon_textlayer[i]));  
    
    daily_temperature_textlayer[i] = text_layer_create(GRect(bounds.size.w / 2 + 17, line, bounds.size.w, 30));
#if defined(PBL_RECT)
    text_layer_set_font(daily_temperature_textlayer[i], font_roboto_regular_14);
#else
    text_layer_set_font(daily_temperature_textlayer[i], font_roboto_regular_18);
#endif
    text_layer_set_text_color(daily_temperature_textlayer[i], FGCOLOR);
    text_layer_set_background_color(daily_temperature_textlayer[i], GColorClear);
    text_layer_set_text_alignment(daily_temperature_textlayer[i], GTextAlignmentLeft);
    scroll_layer_add_child(forecast_weather_scrolllayer, text_layer_get_layer(daily_temperature_textlayer[i]));  
    
    daily_night_temperature_textlayer[i] = text_layer_create(GRect(bounds.size.w / 2 + 45, line, bounds.size.w, 30));
#if defined(PBL_RECT)
    text_layer_set_font(daily_night_temperature_textlayer[i], font_roboto_light_14);
#else
    text_layer_set_font(daily_night_temperature_textlayer[i], font_roboto_light_18);
#endif
    text_layer_set_text_color(daily_night_temperature_textlayer[i], FGCOLOR);
    text_layer_set_background_color(daily_night_temperature_textlayer[i], GColorClear);
    text_layer_set_text_alignment(daily_night_temperature_textlayer[i], GTextAlignmentLeft);
    scroll_layer_add_child(forecast_weather_scrolllayer, text_layer_get_layer(daily_night_temperature_textlayer[i]));
  }
  
  int16_t total_length = ((bounds.size.h * 0.5) + line);
  //uint16_t total_length = 1500;
  scroll_layer_set_content_size(forecast_weather_scrolllayer, GSize(bounds.size.w, total_length));
    
  APP_LOG(APP_LOG_LEVEL_DEBUG, "forecast_window_load(): %lu", (uint32_t) heap_bytes_free());
}

static void forecast_window_unload(Window *window) {
    
  for(int i=0; i<6; i++) {
    text_layer_destroy(daily_time_textlayer[i]);
    text_layer_destroy(daily_icon_textlayer[i]);
    text_layer_destroy(daily_temperature_textlayer[i]);
    text_layer_destroy(daily_night_temperature_textlayer[i]);
  }
  
  content_indicator_destroy(forecast_weather_indicator);
  layer_destroy(forecast_weather_indicator_down_layer);
  layer_destroy(forecast_weather_indicator_up_layer);
  scroll_layer_destroy(forecast_weather_scrolllayer);
  layer_destroy(forecast_weather_layer);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "forecast_window_unload(): %lu", (uint32_t) heap_bytes_free());
}

static void show_forecast() {
  forecast_window = window_create();
  window_set_window_handlers(forecast_window, (WindowHandlers) {
    .load = forecast_window_load,
    .unload = forecast_window_unload,
  });
  window_set_background_color(forecast_window, BGCOLOR);
  window_stack_push(forecast_window, true);
}

static void create_city_menu(char *city) {
  
  strncpy(city_menu[0].title, "Searching for:", sizeof(city_menu[0].title));
  strncpy(city_menu[0].items[0].title, city, sizeof(city_menu[0].items[0].title));
  
  if ( strlen(city_search_results[0].label) > 0 ) {
  strncpy(city_menu[1].title, "Found:", sizeof(city_menu[1].title));    
    for(int i=0; i < MAX_SEARCH_CITIES; i++) {
      if ( strlen(city_search_results[i].label) > 0 ) {
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "create_city_menu(): city result %d = %s", i, city_search_results[i].label );
        strncpy(city_menu[1].items[i].title, city_search_results[i].label, sizeof(city_menu[1].items[i].title));
        city_menu[1].items[i].city_item = city_search_results[i].city_id;
      }
    }
  } else {
    if ( city_search_results[0].city_id == 1 ) {
      strncpy(city_menu[1].title, "No cities found.", sizeof(city_menu[1].title));
    }
  }
  
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "create_city_menu(): rebuild menu structure");
}

static void city_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    
  if ( cell_index->section == 1 ) {
    uint32_t city_id = city_menu[cell_index->section].items[cell_index->row].city_item;
    
    uint8_t item_id = 0;
    for(int i = 0; i < MAX_CITIES; i++) {
      if ( strlen(cities[i].name) == 0) {
        item_id = i;
        i = MAX_CITIES;
      }
      
      if ( city_id == cities[i].city_id ) {
        i = MAX_CITIES;
      }
    }
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "city_menu_select_callback(): found item %d for %lu", item_id, city_id);
    
    if ( item_id > 0 ) {
      
      uint8_t search_item_id = 0;
      for(int i = 0; i < MAX_SEARCH_CITIES; i++) {
        if ( city_search_results[i].city_id == city_id ) {
          search_item_id = i;
          i = MAX_SEARCH_CITIES;
        }
      }
      
      cities[item_id].city_id = city_search_results[search_item_id].city_id;
      strncpy(cities[item_id].name, city_search_results[search_item_id].name, sizeof(cities[item_id].name));
      strncpy(cities[item_id].tz, city_search_results[search_item_id].tz, sizeof(cities[item_id].tz));
      
      current_hourly[item_id] = current_hourly_empty;
      current_weather[item_id] = current_weather_empty;
      current_daily[item_id] = current_daily_empty;
      
      initFetchCurrentWeather(NULL);
      initFetchHourlyWeather(NULL);
      initFetchDailyWeather(NULL);
      
      app_settings.current_city = item_id;
      
      save_settings();
      
      save_cities();
      
      log_cities();
    }
    
    window_stack_pop(true);
  } else {
    //show_city_search("Blah");
    //window_stack_remove(menu_window, true);
    dictation_session_start(dictation_session);
  }
  
}

static uint16_t city_menu_get_num_sections_callback(MenuLayer *menu_layer, void *city) {
  create_city_menu(city);
  
  uint8_t total_items = 0;
  for(int i=0; i < 2; i++) {
    if (strlen(city_menu[i].title) > 0) {
      total_items++;
    } else {
      i = 2;
    }
  }
  
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "city_menu_get_num_sections_callback(): total_items=%d", total_items);
  
  return(total_items);
}

static void city_menu_draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *data) {

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "city_menu_draw_header_callback(): section_index=%d", section_index);

  GRect bounds = layer_get_bounds(cell_layer);
  
  if ( city_menu[section_index].title ) {
    if ( section_index == 0 ) {
      graphics_draw_text(ctx, city_menu[section_index].title, fonts_get_system_font(FONT_KEY_GOTHIC_18), bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    } else {
      graphics_draw_text(ctx, city_menu[section_index].title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
  }
}

int16_t city_menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
  if ( section_index == 0 ) {
    return(22);
  } else {
    return(22);
  }
}

static uint16_t city_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *city) {
  create_city_menu(city);
  
  uint8_t total_items = 0;
  for(int j=0; j < 10; j++) {
    if(strlen(city_menu[section_index].items[j].title) > 0 ) {
      total_items++;
    } else {
      j = 10;
    }
  }
  
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "city_menu_get_num_rows_callback(): section=%d total_items=%d", section_index, total_items);
  
  return(total_items);
}

static void city_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "city_menu_draw_row_callback(): draw row=%d,%d", cell_index->section, cell_index->row);
  
  GRect bounds = layer_get_bounds(cell_layer);
  
  if ( city_menu[cell_index->section].title )
  {
    if ( cell_index->section == 0 ) {
      if ( city_menu[cell_index->section].items[cell_index->row].title ) {
        if ( strlen(city_menu[cell_index->section].items[cell_index->row].subtitle) > 0 ) {
          menu_cell_basic_draw(ctx, cell_layer, city_menu[cell_index->section].items[cell_index->row].title, city_menu[cell_index->section].items[cell_index->row].subtitle, NULL);
        } else {
          menu_cell_basic_draw(ctx, cell_layer, city_menu[cell_index->section].items[cell_index->row].title, NULL, NULL);      
        }
      }
    } else {
      graphics_draw_text(ctx, city_menu[cell_index->section].items[cell_index->row].title, fonts_get_system_font(FONT_KEY_GOTHIC_18), bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
  }  
}

static bool fetchCities(char *city) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fetchCities(): send request, search for: %s", city);
  
  for(int i=0; i < MAX_SEARCH_CITIES; i++) {
    city_search_results[i] = city_search_results_empty;
  }
  
  DictionaryIterator *out;
  AppMessageResult result = app_message_outbox_begin(&out);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchCities(): error app_message_outbox_begin: %d", result);
    return false;
  }

  int value = 4;
  dict_write_int(out, KEY_WEATHER_TYPE, &value, sizeof(int), true);
  dict_write_cstring(out, 100, city);

  result = app_message_outbox_send();
  if(result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_INFO, "fetchCities(): error app_message_outbox_send");
    return false;
  }
    
  return true;  
}

static void search_city(void *city) {
  if ( ready == 0 ) {
    APP_LOG(APP_LOG_LEVEL_INFO, "search_city(): JS not ready, wait 250ms");
    app_timer_register(250, search_city, city);
  } else {
    bool result = fetchCities(city);
    if ( !result ) {
      APP_LOG(APP_LOG_LEVEL_INFO, "search_city(): request failed, trying again in 250ms");
      app_timer_register(250, search_city, city);
    }
  }  
}

static void edit_city_dictation_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  if ( status == DictationSessionStatusSuccess ) {
    strncpy(city_transcription, transcription, sizeof(city_transcription));
    search_city(city_transcription);
    window_set_user_data(city_window, city_transcription);
    layer_mark_dirty(window_get_root_layer(city_window));
  }
}

static void city_window_load(Window *window) {
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "city_window_load():");
  
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  char *city = window_get_user_data(window);
    
  city_menulayer = menu_layer_create(bounds);

  menu_layer_set_callbacks(city_menulayer, city, (MenuLayerCallbacks){
    .get_num_sections = city_menu_get_num_sections_callback,
    .get_header_height = city_menu_get_header_height_callback,
    .draw_header = city_menu_draw_header_callback,
    .get_num_rows = city_menu_get_num_rows_callback,
    .draw_row = city_menu_draw_row_callback,
    .select_click = city_menu_select_callback,
  });

  menu_layer_set_click_config_onto_window(city_menulayer, window);
  menu_layer_set_selected_index(city_menulayer, MenuIndex(0,1), MenuRowAlignCenter, false);
  
  menu_layer_set_normal_colors(city_menulayer, BGCOLOR, FGCOLOR);
  menu_layer_set_highlight_colors(city_menulayer, FGCOLOR, BGCOLOR);

  layer_add_child(window_layer, menu_layer_get_layer(city_menulayer));
  
  dictation_session = dictation_session_create(128, edit_city_dictation_callback, NULL);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "city_window_load(): %lu", (uint32_t) heap_bytes_free());
}

static void city_window_unload(Window *window) {
  menu_layer_destroy(city_menulayer);
  dictation_session_destroy(dictation_session);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "city_window_unload(): %lu", (uint32_t) heap_bytes_free());
}

static void show_city_search(char *city) {
  search_city(city);

  city_window = window_create();
  window_set_window_handlers(city_window, (WindowHandlers) {
    .load = city_window_load,
    .unload = city_window_unload,
  });
  window_set_user_data(city_window, city);
  window_set_background_color(city_window, BGCOLOR);
  window_stack_push(city_window, true);
}

static void add_city_dictation_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  if ( status == DictationSessionStatusSuccess ) {
    strncpy(city_transcription, transcription, sizeof(city_transcription));
    window_stack_remove(menu_window, true);
    show_city_search(city_transcription);
  }
}

static void create_action_menu() {
  
  for(int i = 0; i < MAX_CITIES+3; i++) {
    action_menu_items[i] = action_menu_item_empty;
  }

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "create_action_menu(): rebuild menu structure");
  
  uint8_t item = 0;
  if ( app_settings.current_city != 0 ) {
    action_menu_items[item] = (MenuItems) { .title = "delete city", .type = MENU_ITEMS_TYPE_DELETECITY };
    strncpy(action_menu_items[item].subtitle, cities[app_settings.current_city].name, sizeof(action_menu_items[item].subtitle));
    item++;
  }
  
  uint8_t total_cities = 0;
  for(int i=0; i<MAX_CITIES; i++) {
    if ( strlen(cities[i].name) > 0 ) {
      total_cities++;
    }
  }
  if ( total_cities < MAX_CITIES) {
    action_menu_items[item] = (MenuItems) { .title = "add city", .subtitle = "say city name or zipcode", .type = MENU_ITEMS_TYPE_ADDCITY };
    item++;
  }
  
  action_menu_items[item] = (MenuItems) { .title = "5-day forecast", .type = MENU_ITEMS_TYPE_FORECAST};
  strncpy(action_menu_items[item].subtitle, current_weather[app_settings.current_city].city, sizeof(action_menu_items[1].subtitle));
  item++;
  
  for(uint8_t i=0; i<MAX_CITIES; i++) {
    if ( strlen(cities[i].name) > 0 && app_settings.current_city != i ) {
      action_menu_items[item] = (MenuItems) { .type = MENU_ITEMS_TYPE_CITY, .city_item = i };
      strncpy(action_menu_items[item].title, cities[i].name, sizeof(action_menu_items[item].title));
      if ( i == 0 && strlen(current_weather[i].city) > 0 ) {
        strncpy(action_menu_items[item].subtitle, current_weather[i].city, sizeof(action_menu_items[item].subtitle));        
      }
      item++;
    }
  }
  
  if ( app_settings.celsius == true ) {
    action_menu_items[item] = (MenuItems) { .title = "temperature units", .subtitle = "Celsius", .type = MENU_ITEMS_TYPE_TEMPERATUREUNITS };
  } else {
    action_menu_items[item] = (MenuItems) { .title = "temperature units", .subtitle = "Fahrenheit", .type = MENU_ITEMS_TYPE_TEMPERATUREUNITS };
  }
}

static void action_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  
  switch(action_menu_items[cell_index->row].type) {
    
    case MENU_ITEMS_TYPE_DELETECITY:
      delete_city(app_settings.current_city);
      save_cities();
      save_weather();
      save_daily_weather();
      save_hourly_weather();
      
      app_settings.current_city--;
      save_settings();
    
      scroll_top();
      window_stack_remove(menu_window, true);
      break;
    
    case MENU_ITEMS_TYPE_ADDCITY:
      //window_stack_remove(menu_window, true);
      //show_city_search("Blah");
      dictation_session_start(dictation_session);
      break;
    
    case MENU_ITEMS_TYPE_FORECAST:
      show_forecast();
      scroll_top();
      window_stack_remove(menu_window, true);
      break;
    
    case MENU_ITEMS_TYPE_TEMPERATUREUNITS:
      if ( app_settings.celsius == true ) {
          app_settings.celsius = false;
        } else {
          app_settings.celsius = true;
        }
        save_settings();
        menu_layer_reload_data(menu_layer);
        break;
    
    case MENU_ITEMS_TYPE_CITY:
        app_settings.current_city = action_menu_items[cell_index->row].city_item;
        save_settings();
        scroll_top();
        window_stack_remove(menu_window, true);
        break;
  }
}

static uint16_t action_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  
  create_action_menu();
  
  uint8_t total_items = 0;
  for(int i=0; i < MAX_CITIES + 2; i++) {
    if (strlen(action_menu_items[i].title)) {
      total_items++;
    } else {
      i=MAX_CITIES + 2;
    }
  }
    
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "action_menu_get_num_rows_callback(): total_items=%d", total_items);
  
  return(total_items);
}

static void action_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  
  if ( action_menu_items[cell_index->row].title )
  {
    if ( strlen(action_menu_items[cell_index->row].subtitle) > 0 ) {
      menu_cell_basic_draw(ctx, cell_layer, action_menu_items[cell_index->row].title, action_menu_items[cell_index->row].subtitle, NULL);
    } else {
      menu_cell_basic_draw(ctx, cell_layer, action_menu_items[cell_index->row].title, NULL, NULL);      
    }
  }  
}

static void menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
    
  action_menulayer = menu_layer_create(bounds);

  menu_layer_set_callbacks(action_menulayer, NULL, (MenuLayerCallbacks){
    .get_num_rows = action_menu_get_num_rows_callback,
    .draw_row = action_menu_draw_row_callback,
    .select_click = action_menu_select_callback,
  });

  menu_layer_set_click_config_onto_window(action_menulayer, window);
  
  if ( app_settings.current_city == 0 ) {
    menu_layer_set_selected_index(action_menulayer, MenuIndex(0,1), MenuRowAlignCenter, false);
  } else {
    menu_layer_set_selected_index(action_menulayer, MenuIndex(0,2), MenuRowAlignCenter, false);    
  }
  
  menu_layer_set_normal_colors(action_menulayer, BGCOLOR, FGCOLOR);
  menu_layer_set_highlight_colors(action_menulayer, FGCOLOR, BGCOLOR);

  layer_add_child(window_layer, menu_layer_get_layer(action_menulayer));

  dictation_session = dictation_session_create(128, add_city_dictation_callback, NULL);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "menu_window_load(): %lu", (uint32_t) heap_bytes_free());
}

static void menu_window_unload(Window *window) {
  menu_layer_destroy(action_menulayer);
  dictation_session_destroy(dictation_session);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "menu_window_unload(): %lu", (uint32_t) heap_bytes_free());
}

static void launch_menu() {
  menu_window = window_create();
  window_set_window_handlers(menu_window, (WindowHandlers) {
    .load = menu_window_load,
    .unload = menu_window_unload,
  });
  window_set_background_color(menu_window, BGCOLOR);
  window_stack_push(menu_window, true);
}

void config_provider(Window *window) {
  window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
  
  window_long_click_subscribe(BUTTON_ID_DOWN, 250, down_long_click_handler, down_long_release_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 250, up_long_click_handler, up_long_release_handler);

  window_single_click_subscribe(BUTTON_ID_BACK, back_single_click_handler);
  
  window_single_click_subscribe(BUTTON_ID_SELECT, launch_menu);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  simple_bg_layer = layer_create(bounds);
  layer_set_update_proc(simple_bg_layer, bg_update_proc);
  layer_add_child(window_layer, simple_bg_layer);
  
  current_weather_layer = layer_create(bounds);
  layer_set_update_proc(current_weather_layer, current_weather_update_proc);
  layer_add_child(window_layer, current_weather_layer);
  
  current_weather_scrolllayer = scroll_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  scroll_layer_set_shadow_hidden(current_weather_scrolllayer, true); 
  layer_add_child(current_weather_layer, scroll_layer_get_layer(current_weather_scrolllayer));
  
  current_weather_indicator = scroll_layer_get_content_indicator(current_weather_scrolllayer);

  current_weather_indicator_up_layer = layer_create(GRect(bounds.origin.x, bounds.origin.y, 
                                      bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
  current_weather_indicator_down_layer = layer_create(GRect(0, bounds.size.h - STATUS_BAR_LAYER_HEIGHT, 
                                        bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
  layer_add_child(window_layer, current_weather_indicator_up_layer);
  layer_add_child(window_layer, current_weather_indicator_down_layer);

  const ContentIndicatorConfig up_config = (ContentIndicatorConfig) {
    .layer = current_weather_indicator_up_layer,
    .times_out = false,
    .alignment = GAlignCenter,
    .colors = {
      .foreground = FGCOLOR,
      .background = BGCOLOR
    }
  };
  content_indicator_configure_direction(current_weather_indicator, ContentIndicatorDirectionUp, 
                                        &up_config);

  const ContentIndicatorConfig down_config = (ContentIndicatorConfig) {
    .layer = current_weather_indicator_down_layer,
    .times_out = false,
    .alignment = GAlignCenter,
    .colors = {
      .foreground = FGCOLOR,
      .background = BGCOLOR
    }
  };
  content_indicator_configure_direction(current_weather_indicator, ContentIndicatorDirectionDown, 
                                        &down_config);

#if defined(PBL_RECT)
  city_textlayer = text_layer_create(GRect(0, 25, bounds.size.w, 40));
#else
  city_textlayer = text_layer_create(GRect(0, 30, bounds.size.w, 40));
#endif
  
  text_layer_set_font(city_textlayer, font_roboto_regular_18);
  text_layer_set_text_color(city_textlayer, FGCOLOR);
  text_layer_set_background_color(city_textlayer, GColorClear);
  text_layer_set_text_alignment(city_textlayer, GTextAlignmentCenter);
  scroll_layer_add_child(current_weather_scrolllayer, text_layer_get_layer(city_textlayer));
  
  weather_layer = layer_create(GRect(0, bounds.size.h / 2 - 25, bounds.size.w, 50));
  scroll_layer_add_child(current_weather_scrolllayer, weather_layer);
    
  //icon_textlayer = text_layer_create(GRect(bounds.size.w / 2 - 55, bounds.size.h / 2 - 12, 50, 50));
  icon_textlayer = text_layer_create(GRect(bounds.size.w / 2 - 55, 4, 50, 50));
  text_layer_set_font(icon_textlayer, font_weather_icons_30);
  text_layer_set_text_color(icon_textlayer, FGCOLOR);
  text_layer_set_background_color(icon_textlayer, GColorClear);
  text_layer_set_text_alignment(icon_textlayer, GTextAlignmentRight);
  layer_add_child(weather_layer, text_layer_get_layer(icon_textlayer));

  //temperature_textlayer = text_layer_create(GRect(bounds.size.w / 2 + 5,  bounds.size.h / 2 - 16, 90, 50));
  temperature_textlayer = text_layer_create(GRect(bounds.size.w / 2 + 5,  0, 90, 50));
  text_layer_set_font(temperature_textlayer, font_roboto_light_36);
  text_layer_set_text_color(temperature_textlayer, FGCOLOR);
  text_layer_set_background_color(temperature_textlayer, GColorClear);
  text_layer_set_text_alignment(temperature_textlayer, GTextAlignmentLeft);
  layer_add_child(weather_layer, text_layer_get_layer(temperature_textlayer));
  
  description_textlayer = text_layer_create(GRect(15,  bounds.size.h / 2 + 30, bounds.size.w - 30, 50));
  text_layer_set_font(description_textlayer, font_roboto_light_18);
  text_layer_set_text_color(description_textlayer, FGCOLOR);
  text_layer_set_background_color(description_textlayer, GColorClear);
  text_layer_set_text_alignment(description_textlayer, GTextAlignmentCenter);
//  text_layer_set_overflow_mode(description_textlayer, GTextOverflowModeWordWrap);
  scroll_layer_add_child(current_weather_scrolllayer, text_layer_get_layer(description_textlayer));
  
  int line;
  for(int i=0; i < 11; i++) {

#if defined(PBL_RECT)  
    line = bounds.size.h * 1.5 + ( i * 35 ) - 10;
#else
    line = bounds.size.h * 1.5 + ( i * 42 ) - 10;
#endif
    
    hourly_time_textlayer[i] = text_layer_create(GRect(0, line, bounds.size.w / 2 - 15, 30));
#if defined(PBL_RECT)  
    text_layer_set_font(hourly_time_textlayer[i], font_roboto_light_14);
#else
    text_layer_set_font(hourly_time_textlayer[i], font_roboto_light_18);
#endif
    text_layer_set_text_color(hourly_time_textlayer[i], FGCOLOR);
    text_layer_set_background_color(hourly_time_textlayer[i], GColorClear);
    text_layer_set_text_alignment(hourly_time_textlayer[i], GTextAlignmentRight);
    scroll_layer_add_child(current_weather_scrolllayer, text_layer_get_layer(hourly_time_textlayer[i]));
    
    hourly_icon_textlayer[i] = text_layer_create(GRect(bounds.size.w / 2 - 10, line, 25, 30));
#if defined(PBL_RECT)  
    text_layer_set_font(hourly_icon_textlayer[i], font_weather_icons_14);
#else
    text_layer_set_font(hourly_icon_textlayer[i], font_weather_icons_18);
#endif
    text_layer_set_text_color(hourly_icon_textlayer[i], FGCOLOR);
    text_layer_set_background_color(hourly_icon_textlayer[i], GColorClear);
    text_layer_set_text_alignment(hourly_icon_textlayer[i], GTextAlignmentCenter);
    scroll_layer_add_child(current_weather_scrolllayer, text_layer_get_layer(hourly_icon_textlayer[i]));  
    
    hourly_temperature_textlayer[i] = text_layer_create(GRect(bounds.size.w / 2 + 20, line, bounds.size.w, 30));
#if defined(PBL_RECT)  
    text_layer_set_font(hourly_temperature_textlayer[i], font_roboto_light_14);
#else
    text_layer_set_font(hourly_temperature_textlayer[i], font_roboto_light_18);
#endif
    text_layer_set_text_color(hourly_temperature_textlayer[i], FGCOLOR);
    text_layer_set_background_color(hourly_temperature_textlayer[i], GColorClear);
    text_layer_set_text_alignment(hourly_temperature_textlayer[i], GTextAlignmentLeft);
    scroll_layer_add_child(current_weather_scrolllayer, text_layer_get_layer(hourly_temperature_textlayer[i]));  
  }
  
  uint16_t total_length = line + bounds.size.h * 0.5 + 10;
  scroll_layer_set_content_size(current_weather_scrolllayer, GSize(bounds.size.w, total_length));
  
  spinner_bitmaplayer = bitmap_layer_create(GRect(bounds.size.w / 2 - 4,5,12,12));
  bitmap_layer_set_compositing_mode(spinner_bitmaplayer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(spinner_bitmaplayer));
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "main_window_load(): %lu", (uint32_t) heap_bytes_free());
}

static void main_window_unload(Window *window) {
  
  bitmap_layer_destroy(spinner_bitmaplayer);
  
  for(int i=0; i < 10; i++) {
    text_layer_destroy(hourly_temperature_textlayer[i]);
    text_layer_destroy(hourly_time_textlayer[i]);
    text_layer_destroy(hourly_icon_textlayer[i]);
  }

  text_layer_destroy(description_textlayer);
  text_layer_destroy(temperature_textlayer);
  text_layer_destroy(icon_textlayer);
  text_layer_destroy(city_textlayer);
  layer_destroy(weather_layer);
  layer_destroy(current_weather_indicator_up_layer);
  layer_destroy(current_weather_indicator_down_layer);
  content_indicator_destroy(current_weather_indicator);
  scroll_layer_destroy(current_weather_scrolllayer);
  layer_destroy(current_weather_layer);
  layer_destroy(simple_bg_layer);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "main_window_unload(): %lu", (uint32_t) heap_bytes_free());
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {

  Tuple *ready_key = dict_find(iter, KEY_READY);
  if (ready_key ) {
    ready = 1;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox_received_handler(): JS reports ready (%u)", (int) ready_key->value->int32);
    return;
  }
  
  Tuple *weather_type = dict_find(iter, KEY_WEATHER_TYPE);
  if ( weather_type->value->int8 == 1 )
  {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox_received_handler(): JS reports current weather");

    Tuple *code_key = dict_find(iter, WEATHER_CODE_KEY);
    Tuple *temperature_key = dict_find(iter, WEATHER_TEMPERATURE_KEY);
    Tuple *sunrise_key = dict_find(iter, WEATHER_SUNRISE_KEY);
    Tuple *sunset_key = dict_find(iter, WEATHER_SUNSET_KEY);
    Tuple *citycode_key = dict_find(iter, WEATHER_CITYCODE_KEY);    
    Tuple *city_key = dict_find(iter, WEATHER_CITY_KEY);
    Tuple *description_key = dict_find(iter, WEATHER_DESCRIPTION);
    Tuple *timestamp_key = dict_find(iter, WEATHER_TIMESTAMP_KEY);
    Tuple *current_location_key = dict_find(iter, WEATHER_CURRENT_LOCATION_KEY);
    
    if ( code_key->value->int32 > 0 ) {
      
      static uint8_t item;
      if ( current_location_key->value->int32 == 0 ) {
        item = getCurrentWeatherItem(citycode_key->value->int32);
      } else {
        item = 0;
      }
      
      // refetch forecasts when location has changed but only if there current weather in the first place
      if ( item == 0 && current_weather[item].city_id > 0 && current_weather[item].city_id !=  citycode_key->value->uint32 ) {
        current_daily[item] = current_daily_empty;
        current_hourly[item] = current_hourly_empty;
        initFetchHourlyWeather(NULL);
        initFetchDailyWeather(NULL);
      }
      
      current_weather[item].code = code_key->value->int32;
      current_weather[item].temperature = temperature_key->value->int16;
      current_weather[item].sunrise = sunrise_key->value->int32;
      current_weather[item].sunset = sunset_key->value->int32;
      current_weather[item].city_id = citycode_key->value->int32;      
      snprintf(current_weather[item].city, sizeof(current_weather[item].city), city_key->value->cstring);
      snprintf(current_weather[item].description, sizeof(current_weather[item].description), description_key->value->cstring);      
      current_weather[item].timestamp = timestamp_key->value->int32;
      
      if ( window_stack_contains_window(main_window) ) {
        layer_mark_dirty(window_get_root_layer(main_window));
      }
      
      log_weather(item);
      
      save_weather();
      
      stop_spinner();
    }
  }
  
  if ( weather_type->value->int8 == 2 ) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox_received_handler(): JS reports hourly weather");
    
    Tuple *city_id_key = dict_find(iter, WEATHER_CITY_KEY);
    Tuple *timestamp_key = dict_find(iter, WEATHER_TIMESTAMP_KEY);
    Tuple *utcoffset_key = dict_find(iter, UTC_OFFSET_KEY);
    Tuple *current_location_key = dict_find(iter, WEATHER_CURRENT_LOCATION_KEY);

    if ( city_id_key->value->uint32 > 0 ) {
      
      static uint8_t item;
      if ( current_location_key->value->int32 == 0 ) {
        item = getCurrentWeatherItem(city_id_key->value->int32);
      } else {
        item = 0;
      }
      
      current_hourly[item].city_id = city_id_key->value->uint32;
      current_hourly[item].timestamp = timestamp_key->value->uint32;
      current_hourly[item].utcoffset = utcoffset_key->value->int32;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox_received_handler(): UTC offset %ld for city %lu", current_hourly[item].utcoffset, current_hourly[item].city_id);
      
      uint8_t cnt = 0;
      for(uint8_t i = 100; i < 127; i=i+3) {
        Tuple *h_timestamp = dict_find(iter, i);
        if ( h_timestamp ) {
          current_hourly[item].items[cnt].timestamp = h_timestamp->value->uint32;
        }
        
        Tuple *h_temperature = dict_find(iter, i+1);
        if ( h_temperature ) {
          current_hourly[item].items[cnt].temperature = h_temperature->value->int16;
        }
        
        Tuple *h_weather_code = dict_find(iter, i+2);
        if ( h_weather_code ) {
          current_hourly[item].items[cnt].code = h_weather_code->value->uint16; 
        }
        
        if ( ! h_weather_code || ! h_temperature || ! h_timestamp ) {
          current_hourly[item].timestamp = 0;
        }
        
        cnt++;
      }
      
      if ( window_stack_contains_window(main_window) ) {
        layer_mark_dirty(window_get_root_layer(main_window));
      }
      
      log_hourly_weather(item);
      
      save_hourly_weather();
      
      stop_spinner();
    }
  }
  
  if ( weather_type->value->int8 == 3 ) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox_received_handler(): JS reports daily weather");
   
    Tuple *city_id_key = dict_find(iter, WEATHER_CITY_KEY);
    Tuple *timestamp_key = dict_find(iter, WEATHER_TIMESTAMP_KEY);
    Tuple *utcoffset_key = dict_find(iter, UTC_OFFSET_KEY);
    Tuple *current_location_key = dict_find(iter, WEATHER_CURRENT_LOCATION_KEY);
   
    if ( city_id_key->value->uint32 > 0 ) {
      
      static uint8_t item;
      if ( current_location_key->value->int32 == 0 ) {
        item = getCurrentWeatherItem(city_id_key->value->int32);
      } else {
        item = 0;
      }
      
      current_daily[item].city_id = city_id_key->value->uint32;
      current_daily[item].timestamp = timestamp_key->value->uint32;
      current_daily[item].utcoffset = utcoffset_key->value->int32;

      uint8_t cnt = 0;
      for(uint8_t i = 100; i < 124; i=i+4) {
        Tuple *h_timestamp = dict_find(iter, i);
        if ( h_timestamp ) {
          current_daily[item].items[cnt].timestamp = h_timestamp->value->uint32;
        }
        
        Tuple *h_temperature_min = dict_find(iter, i+1);
        if ( h_temperature_min ) {
          current_daily[item].items[cnt].temperature_min = h_temperature_min->value->int16;
        }

        Tuple *h_temperature_max = dict_find(iter, i+2);
        if ( h_temperature_max ) {
          current_daily[item].items[cnt].temperature_max = h_temperature_max->value->int16;        
        }
        
        Tuple *h_weather_code = dict_find(iter, i+3);
        if ( h_weather_code ) {
          current_daily[item].items[cnt].code = h_weather_code->value->uint16; 
        }
        
        if ( ! h_weather_code || ! h_temperature_max || ! h_temperature_min || ! h_timestamp ) {
          current_daily[item].timestamp = 0;
        }
        
        cnt++;
      }
      
      if ( window_stack_contains_window(forecast_window) ) {
        layer_mark_dirty(window_get_root_layer(forecast_window));
      }
      
      log_daily_weather(item);
      
      save_daily_weather();
      
      stop_spinner();
    }
  }
  
  if ( weather_type->value->int8 == 4 ) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox_received_handler(): JS reports city search results");
    
    Tuple *city_id_key = dict_find(iter, 100);
    if ( city_id_key ) {    
      uint8_t cnt = 0;
      for(uint8_t i = 100; i < 100 + (MAX_SEARCH_CITIES * 4); i=i+4) {
        Tuple *city_id_key = dict_find(iter, i);
        if ( city_id_key ) {
          
          bool existing_city = 0;
          for(uint8_t j=1; j < MAX_CITIES; j++) {
            if ( city_id_key->value->uint32 == cities[j].city_id ) {
              existing_city = 1;
              break;
            }
          }
          
          if ( existing_city == 1 ) {
            continue;
          }
          
          city_search_results[cnt].city_id = city_id_key->value->uint32;
        }
        
        Tuple *city_name_key = dict_find(iter, i+1);
        if ( city_name_key ) {
          snprintf(city_search_results[cnt].name, sizeof(city_search_results[cnt].name), city_name_key->value->cstring);      
        }
        
        Tuple *city_label_key = dict_find(iter, i+2);
        if ( city_label_key ) {
          snprintf(city_search_results[cnt].label, sizeof(city_search_results[cnt].label), city_label_key->value->cstring);      
        }
        
        Tuple *city_timezone_key = dict_find(iter, i+3);
        if ( city_timezone_key ) {
          snprintf(city_search_results[cnt].tz, sizeof(city_search_results[cnt].tz), city_timezone_key->value->cstring);      
        }      
        APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox_received_handler(): cities: %lu %s %s", city_search_results[cnt].city_id, city_search_results[cnt].name, city_search_results[cnt].tz);
        
        cnt++;
      }
    } else {
      city_search_results[0].city_id = 1;
    }
      
    if ( window_stack_contains_window(city_window) ) {
        menu_layer_reload_data(city_menulayer);
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

void handle_init(void) {

  app_message_open(2048, 1024);    

  read_settings();
  read_cities();
  
  read_weather();
  read_hourly_weather();
  read_daily_weather();

//  APP_LOG(APP_LOG_LEVEL_INFO, "size current_hourly_empty = %d", sizeof(current_hourly_empty));
  
  initFetchCurrentWeather(NULL);
  initFetchHourlyWeather(NULL);
  initFetchDailyWeather(NULL);
  

  font_roboto_light_36 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_LIGHT_36));
  font_weather_icons_30 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_WEATHERICONS_30));
  font_roboto_light_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_LIGHT_18));
  font_roboto_regular_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_REGULAR_18));  
  
#if defined(PBL_RECT)  
  font_roboto_light_14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_LIGHT_14));
  font_roboto_regular_14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_REGULAR_14));
  font_weather_icons_14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_WEATHERICONS_14));
#else
  font_weather_icons_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_WEATHERICONS_18));
#endif

  main_window = window_create();
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(main_window, true);
  
  if ( ! current_weather[app_settings.current_city].timestamp ) {
      show_startup_animation();    
  }
  
  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  window_set_click_config_provider(main_window, (ClickConfigProvider) config_provider);
}

void handle_deinit(void) {
  
  fonts_unload_custom_font(font_roboto_light_36);
  fonts_unload_custom_font(font_weather_icons_30);
  fonts_unload_custom_font(font_roboto_light_18);
  fonts_unload_custom_font(font_roboto_regular_18);

#if defined(PBL_RECT)  
  fonts_unload_custom_font(font_roboto_light_14);
  fonts_unload_custom_font(font_roboto_regular_14);
  fonts_unload_custom_font(font_weather_icons_14);
#else
  fonts_unload_custom_font(font_weather_icons_18);
#endif
    
  window_destroy(main_window);
  window_destroy(menu_window);
  window_destroy(city_window);
  window_destroy(forecast_window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
