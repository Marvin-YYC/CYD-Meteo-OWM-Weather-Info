#ifndef _WEATHER_STATION_H

#define _WEATHER_STATION_H

#include <Arduino.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESP32Time.h>
#include "ArialRounded14.h"
#include "ArialRounded36.h"
#include "ArialRounded20.h"
#include "ArialRounded26.h"
#include "ArialRounded44.h"
#include "ArialRounded54.h"
#include "WeatherFont60.h"
#include "weatherIcons.h"

#define VW_Version "v. 1.00"

// Objects to manipulate display
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// Network and Web app related
WiFiManager wifiManager;
String staHostname = "CYD Weather Station";      // Access Point hostname.
// >>>>> [CHANGE THIS] - Custom Static address for the Weather Station
char static_ip[16] = "10.0.0.10"; // CHANGE THIS INFORMATION 
char static_gw[16] = "10.0.0.1"; // CHANGE THIS INFORMATION 
char static_mask[16] = "255.255.255.0"; //CHANGE THIS INFORMATION 
char static_dns[16] = "10.0.0.1";  // CHANGE THIS INFORMATION 

// >>>> [CHANGE THIS] - TimeZone/Town/Latitude/longitude
const String timeZone = "America/Edmonton";
const String townName = "Calgary AB";
const String townLat = "51.0501";
const String townLon = "-114.0853";

WebServer server(80);

// API Web server for accurate time getTime.
String timeServer = "https://timeapi.io/api/time/current/zone?timeZone=" + timeZone;

String weatherServer = "https://api.open-meteo.com/v1/forecast?latitude=" + townLat + "&longitude=" + townLon + "&current=weather_code&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,uv_index_max,daylight_duration,sunrise,sunset,&current=temperature_2m,relative_humidity_2m,wind_speed_10m,wind_gusts_10m,apparent_temperature,weather_code,cloud_cover,visibility,wind_direction_10m,dew_point_2m,cloud_cover_low,cloud_cover_mid,cloud_cover_high&timezone=America%2FDenver&forecast_days=1";

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
void drawForecast(int wmo, float currTemp,  short currHumi, float minTemp, float maxTemp, float currWind, float feelTemp, float gustWind, short cldTotal, float visib, float uvIndex, float wndDir, float dewPo, int sunriseHour, int sunriseMin,int sunsetHour,int sunsetMin,int daylightHours, int daylightMinutes, short cldLow, short cldMid, short cldHi, short rainProba);
const uint16_t* getIconFromWMO(int wmo);
String getDescriptionFromWMO(int wmo);
bool getTime(void *);
DateTime parseISO8601(const String& iso8601);
void timeString (void *); 
bool getForecast(void *);
void setupApi();
void handlePost();

#endif 
