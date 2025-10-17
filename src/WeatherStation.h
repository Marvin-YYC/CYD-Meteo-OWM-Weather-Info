#ifndef _WEATHER_STATION_H

#define _WEATHER_STATION_H

#include <Arduino.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESP32Time.h>
#include "ArialRounded09.h"
#include "ArialRounded14.h"
#include "ArialRounded36.h"
#include "ArialRounded20.h"
#include "ArialRounded26.h"
#include "ArialRounded44.h"
#include "ArialRounded54.h"
#include "WeatherFont60.h"
#include "WeatherFont80.h"
#include "WeatherFont96.h"

#define VW_Version "v. 1.00"

// Objects to manipulate display
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// Network and Web app related
WiFiManager wifiManager;
String staHostname = "CYD Weather Station";      // Access Point hostname.
//use your smartphone to connect to weather station access point and set SSID and password.
//IMO Not the best set up of a wifi manager, but may change it later
// >>>>> [CHANGE THIS] - Custom Static address for the Weather Station
char static_ip[16] = "10.0.0.10"; // "10.0.0.10";
char static_gw[16] = "10.0.0.1"; //
char static_mask[16] = "255.255.255.0"; //
char static_dns[16] = "10.0.0.1";  //

// >>>> [CHANGE THIS] - TimeZone/Town/Latitude/longitude
//Meteo Weather info >>>>>>>>>>>>>>>PUT YOUR INFO BELOW <<<<<<<<<<<<<<
const String timeZone = "America/Edmonton";  //  use time zones as set by Meteo
const String townLat = "51.1293";  // USE METEO COORDINATES FOR YOUR CITY
const String townLon = "-114.0129";

//Open Weather Map info >>>>>>>>>>>>>>>PUT YOUR INFO BELOW <<<<<<<<<<<<<<
const String owmTown = "Calgary";    // EDIT <<<<  your city/town/location name "CALGARY" "Dawson+Creek" (caps not important)
const String owmCountry = "CA";     // EDIT <<<< your country code "CA"
const String owmKey = "YOUR_OWM_API_KEY_HERE"; // EDIT <<<<<< Your OWM api key here

//World Air Quality Index project station ID and API key https://aqicn.org/  >>>>>>>>>>>>>>>PUT YOUR INFO BELOW <<<<<<<<<<<<<<
const String waqiKey = "YOUR_AQICN_API_KEY_HERE"; // you own API key goes here
const String waqiStn1 = "@12498";  // set AQI station 1
const String waqiStn2 = "@5477";  // set AQI station 2
//const String waqiStn3 = "stationid";  // set AQI station 3

WebServer server(80);

// API Web server for accurate time getTime.
String timeServer = "https://timeapi.io/api/time/current/zone?timeZone=" + timeZone;

 //AQI using multiple stations from WAQI server.  You will need to obtain your own API key from them. 
String aqicnURLs[] = {
  "https://api.waqi.info/feed/" + waqiStn1 + "/?token=" + waqiKey,   // Station 1 Varsity
  "https://api.waqi.info/feed/" + waqiStn2 + "/?token=" + waqiKey,    // Station 2 Downtown
  //"https://api.waqi.info/feed/" + waqiStn3, + "/?token=" + waqiKey,   // Station 3

};

const char* auroraURL = "https://www.aurorawatch.ca/AWVal.txt";
  
String owmServerURL = "https://api.openweathermap.org/data/2.5/weather?q=" + owmTown + "," + owmCountry + "&units=metric&APPID=" + owmKey;

String meteoWeatherServer = "https://api.open-meteo.com/v1/forecast?latitude=" + townLat + "&longitude=" + townLon + "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,uv_index_max,daylight_duration,sunrise,sunset,&current=weather_code,temperature_2m,relative_humidity_2m,wind_speed_10m,wind_gusts_10m,apparent_temperature,cloud_cover,visibility,wind_direction_10m,dew_point_2m,is_day,cape,lightning_potential,&timezone=" + timeZone + "&forecast_days=1";

const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char *months[] = {"Jan","Feb","Mar","Apr","May","June","Jul","Aug","Sept","Oct","Nov","Dec"};
ESP32Time rtc(0); 

struct DateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  long microsecond;
};
#define WS_BLACK TFT_BLACK
#define WS_WHITE TFT_WHITE
#define WS_YELLOW TFT_YELLOW
#define WS_BLUE 0x7E3C

// forward declarations
bool drawTime(void *);
void drawMeteoForecast(int meteo, float currTemp,  short currHumi, float minTemp, float maxTemp, float currWind, float feelTemp, float gustWind, short cldTotal, float visib, float uvIndex, float wndDir, float dewPo, int sunriseHour, int sunriseMin,int sunsetHour,int sunsetMin,int daylightHours, int daylightMinutes, short rainProba, int isDay, int cape, int litPot);
String getDescFromMeteo(int meteo);
bool getTime(void *);
DateTime parseISO8601(const String& iso8601);
void timeString (void *);  // >>
bool getMeteoForecast(void *);
void setupApi();
void handlePost();

#endif 

