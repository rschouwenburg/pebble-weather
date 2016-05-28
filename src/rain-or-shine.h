#include <pebble.h>

struct weather_icons {
  uint16_t code;
  char ascii[12];
};

typedef struct weather_icons WeatherIcons;

static const WeatherIcons WEATHER_DAY_ICONS_CODE_DEF[] = {
  { 0,   "\U0000f04c" }, // no report or unknown code
  { 200, "\U0000f010" }, //	thunderstorm with light rain	 11d
  { 201, "\U0000f010"	}, // thunderstorm with rain	 11d
  { 202, "\U0000f010"	}, // thunderstorm with heavy rain	 11d
  { 210, "\U0000f005"	}, // light thunderstorm	 11d
  { 211, "\U0000f005"	}, // thunderstorm	 11d
  { 212, "\U0000f005"	}, // heavy thunderstorm	 11d
  { 221, "\U0000f005"	}, // ragged thunderstorm	 11d
  { 230, "\U0000f010"	}, // thunderstorm with light drizzle	 11d
  { 231, "\U0000f010"	}, // thunderstorm with drizzle	 11d
  { 232, "\U0000f010"	}, // thunderstorm with heavy drizzle	 11d
    
  { 300, "\U0000f00b"	}, // light intensity drizzle	 09d
  { 301, "\U0000f00b" }, // drizzle	 09d
  { 302, "\U0000f008"	}, // heavy intensity drizzle	 09d
  { 310, "\U0000f006" }, // light intensity drizzle rain	 09d
  { 311, "\U0000f008"	}, // drizzle rain	 09d
  { 312, "\U0000f008"	}, // heavy intensity drizzle rain	 09d
  { 313, "\U0000f009"	}, // shower rain and drizzle	 09d
  { 314, "\U0000f008"	}, // heavy shower rain and drizzle	 09d
  { 321, "\U0000f01c"	}, // shower drizzle	 09d
    
  { 500, "\U0000F00B"	}, // light rain	 10d
  { 501, "\U0000f008"	}, // moderate rain	 10d
  { 502, "\U0000f008"	}, // heavy intensity rain	 10d
  { 503, "\U0000f008"	}, // very heavy rain	 10d
  { 504, "\U0000f008"	}, // extreme rain	 10d
  { 511, "\U0000f006"	}, // freezing rain	 13d
  { 520, "\U0000f009"	}, // light intensity shower rain	 09d
  { 521, "\U0000f009"	}, // shower rain	 09d
  { 522, "\U0000f009"	}, // heavy intensity shower rain	 09d
  { 531, "\U0000f00e"	}, // ragged shower rain
    
  { 600, "\U0000f00a"	}, // light snow	[[file:13d.png]]
  { 601, "\U0000f00a"	}, // snow	[[file:13d.png]]
  { 602, "\U0000f00a"	}, // heavy snow	[[file:13d.png]]
  { 611, "\U0000f0b2"	}, // sleet	[[file:13d.png]]
  { 612, "\U0000f006"	}, // shower	[[file:13d.png]]
  { 615, "\U0000f006"	}, // light rain and snow	[[file:13d.png]]
  { 616, "\U0000f006"	}, // rain and snow	[[file:13d.png]]
  { 620, "\U0000f006"	}, // light shower snow	[[file:13d.png]]
  { 621, "\U0000f00a"	}, // shower snow	[[file:13d.png]]
  { 622, "\U0000f00a"	}, // heavy shower snow	[[file:13d.png]]
  
  { 701, "\U0000f003" }, // mist	[[file:50d.png]]
  { 711, "\U0000f062"	}, // smoke	[[file:50d.png]]
  { 721, "\U0000f0b6" }, // haze	[[file:50d.png]]
  { 731, "\U0000f063"	}, // sand, dust whirls	[[file:50d.png]]
  { 741, "\U0000f003" }, // fog	[[file:50d.png]]
  { 751, "\U0000f063"	}, // sand	[[file:50d.png]]
  { 761, "\U0000f063"	}, // dust	[[file:50d.png]]
  { 762, "\U0000f063"	}, // volcanic ash	[[file:50d.png]]
  { 771, "\U0000f000"	}, // squalls	[[file:50d.png]]
  { 781, "\U0000f056"	}, // tornado	[[file:50d.png]]
    
  { 800, "\U0000f00d"	}, // clear sky	[[file:01d.png]] [[file:01n.png]]
  
  { 801, "\U0000f00c" }, //	few clouds	[[file:02d.png]] [[file:02n.png]]
  { 802, "\U0000f002"	}, // scattered clouds	[[file:03d.png]] [[file:03d.png]]
  { 803, "\U0000f002"	}, // broken clouds	[[file:04d.png]] [[file:03d.png]]
  { 804, "\U0000f002"	}, // overcast clouds	[[file:04d.png]] [[file:04d.png]]
    
  { 900, "\U0000f056" }, // tornado
  { 901, "\U0000f00e" }, // tropical storm
  { 902, "\U0000f073" }, // hurricane
  { 903, "\U0000f076"	}, // cold
  { 904, "\U0000f072"	}, // hot
  { 905, "\U0000f085"	}, // windy
  { 906, "\U0000f004" }, // hail
    
  { 951, "\U0000f00d"	}, // calm
  { 952, "\U0000f00d"	}, // light breeze
  { 953, "\U0000f00d"	}, // gentle breeze
  { 954, "\U0000f00d"	}, // moderate breeze
  { 955, "\U0000f00d"	}, // fresh breeze
  { 956, "\U0000f085"	}, // strong breeze
  { 957, "\U0000f085"	}, // high wind, near gale
  { 958, "\U0000f085"	}, // gale
  { 959, "\U0000f085"	}, // severe gale
  { 960, "\U0000f00e"	}, // storm
  { 961, "\U0000f00e"	}, // violent storm
  { 962, "\U0000f073" }, // hurricane

  { 998, "\U0000f051" }, // sunrise
  { 999, "\U0000f052" }, // sunset  
};

static const WeatherIcons WEATHER_NIGHT_ICONS_CODE_DEF[] = {
  { 0,   "\U0000f04c" }, // no report or unknown code
  { 200, "\U0000f02d" }, //	thunderstorm with light rain	 11d
  { 201, "\U0000f02d"	}, // thunderstorm with rain	 11d
  { 202, "\U0000f02d"	}, // thunderstorm with heavy rain	 11d
  { 210, "\U0000f025"	}, // light thunderstorm	 11d
  { 211, "\U0000f025"	}, // thunderstorm	 11d
  { 212, "\U0000f025"	}, // heavy thunderstorm	 11d
  { 221, "\U0000f025"	}, // ragged thunderstorm	 11d
  { 230, "\U0000f02d"	}, // thunderstorm with light drizzle	 11d
  { 231, "\U0000f02d"	}, // thunderstorm with drizzle	 11d
  { 232, "\U0000f02d"	}, // thunderstorm with heavy drizzle	 11d
    
  { 300, "\U0000f02b"	}, // light intensity drizzle	 09d
  { 301, "\U0000f02b" }, // drizzle	 09d
  { 302, "\U0000f028"	}, // heavy intensity drizzle	 09d
  { 310, "\U0000f028" }, // light intensity drizzle rain	 09d
  { 311, "\U0000f028"	}, // drizzle rain	 09d
  { 312, "\U0000f028"	}, // heavy intensity drizzle rain	 09d
  { 313, "\U0000f029"	}, // shower rain and drizzle	 09d
  { 314, "\U0000f02b"	}, // heavy shower rain and drizzle	 09d
  { 321, "\U0000f01c"	}, // shower drizzle	 09d
    
  { 500, "\U0000f02b"	}, // light rain	 10d
  { 501, "\U0000f028"	}, // moderate rain	 10d
  { 502, "\U0000f028"	}, // heavy intensity rain	 10d
  { 503, "\U0000f028"	}, // very heavy rain	 10d
  { 504, "\U0000f028"	}, // extreme rain	 10d
  { 511, "\U0000f006"	}, // freezing rain	 13d
  { 520, "\U0000f029"	}, // light intensity shower rain	 09d
  { 521, "\U0000f029"	}, // shower rain	 09d
  { 522, "\U0000f029"	}, // heavy int ensity shower rain	 09d
  { 531, "\U0000f02c"	}, // ragged shower rain
    
  { 600, "\U0000f02a"	}, // light snow	[[file:13d.png]]
  { 601, "\U0000f02a"	}, // snow	[[file:13d.png]]
  { 602, "\U0000f02a"	}, // heavy snow	[[file:13d.png]]
  { 611, "\U0000f0b3"	}, // sleet	[[file:13d.png]]
  { 612, "\U0000f026"	}, // shower	[[file:13d.png]]
  { 615, "\U0000f026"	}, // light rain and snow	[[file:13d.png]]
  { 616, "\U0000f026"	}, // rain and snow	[[file:13d.png]]
  { 620, "\U0000f026"	}, // light shower snow	[[file:13d.png]]
  { 621, "\U0000f02a"	}, // shower snow	[[file:13d.png]]
  { 622, "\U0000f02a"	}, // heavy shower snow	[[file:13d.png]]
  
  { 701, "\U0000f04a" }, // mist	[[file:50d.png]]
  { 711, "\U0000f062"	}, // smoke	[[file:50d.png]]
  { 721, "\U0000f04a" }, // haze	[[file:50d.png]]
  { 731, "\U0000f063"	}, // sand, dust whirls	[[file:50d.png]]
  { 741, "\U0000f04a" }, // fog	[[file:50d.png]]
  { 751, "\U0000f063"	}, // sand	[[file:50d.png]]
  { 761, "\U0000f063"	}, // dust	[[file:50d.png]]
  { 762, "\U0000f063"	}, // volcanic ash	[[file:50d.png]]
  { 771, "\U0000f02f"	}, // squalls	[[file:50d.png]]
  { 781, "\U0000f056"	}, // tornado	[[file:50d.png]]
    
  { 800, "\U0000f02e"	}, // clear sky	[[file:01d.png]] [[file:01n.png]]
  
  { 801, "\U0000f02e" }, //	few clouds	[[file:02d.png]] [[file:02n.png]]
  { 802, "\U0000f031"	}, // scattered clouds	[[file:03d.png]] [[file:03d.png]]
  { 803, "\U0000f031"	}, // broken clouds	[[file:04d.png]] [[file:03d.png]]
  { 804, "\U0000f07e"	}, // overcast clouds	[[file:04d.png]] [[file:04d.png]]
    
  { 900, "\U0000f056" }, // tornado
  { 901, "\U0000f03a" }, // tropical storm
  { 902, "\U0000f073" }, // hurricane
  { 903, "\U0000f076"	}, // cold
  { 904, "\U0000f072"	}, // hot
  { 905, "\U0000f023"	}, // windy
  { 906, "\U0000f032" }, // hail
    
  { 951, "\U0000f02e"	}, // calm
  { 952, "\U0000f02e"	}, // light breeze
  { 953, "\U0000f02e"	}, // gentle breeze
  { 954, "\U0000f02e"	}, // moderate breeze
  { 955, "\U0000f02e"	}, // fresh breeze
  { 956, "\U0000f023"	}, // strong breeze
  { 957, "\U0000f023"	}, // high wind, near gale
  { 958, "\U0000f023"	}, // gale
  { 959, "\U0000f023"	}, // severe gale
  { 960, "\U0000f03a" }, // storm
  { 961, "\U0000f03a"	}, // violent storm
  { 962, "\U0000f073" }, // hurricane
    
  { 998, "\U0000f051" }, // sunrise
  { 999, "\U0000f052" }, // sunset
};
