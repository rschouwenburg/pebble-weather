//var console = {};
//console.log = function(){};

var MessageQueue = require('message-queue-pebble');
//var moment = require('moment');
var moment = require('moment-timezone');
                                                                                                                                                                     
var myAPIKey = 'aaa2844f92ab599ab4ea966a6da8a965';

function fetchCurrentWeather(city_id, latitude, longitude) {
  console.log('fetchCurrentWeather(): requesting weather for ' + city_id);
  var req = new XMLHttpRequest();
  if ( city_id == 0 ) {
    req.open('GET', 'http://api.openweathermap.org/data/2.5/weather?' +
      'lat=' + latitude + '&lon=' + longitude + '&cnt=1&appid=' + myAPIKey, true);
  } else {
    req.open('GET', 'http://api.openweathermap.org/data/2.5/weather?' +
      'id=' + city_id + '&cnt=1&appid=' + myAPIKey, true);
  }
  req.onload = function () {
    if (req.readyState === 4) {
      if (req.status === 200) {
        console.log(req.responseText);
        var response = JSON.parse(req.responseText);
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         
        if ( typeof response.id != 'undefined' ) {
          var temperature_F = Math.round(1.8 * (response.main.temp - 273.15) + 32);
          var code = response.weather[0].id;
          var city_code = response.id;
          var city = response.name;
          var description = response.weather[0].description.replace(' intensity','');
          var current_location = 0;
          if ( city_id == 0 ) {
            current_location = 1;
          }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
          
          var sunrise = response.sys.sunrise;
          var sunset = response.sys.sunset;
  
          var d = new Date();
          var timestamp = Math.round(d.getTime() / 1000);
          
          MessageQueue.sendAppMessage({
            'WEATHER_TYPE': 1,
            'WEATHER_CODE': code,
            'WEATHER_TEMPERATURE': temperature_F,
            'WEATHER_SUNRISE': sunrise,
            'WEATHER_SUNSET': sunset,
            'WEATHER_CITYCODE': city_code,
            'WEATHER_CITY': city,
            'WEATHER_TIMESTAMP': timestamp,
            'WEATHER_DESCRIPTION': description,
            'WEATHER_CURRENT_LOCATION': current_location
          });
        }                                                                                                                                                                                        
      } else {
        console.log('fetchCurrentWeather(): error ' + req.status);
        var wait = Math.round(Math.random() * 2000);
        if ( req.status == 429 ) {
          console.log('fetchCurrentWeather(): 429: waiting ' + wait + 'ms to retry');
          setTimeout(function(){
            fetchCurrentWeather(city_id, latitude, longitude);
          }, wait);
        }
      }
    }
  };
  req.send(null);
}

function fetchHourlyWeather(city_id, timezone, latitude, longitude) {
  console.log('fetchHourlyWeather(): requesting hourly forecast for ' + city_id + ' with timezone ' + timezone);
  var req = new XMLHttpRequest();
  if ( city_id == 0 ) {
    req.open('GET', 'http://api.openweathermap.org/data/2.5/forecast?' +
      'lat=' + latitude + '&lon=' + longitude + '&cnt=10&appid=' + myAPIKey, true);
  } else {
    req.open('GET', 'http://api.openweathermap.org/data/2.5/forecast?' +
       'id=' + city_id + '&cnt=10&appid=' + myAPIKey, true);
  }

  req.onload = function () {
    if (req.readyState === 4) {
      if (req.status === 200) {
        console.log(req.responseText);
        var response = JSON.parse(req.responseText);
        
        if ( typeof response.city != 'undefined' ) {
          if ( typeof response.city.id != 'undefined' ) {
            var message = {};
            message['WEATHER_TYPE'] = 2;
            message['WEATHER_CITY'] = response.city.id;
            message['WEATHER_CURRENT_LOCATION'] = 0;
            if ( city_id == 0 ) {
              message['WEATHER_CURRENT_LOCATION'] = 1;
            }
                    
            var d = new Date();
            var timestamp = Math.round(d.getTime() / 1000);
            message['WEATHER_TIMESTAMP'] = timestamp;
            
            var utcoffset = 0;
            if ( city_id > 0 ) {
              var utcoffset = 0 - moment.tz.zone(timezone).offset(timestamp * 1000) * 60;
              console.log("fetchHourlyWeather(): utcoffset = " + utcoffset);
            }
            message['UTC_OFFSET'] = utcoffset;
            
            var cnt = 100;
            response.list.forEach(function (item) {

                message[cnt.toString()] = item.dt;// + add_time;
                cnt++;
                message[cnt.toString()] = Math.round(1.8 * (item.main.temp - 273.15) + 32);
                cnt++;
                message[cnt.toString()] = item.weather[0].id;
                cnt++;

            });
            
            console.log(JSON.stringify(message));
            MessageQueue.sendAppMessage(message);
          }
        }
      } else {
        console.log('fetchHourlyWeather(): error ' + req.status);
        if ( req.status == 429 ) {
          var wait = Math.round(Math.random() * 2000);
          console.log('fetchHourlyWeather(): 429: waiting ' + wait + 'ms to retry');
          setTimeout(function(){
            fetchHourlyWeather(city_id, timezone, latitude, longitude);
          }, wait);
        }
      }
    }
  };
  req.send(null);
}

function fetchDailyWeather(city_id, timezone, latitude, longitude) {
  console.log('fetchDailyWeather(): requesting daily weather for ' + city_id);
  var req = new XMLHttpRequest();
  if ( city_id == 0 ) {
    req.open('GET', 'http://api.openweathermap.org/data/2.5/forecast/daily?' +
      'lat=' + latitude + '&lon=' + longitude + '&cnt=6&appid=' + myAPIKey, true);
  } else {
    req.open('GET', 'http://api.openweathermap.org/data/2.5/forecast/daily?' +
      'id=' + city_id + '&cnt=6&appid=' + myAPIKey, true);    
  }
  req.onload = function () {
    if (req.readyState === 4) {
      if (req.status === 200) {
        console.log(req.responseText);
        var response = JSON.parse(req.responseText);
        if ( typeof response.city != 'undefined' ) {
          if ( typeof response.city.id != 'undefined' ) {
            var message = {};
            message['WEATHER_TYPE'] = 3;
            message['WEATHER_CITY'] = response.city.id;
            message['WEATHER_CURRENT_LOCATION'] = 0;
            if ( city_id == 0 ) {
              message['WEATHER_CURRENT_LOCATION'] = 1;
            }
                    
            var d = new Date();
            var timestamp = Math.round(d.getTime() / 1000);
            message['WEATHER_TIMESTAMP'] = timestamp;
            
            var utcoffset = 0;
            if ( city_id > 0 ) {
              var utcoffset = 0 - moment.tz.zone(timezone).offset(timestamp * 1000) * 60;
              console.log("fetchDailyWeather(): utcoffset = " + utcoffset);
            }
            message['UTC_OFFSET'] = utcoffset;
            
            var cnt = 100;
            response.list.forEach(function (item) {
              message[cnt.toString()] = item.dt;
              cnt++;
              message[cnt.toString()] = Math.round(1.8 * (item.temp.min - 273.15) + 32);
              cnt++;
              message[cnt.toString()] = Math.round(1.8 * (item.temp.max - 273.15) + 32);
              cnt++;
              message[cnt.toString()] = item.weather[0].id;
              cnt++;            
            });
            
            console.log(JSON.stringify(message));
            MessageQueue.sendAppMessage(message);
          }
        }
      } else {
        console.log('fetchDailyWeather(): error ' + req.status);
        if ( req.status == 429 ) {
          var wait = Math.round(Math.random() * 2000);
          console.log('fetchDailyWeather(): 429: waiting ' + wait + 'ms to retry');
          setTimeout(function(){
            fetchDailyWeather(city_id, latitude, longitude);
          }, wait);
        }
      }
    }
  };
  req.send(null);
}

function fetchCities(name)
{
  console.log('fetchCities(): requesting cities like ' + name);
  var req = new XMLHttpRequest();
  req.open('GET', 'http://api.geonames.org/searchJSON?formatted=true&q=' + encodeURIComponent(name) + 
                  '&lang=en&featureClass=P&cities=cities15000&maxRows=10&lang=es&username=rdschouw&style=full&isNameRequired=true', true);  
  req.onload = function () {
    if (req.readyState === 4) {
      if (req.status === 200) {
        //console.log(req.responseText);
        var response = JSON.parse(req.responseText);
        
        var message = {};
        message['WEATHER_TYPE'] = 4;
        
        var cnt = 100;
        response.geonames.forEach(function (item) {
          message[cnt.toString()] = item.geonameId;
          cnt++;
          message[cnt.toString()] = item.name;
          cnt++;
          message[cnt.toString()] = item.name + ", " + item.adminName1 + ", " + item.countryName;
          cnt++;
          message[cnt.toString()] = item.timezone.timeZoneId;
          cnt++;
        });
        
        console.log(JSON.stringify(message));
        MessageQueue.sendAppMessage(message);
        
      } else {
        console.log('fetchCities(): error ' + req.status);
      }
    }
  };
  req.send(null);
}

function getIPlocation(successFunction) {
  console.log('getIPlocation(): requesting location');
  var req = new XMLHttpRequest();
  req.open('GET', 'http://freegeoip.net/json/');
  req.onload = function () {
    if (req.readyState === 4) {
      if (req.status === 200) {
        console.log(req.responseText);
        var response = JSON.parse(req.responseText);
        var coords = [];
        coords["latitude"] = response.latitude;
        coords["longitude"] = response.longitude;
        var pos = [];
        pos["coords"] = coords;
        
        successFunction(pos);
      }
    } else {
      console.log('getIPlocation(): error ' + req.status);
      
      MessageQueue.sendAppMessage({
        'WEATHER_ICON': 0,
        'WEATHER_TEMPERATURE': 0,
        'WEATHER_SUNRISE': 0,
        'WEATHER_SUNSET': 0,
        'WEATHER_TIMESTAMP': 0
      });
    }
  };
  req.send(null);
}

function locationSuccessFetchCurrent(pos) {
  console.log("locationSuccessFetchCurrent(): accuracy=" + pos.coords.accuracy);
  
  var coordinates = pos.coords;
  //coordinates.latitude = (Math.random() * 180) - 90;
  fetchCurrentWeather(0, coordinates.latitude, coordinates.longitude);
}

function locationErrorFetchCurrent(err) {
  console.log('locationErrorFetchCurrent(): GPS failed, fallback to IP');
  
  getIPlocation(locationSuccessFetchCurrent);
}

function locationSuccessFetchHourly(pos) {
  var coordinates = pos.coords;
  fetchHourlyWeather(0, "", coordinates.latitude, coordinates.longitude);
}

function locationErrorFetchHourly(err) {
  console.log('locationErrorFetchHourly(): GPS failed, fallback to IP');
  
  getIPlocation(locationSuccessFetchHourly);
}

function locationSuccessFetchDaily(pos) {
  var coordinates = pos.coords;
  fetchDailyWeather(0, "", coordinates.latitude, coordinates.longitude);
}

function locationErrorFetchDaily(err) {
  console.log('locationErrorFetchDaily(): GPS failed, fallback to IP');
  
  getIPlocation(locationSuccessFetchDaily);
}

var locationOptions = {
  'timeout': 3000,
  'maximumAge': 180000,
  'enableHighAccuracy': true
};

Pebble.addEventListener('appmessage', function (e) {
  
  console.log('Received message: ' + JSON.stringify(e.payload));
  
  switch(e.payload['WEATHER_TYPE']) {
      
    case 1:
      for(var i=100; i<111; i++) {
        if ( e.payload[i] != null ) {
          console.log('appmessage event: request for current weather for city ' + e.payload[i]);
          if ( e.payload[i] == 0 ) {
            window.navigator.geolocation.getCurrentPosition(locationSuccessFetchCurrent, locationErrorFetchCurrent,
                                                  locationOptions); 
          } else {
            fetchCurrentWeather(e.payload[i], null, null);
          }
        }
      }
      
      console.log('appmessage event: fetch current weather!');
      break;
      
    case 2:
      for(var i=100; i<111; i++) {
        if ( e.payload[i] != null ) {
          var city = e.payload[i].split(",");
          console.log('appmessage event: request for hourly forecast for city ' + city[0]);
          if ( city[0] == 0 ) {
            window.navigator.geolocation.getCurrentPosition(locationSuccessFetchHourly, locationErrorFetchHourly,
                                                  locationOptions); 
          } else {
            fetchHourlyWeather(city[0], city[1], null, null);
          }
        }
      }
      console.log('appmessage event: fetch hourly weather!');
      break;
      
    case 3:
      for(var i=100; i<111; i++) {
        if ( e.payload[i] != null ) {
          var city = e.payload[i].split(",");
          console.log('appmessage event: request for daily forecast for city ' + city[0]);
          if ( city[0] == 0 ) {
            window.navigator.geolocation.getCurrentPosition(locationSuccessFetchDaily, locationErrorFetchDaily,
                                                  locationOptions); 
          } else {
            fetchDailyWeather(city[0], city[1], null, null);
          }
        }
      }
      console.log('appmessage event: fetch daily weather!');
      break;

    case 4:
      if ( e.payload[100] != null) {
        fetchCities(e.payload[100]);
        console.log('appmessage evetns: fetch cites');
      } else {
        console.log('appmessage event: fetch cities - missing city');
      }
      break;
      
    default:
      console.log('appmessage event: unknown request')
  }
});

function reportReady() {
  MessageQueue.sendAppMessage({
     'READY': 1}, function() {}, reportReady);
}

Pebble.addEventListener('ready', function (e) {
  console.log('JS ready');
  
  reportReady();
});
