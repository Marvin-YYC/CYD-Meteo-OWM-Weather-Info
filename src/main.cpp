#include <Arduino.h>
#include "WeatherStation.h"
#include <arduino-timer.h>

#define TFT_BL 21
#define BACKLIGHT_PIN 21

//--- Time object
auto timer = timer_create_default();

void setup() {
  Serial.begin(115200);
  while (!Serial) {}; 
  Serial.print("WeatherStation");
  Serial.println(VW_Version);
  
    delay(1000);

  //--- WiFi connection using Captive_AP.
  wifiManager.setConfigPortalTimeout(5000);
  IPAddress _ip,_gw,_mask,_dns;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _mask.fromString(static_mask);
  _dns.fromString(static_dns);

  wifiManager.setHostname(staHostname);
  wifiManager.setSTAStaticIPConfig(_ip,_gw,_mask,_dns);

  if(!wifiManager.autoConnect("Access_Weather","12345678")) { //this is the ID and Password for the Wifi Manager (PW must be 8 characters)
    Serial.println("Failed to connect and hit timeout");

    delay(3000);
    ESP.restart();
  }

  Serial.println("INFO: connected to WiFi");
  setupApi();
  getTime(NULL);

  //--- Initialize display
  tft.init();
  tft.setRotation(1); // (1) = landscape - buttons top (2) = portrait - buttons left (3) Landscape - buttons bottom (4) portrait - reverse image - buttons left 
  //(5) landscape - reverse image - buttons bottom (6) portrait - reverse image - buttons right (7) landscape - reverse image - buttons top. (8) portrait - buttons right

  // Set up the backlight pin as an output
  pinMode(BACKLIGHT_PIN, OUTPUT);  //define backlight as output
  ledcWrite(BACKLIGHT_PIN, 50);   // set backlight to max
  // Configure PWM
  //ledcSetup(BACKLIGHT_PIN, 5000, 8);

  tft.fillScreen(WS_BLACK);
  drawTime(NULL);
  getForecast(NULL);
  
  //--- Create Timers for main Weather Station functions
  timer.every(500,drawTime);               // Every 500ms, display time
  timer.every(15*60*1000,getTime);         // Every 15mn
  timer.every(20*60*1000,getForecast);     // Every 20min
}

void loop() {
  server.handleClient();
  timer.tick();
}

//--- getInternet Time From API server and set RTC time.
DateTime parseISO8601(const String& iso8601) {
  DateTime dt;
  sscanf(iso8601.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d.%7ld", // <<<<< number of letters??!?  "%4d-%2d-%2dT%2d:%2d:%2d.%7ld",
         &dt.year, &dt.month, &dt.day,
         &dt.hour, &dt.minute, &dt.second, &dt.microsecond);
  return dt;
}

bool getTime(void *) {
  HTTPClient http;
  int httpResponseCode;
  JsonDocument jsonDoc;
  String payload;
  DeserializationError error;
  const char * datetime;
  DateTime dt;

  http.begin(timeServer);
  httpResponseCode = http.GET();
  if (httpResponseCode > 0){
    payload = http.getString();
    Serial.println(httpResponseCode);      // for debug purpose
    Serial.println(payload);
  } else {
    Serial.print("ERROR: bad HTTP1 request: ");
    Serial.println(httpResponseCode);

    delay(1000);

  }

  error = deserializeJson(jsonDoc,payload);
  if (!error) {
    datetime = jsonDoc["dateTime"];
    dt = parseISO8601(String(datetime));
    rtc.setTime(dt.second, dt.minute, dt.hour, dt.day, dt.month, dt.year); //rtc.setTime(dt.second, dt.minute, dt.hour, dt.day, dt.month, dt.year);rtc.setTime(dt.second, dt.minute, dt.hour, dt.day, dt.month, dt.year);
  }
  return true;
} 

bool getForecast(void *) {
  HTTPClient http;
  int httpResponseCode;
  JsonDocument jsonDoc;
  String payload;
  DeserializationError error;
  String timeString; //>>>>
  float currTemp;
  float minTemp, maxTemp;
  short currHumi;
  float currWind, gustWind, feelTemp, visib;
  short cldTotal, cldLow, cldMid, cldHi;
  float dewPo;
  float uvIndex;
  float wndDir;
  float dayLite;
  String  timZon;
  short sunRise;
  short sunSet;
  int rainProba;
  int wmoCode;

  // get forecast
  http.begin(weatherServer);
  httpResponseCode = http.GET();
  if (httpResponseCode > 0){
    payload = http.getString();
    Serial.println(httpResponseCode);      // for debug purpose
    Serial.println(payload);
  } else{
    Serial.print("ERROR: bad HTTP1 request: ");
    Serial.println(httpResponseCode);
  }

  error = deserializeJson(jsonDoc,payload);
  if (!error) {

    wmoCode = jsonDoc["current"]["weather_code"];
    currTemp = jsonDoc["current"]["temperature_2m"]; //~
    currHumi = jsonDoc["current"] ["relative_humidity_2m"];//~
    cldTotal = jsonDoc["current"]["cloud_cover"];
    visib = jsonDoc["current"]["visibility"];
    wndDir = jsonDoc["current"]["wind_direction_10m"];
    dewPo = jsonDoc["current"]["dew_point_2m"];
    currWind = jsonDoc["current"]["wind_speed_10m"];
    gustWind = jsonDoc["current"]["wind_gusts_10m"];
    feelTemp = jsonDoc["current"]["apparent_temperature"];
    cldLow = jsonDoc["current"]["cloud_cover_low"];
    cldMid = jsonDoc["current"]["cloud_cover_mid"];
    cldHi = jsonDoc["current"]["cloud_cover_high"];
    sunRise = jsonDoc["daily"]["sunrise"][0];
    sunSet = jsonDoc["daily"]["sunset"][0];
    minTemp = jsonDoc["daily"]["temperature_2m_min"][0];
    maxTemp = jsonDoc["daily"]["temperature_2m_max"][0];
    uvIndex = jsonDoc["daily"]["uv_index_max"][0];
    dayLite = jsonDoc["daily"]["daylight_duration"][0];
    rainProba = jsonDoc["daily"]["precipitation_probability_max"][0];
    String timZon = jsonDoc["timezone_abbreviation"];

    // display forecast 
    drawForecast(wmoCode,currTemp,currHumi,minTemp,maxTemp,currWind,feelTemp,gustWind,cldTotal,visib,uvIndex,wndDir,dayLite,dewPo,sunRise,sunSet,cldLow,cldMid,cldHi,timZon,rainProba); //order matters

  }

  return true;
}

//--- setup and launch ReST API web server
void setupApi() {
  Serial.println("INFO: Web server started");
  server.on("/post-data", HTTP_POST, handlePost);

  // start Web server
  server.begin();
}

//--- handle POST request : store new values of sensor data to fromSensor structure.
//    Error handling is minimal : nothing is done if temp/humi/bat.level are non sense values!!
void handlePost() {
  JsonDocument jsonDoc;
  char buffer[128];
  String payload;

  if (server.hasArg("plain") == false) {
    jsonDoc.clear();
    jsonDoc["error"] = "true";
    jsonDoc["reason"] = "no body found.";
    serializeJson(jsonDoc,buffer);
    
    server.send(200,"application/json",buffer);
  } else {
    payload = server.arg("plain");
    deserializeJson(jsonDoc,payload);

    server.send(200, "application/json", "{}");

  } 
}

//--- return PROGMEM iconname from wmo code
const uint16_t* getIconFromWMO(int wmo) {
  if (wmo == 0) return sunny;
  if (wmo == 1) return mainlysunny;
  if (wmo == 2) return partlycloudy;
  if (wmo == 3) return cloudy;
  if (wmo == 45 || wmo == 48) return fog;
  if (wmo == 51 || wmo == 61 || wmo == 80 || wmo == 83) return lightrain;
  if (wmo == 53 || wmo == 55 || wmo == 63 || wmo == 65) return rain;
  if (wmo == 56 || wmo == 57 || wmo == 66 || wmo == 67) return freezingrain;
  if (wmo == 71) return lightsnow;
  if (wmo == 73 || wmo == 75 || wmo == 77) return snow;
  if (wmo == 85 || wmo == 86) return snow;
  if (wmo >= 95 && wmo <= 99) return storms;
  return unknown;
}

// return Short Weather Description from WMO code
String getDescriptionFromWMO(int wmo) {
  if (wmo == 0) return "Clear";
  if (wmo == 1) return "Mostly Clear";
  if (wmo == 2) return "Partly Cloudy";
  if (wmo == 3) return "Overcast";
  if (wmo == 45 || wmo == 48) return "Fog/Mist";
  if (wmo == 51 || wmo == 61 || wmo == 80 || wmo == 83) return "Light Rain";
  if (wmo == 53 || wmo == 55 || wmo == 63 || wmo == 65) return "Rain";
  if (wmo == 56 || wmo == 57 || wmo == 66 || wmo == 67) return "Freezing Rain";
  if (wmo == 71) return "Light Snow";
  if (wmo == 73 || wmo == 75 || wmo == 77) return "Moderate Snow";
  if (wmo == 85 || wmo == 86) return "Heavy Snow";
  if (wmo >= 95 && wmo <= 99) return "Storm";
  return "";
}

//--- Drawing functions
bool drawTime(void *) {
  struct tm now;
  int dw;
  char tempo[20];

  sprite.createSprite(320,70); // 148,50 //sprite.createSprite(320,50);
  sprite.fillSprite(WS_BLACK);
  sprite.setTextColor(TFT_SKYBLUE);
  now = rtc.getTimeStruct();
  dw = rtc.getDayofWeek();

  sprite.loadFont(arialround20);
  sprite.setTextDatum(CL_DATUM);
  sprintf(tempo,"%s %02d %s %4d",days[dw],now.tm_mday,months[now.tm_mon],(now.tm_year+1900)); // sprintf(tempo,"%s %02d %s %4d",days[dw],now.tm_mday,months[now.tm_mon],(now.tm_year+1900));
  sprite.drawString(tempo,10,10);

  //sprite.setTextColor(TFT_DARKGREY);
  //sprite.setTextDatum(MC_DATUM);
  //sprintf(tempo,"UTC-7"); //~~~
  //sprite.drawString(tempo,30,40);

  sprite.setTextColor(WS_WHITE);
  sprintf(tempo,"%02d:%02d",now.tm_hour,now.tm_min); // sprintf(tempo,"%02d:%02d:%02d",now.tm_hour,now.tm_min,now.tm_sec);
  sprite.loadFont(arialround54);
  sprite.drawString(tempo,10,50); //~  5,40  moved this to the left // sprite.drawString(tempo,160,40);
  sprintf(tempo,":%02d",now.tm_sec); //~ add seconds
  sprite.loadFont(arialround20); //~ make seconds smaller font
  sprite.drawString(tempo,153,35); //~ 102,30 moves seconds to the right of centre
/*
  sprite.setTextColor(TFT_BROWN);
  sprintf(tempo,":test one"); //~ 
  sprite.loadFont(arialround14); //~ 
  sprite.drawString(tempo,150,30); //~
  sprite.setTextColor(TFT_BROWN);
  sprintf(tempo,":test two"); //~ 
  sprite.loadFont(arialround14); //~ 
  sprite.drawString(tempo,170,46); //~
  sprite.setTextColor(TFT_BROWN);
  sprintf(tempo,":test three"); //~ 
  sprite.loadFont(arialround14); //~ 
  sprite.drawString(tempo,220,20); //~
*/

      //for future am/pm when i figure it out
      sprite.setTextColor(TFT_DARKGREY);
      sprite.loadFont(arialround14);
      sprintf(tempo,"24hr");
      sprite.drawString(tempo,154,56); // 102,46

      //sprite.setTextColor(TFT_DARKGREY);
      //sprintf(tempo,"TZ: %4.6f ", timeZone);
      //sprite.drawString(tempo,110,55);

  sprite.pushSprite(0,0);
  sprite.deleteSprite();

  return true;
}

void drawForecast(int wmo, float currTemp, short currHumi, float minTemp, float maxTemp, float currWind, float feelTemp, float gustWind, short cldTotal, float visib, float uvIndex, float wndDir, float dayLite, float dewPo, short sunRise, short sunSet, short cldLow, short cldMid, short cldHi, String timZon, short rainProba) {
  char tempo[10];

  sprite.createSprite(160,160);   // icon sprite    sprite.createSprite(150,150);
  sprite.setSwapBytes(true);
  sprite.fillSprite(WS_BLACK);

  // Cf. example : Sprite_image_4bit de la lib TFT
  sprite.pushImage(15,15,128,128,getIconFromWMO(wmo));  // sprite.pushImage(15,15,128,128,getIconFromWMO(wmo));
  sprite.pushSprite(0,45);//  sprite.pushSprite(0,45);
  sprite.deleteSprite();
  sprite.createSprite(320,55);    // text sprite (bottom display for weather condition, min/max temp and rain %)  (320,55);
  sprite.fillSprite(WS_BLACK);   //sprite.fillSprite(WS_BLACK);

  sprite.setTextColor(TFT_DARKGREEN); 
  sprite.loadFont(arialround20); //(arialround36);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(getDescriptionFromWMO(wmo),5,10); //   15,6    115,6   315,18

///>>>TEST AREA
      //sprite.setTextColor(TFT_DARKGREY);
      //sprintf(tempo,"TZ: %2d", timZon);
      //sprite.drawString(tempo,5,25);//115,20

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);
  sprintf(tempo,"Day: %4.3f ", dayLite /3600); //sprintf(tempo,"Day: %4.3f ", dayLite /3600);
  sprite.drawString(tempo,250,45);

  sprite.setTextColor(TFT_DARKGREY);
  sprintf(tempo,"Sunrise: %4.2f",sunRise);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,5,45);

  sprite.setTextColor(TFT_DARKGREY);
  sprintf(tempo,"Sunset: %4.2f",sunSet);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString(tempo,160,45);
  
  sprite.pushSprite(0,185);
  sprite.deleteSprite();

  //Display town name 
  sprite.createSprite(270,170); //  create space for sprite ? ? made it bigger sprite.createSprite(170,136);
  sprite.fillSprite(WS_BLACK);
  sprite.setTextColor(TFT_BLUE);
  sprite.loadFont(arialround14);
  sprite.setTextDatum(CL_DATUM); //sprite.setTextDatum(TR_DATUM);
  sprite.drawString(townName,5,20); //170,0    168,10

  // display Temp & Humi
  sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround36);
  sprintf(tempo,"%4.1f/",currTemp);
  sprite.setTextDatum(TR_DATUM);
  sprite.drawString(tempo,100,25); // 150,45    was 165,45 moved right make room for °C

  sprite.setTextColor(TFT_ORANGE);
  sprite.loadFont(arialround36);
  sprintf(tempo,"%3.0f",feelTemp);
  sprite.setTextDatum(TR_DATUM);
  sprite.drawString(tempo,154,25); //150,45

  sprite.setTextColor(TFT_ORANGE);
  sprite.loadFont(arialround14);
  sprintf(tempo,"°C"); // separated this from lines above to make °C smaller font
  sprite.setTextDatum(TR_DATUM);
  sprite.drawString(tempo,170,25); //  168,35 full right - middle of digit 165,45

  sprite.setTextColor(TFT_ORANGE);
  sprite.loadFont(arialround14);
  sprintf(tempo,"WC: %4.1f°", feelTemp);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,170,70);

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"Humi: %2d %%", currHumi);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,170,85); //sprite.drawString(tempo,60,110);

  sprite.setTextColor(TFT_MAGENTA);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"Wnd:%4.0fkph", currWind);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,170,100);  

  sprite.setTextColor(TFT_MAGENTA);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"Gst:%4.0fkph", gustWind);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,170,115);  

  //sprite.setTextColor(TFT_LIGHTGREY);
  //sprite.loadFont(arialround14);  
  //sprintf(tempo,"CldT: %2d %%", cldTotal);
  //sprite.setTextDatum(CL_DATUM);
  //sprite.drawString(tempo,0,85);  

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"PoP: %2d %%",rainProba);
  //sprite.setTextDatum(CR_DATUM);
  //sprite.drawString(tempo,125,20);  
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,0,85); 

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"Cld<3: %2d %%", cldLow);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,0,100);  
  
  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"Cld3+: %2d %%", cldMid);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,0,115);  

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"Cld8+: %2d %%", cldHi);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,0,130);  

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"Vis: %4.1fkm", visib /1000);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,0,70);  

   sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"UVdx: %4.0f", uvIndex);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,0,145);  

  //sprite.setTextColor(TFT_LIGHTGREY);
  //sprite.loadFont(arialround14);  
  //sprintf(tempo,"PoP: %2d %%",rainProba);
  //sprite.setTextDatum(CR_DATUM);
  //sprite.drawString(tempo,0,115);  

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"Dew: %4.1f", dewPo);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,170,145);  
/*
  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"Day: %4.3f ", dayLite/3600);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,0,130);  
  */
  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);
  sprintf(tempo,"Dir: %4.1f", wndDir);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,170,130);

  sprite.setTextColor(TFT_SKYBLUE);
  sprintf(tempo,"Lo: %4.1f°c",minTemp);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,0,160);

  sprite.setTextColor(TFT_GREENYELLOW);
  sprintf(tempo,"Hi: %4.1f°c",maxTemp);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,170,160);

  sprite.pushSprite(150,50);//150,50
  sprite.deleteSprite();

  //>>
}
/*
void displayTime(unsigned float dayLite) {
  // Calculate hours, minutes, and seconds
  int hours = (dayLite % 86400L) / 3600; // Seconds in a day
  int minutes = (dayLite % 3600) / 60;
  int seconds = dayLite % 60;

  // Format the time string
  String timeString = String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
}
*/