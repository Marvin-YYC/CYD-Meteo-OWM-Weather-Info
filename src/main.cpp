#include <Arduino.h>
#include "WeatherStation.h"
#include <arduino-timer.h>

bool getAuroraData(void*);  
void drawAurora(float auroraValue); 
bool getaqiData(void*);
void drawAQI(int aqiValue);

#define TFT_BL 21
#define BACKLIGHT_PIN 21

const char* auroraURL = "https://www.aurorawatch.ca/AWVal.txt";
const char* aqicnURL = "https://api.waqi.info/feed/"yourlocationinformationorstationID"/?token=YOUR_API_KEY";  // PROVIDE YOUR API TOKEN AND LOCATIONINFORMATION HERE

//--- Time object
auto timer = timer_create_default();

void setup() {
  Serial.begin(115200);
  Serial.begin(115200);
  bool getAuroraData(void*); 
  bool getgetaqiData(void*);
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
  getTime(NULL);

  //--- Initialize display
  tft.init();
  tft.setRotation(1); // (1) = landscape - buttons top (2) = portrait - buttons left (3) Landscape - buttons bottom (4) portrait - reverse image - buttons left 
  //(5) landscape - reverse image - buttons bottom (6) portrait - reverse image - buttons right (7) landscape - reverse image - buttons top. (8) portrait - buttons right
//////~~~~~~~~
  // Set up the backlight pin as an output
  pinMode(BACKLIGHT_PIN, OUTPUT);  //define backlight as output
  ledcWrite(BACKLIGHT_PIN, 50); 
  ledcSetup(0, 5000, 8); // Channel 0, 5kHz, 8-bit resolution
  ledcAttachPin(BACKLIGHT_PIN, 0);
  ledcWrite(0, 130); // Adjust brightness (0-255) 128 adjust brightness level here<<< 80
//////~~~~~~~~

  tft.fillScreen(WS_BLACK);
  drawTime(NULL);
  getForecast(NULL);
  bool getAuroraData(void*);
  getAuroraData(nullptr); 
  bool getaqiData(void*);
  getaqiData(nullptr);


  //--- Create Timers for main Weather Station functions
  timer.every(500,drawTime);               // Every 500ms, display time
  timer.every(30*60*1000,getTime);         // Every 30mn
  timer.every(20*60*1000,getForecast);     // Every 20min
  timer.every(20*60*1000,getAuroraData);   // Fetch aurora data every 20 minutes
  timer.every(20*60*1000,getaqiData);      // Fetch AQI data every 20 minutes
  //recommend that Forecast/Aurora/AQI updates occur at he same time or the sprites will be blank waiting for data.
}

void loop() {
  server.handleClient();
  timer.tick();
  struct tm now = rtc.getTimeStruct();
/*
// Enter deep sleep - short sleep test
if (now.tm_hour == 20 && now.tm_min == 15) { //8:15pm  for short test
  Serial.println("Entering Deep Sleep for 5 minutes...");
  esp_sleep_enable_timer_wakeup(3 * 60 * 1000000); // 5 minutes
  esp_deep_sleep_start();
}
*/
// Enter deep sleep
if (now.tm_hour >= 23) {    //   Begin deep sleeps at 23:00hrs wakes up when timer expires
  Serial.println("Entering Deep Sleep...");
  esp_sleep_enable_timer_wakeup((uint64_t)8 * 60 * 60 * 1000000); // 8 hours - wakes up after timer ends.  
  //Timer is not accurate when in deep sleep. It may wake up a few minutes early.
  esp_deep_sleep_start();
  }
}

//--- getInternet Time From API server and set RTC time.
DateTime parseISO8601(const String& iso8601) {
  DateTime dt;
  sscanf(iso8601.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d.%7ld", 
         &dt.year, &dt.month, &dt.day,
         &dt.hour, &dt.minute, &dt.second, &dt.microsecond);

    return dt;
  }

bool getAuroraData(void *) {
  Serial.println("Fetching Aurora Data...");

  if (WiFi.status() != WL_CONNECTED) {  
      Serial.println("WiFi not connected, trying to reconnect...");
      WiFi.disconnect();
      WiFi.reconnect();
      //delay(500);
      if (WiFi.status() != WL_CONNECTED) {
          Serial.println("Failed to reconnect.");
          return true;  // Keep the timer running
      }
  }

  HTTPClient http;
  http.begin(auroraURL);

  int httpResponseCode = http.GET();
  Serial.print("HTTP Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("Aurora Data: " + payload);
      float auroraValue = payload.toFloat();
      Serial.print("Parsed Aurora Value: ");
      Serial.println(auroraValue);

      Serial.println("Calling drawAurora...");
      drawAurora(auroraValue);  // Call function to display data
  } else {
      Serial.print("HTTP Request Failed! Code: ");
      Serial.println(httpResponseCode);
  }

  http.end();
  yield();  // Allow background tasks to run
  return true;  // Keep the timer running
}

void drawAurora(float auroraValue) {
  char tempo[20];
  Serial.println("Drawing Aurora with Sprites...");
  sprite.createSprite(120, 25);// width,height
  sprite.fillSprite(WS_BLACK); 

        if (auroraValue > 75) { 
          sprite.setTextColor(TFT_RED);
        }
        if (auroraValue > 65) { 
        sprite.setTextColor(TFT_YELLOW);
        }
        else if (auroraValue > 50) { 
          sprite.setTextColor(TFT_GREENYELLOW);
        }
        else {
        sprite.setTextColor(TFT_LIGHTGREY);
        }
  sprite.loadFont(arialround20); 
  sprite.setCursor(0, 0);  
  sprintf(tempo, "PoAB: %2d%%", (int)auroraValue);  
  sprite.pushSprite(0, 220); 
  //delay(500); 
  sprite.deleteSprite();  
}

bool getaqiData(void *) {
  Serial.println("Fetching AQI Data...");

  if (WiFi.status() != WL_CONNECTED) {  
      Serial.println("WiFi not connected, trying to reconnect...");
      WiFi.disconnect();
      WiFi.reconnect();
      if (WiFi.status() != WL_CONNECTED) {
          Serial.println("Failed to reconnect.");
          return true;  // Keep the timer running
      }
  }
  HTTPClient http;
  http.begin(aqicnURL);

  int httpResponseCode = http.GET();
  Serial.print("HTTP Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("AQI Data: " + payload);
      Serial.print("Parsed AQI Data: ");
      JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (!error) {

                int aqiValue = doc["data"]["aqi"]; // Extract AQI value
                Serial.print("AQI: ");
                Serial.println(aqiValue);
                drawAQI(aqiValue);
            } else {
                Serial.println("Failed to parse JSON");
            }
        } else {
            Serial.print("Error on HTTP request: ");
            Serial.println(httpResponseCode);
        }
  http.end();
  yield();  // Allow background tasks to run
  return true;  // Keep the timer running
}

/**/
void drawAQI(int aqiValue) {
  char tempo[20];
  Serial.println("Drawing AQI with Sprites...");
  sprite.createSprite(200, 20);
  sprite.fillSprite(WS_BLACK);

      if (aqiValue <= 10) {
        sprite.setTextColor(TFT_DARKGREEN);
      } else if (aqiValue <= 30) {
        sprite.setTextColor(TFT_GREEN);
      } else if (aqiValue <= 50) {
        sprite.setTextColor(TFT_GREENYELLOW);
      } else if (aqiValue <= 100) {
        sprite.setTextColor(TFT_YELLOW);
      } else if (aqiValue <= 150) {
        sprite.setTextColor(TFT_ORANGE);
      } else if (aqiValue <= 200) {
        sprite.setTextColor(TFT_RED);
        sprite.fillSprite(TFT_YELLOW);
      } else if (aqiValue <= 300) {
        sprite.setTextColor(TFT_PURPLE);
        sprite.fillSprite(TFT_YELLOW);
      } else {
        sprite.setTextColor(TFT_MAROON);
        sprite.fillSprite(TFT_YELLOW);
      }
  sprite.loadFont(arialround20); 
  sprite.setCursor(25, 3);  
  sprintf(tempo, "%d :AQI", (int)aqiValue);  
  sprite.print(tempo);  
  sprite.pushSprite(220, 220);  
  //delay(500); 
  sprite.deleteSprite();       
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
    Serial.println(httpResponseCode); 
    Serial.println(payload);
  } else {
    Serial.print("ERROR: bad HTTP1 request: ");
    Serial.println(httpResponseCode);

    delay(500);
  }

  error = deserializeJson(jsonDoc,payload);
  if (!error) {
    datetime = jsonDoc["dateTime"];
    dt = parseISO8601(String(datetime));
    rtc.setTime(dt.second, dt.minute, dt.hour, dt.day, dt.month, dt.year); 
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
  int daylightSeconds;
  //String  timZon;
  String sunriseStr;
  String sunsetStr;
  int rainProba;
  int wmoCode;
  String Compass;
  
  // get forecast
  http.begin(weatherServer);
  httpResponseCode = http.GET();
  
  if (httpResponseCode > 0){
    payload = http.getString();
    Serial.println(httpResponseCode);  
    Serial.println(payload);
  } else{
    Serial.print("ERROR: bad HTTP1 request: ");
    Serial.println(httpResponseCode);
  }

  error = deserializeJson(jsonDoc,payload);
  if (!error) {

    wmoCode = jsonDoc["current"]["weather_code"];
    currTemp = jsonDoc["current"]["temperature_2m"];  
    currHumi = jsonDoc["current"] ["relative_humidity_2m"]; 
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
    String sunriseStr = jsonDoc["daily"]["sunrise"][0];
    String sunsetStr  = jsonDoc["daily"]["sunset"][0];
    minTemp = jsonDoc["daily"]["temperature_2m_min"][0];
    maxTemp = jsonDoc["daily"]["temperature_2m_max"][0];
    uvIndex = jsonDoc["daily"]["uv_index_max"][0];
    int daylightSeconds = jsonDoc["daily"]["daylight_duration"][0];
    rainProba = jsonDoc["daily"]["precipitation_probability_max"][0];
    String timZon = jsonDoc["timezone_abbreviation"];

int sunriseHour, sunriseMin, sunsetHour, sunsetMin;
sscanf(sunriseStr.c_str(), "%*d-%*d-%*dT%d:%d", &sunriseHour, &sunriseMin);
sscanf(sunsetStr.c_str(), "%*d-%*d-%*dT%d:%d", &sunsetHour, &sunsetMin);


    // Convert daylight duration
    int daylightHours = daylightSeconds / 3600;
    int daylightMinutes = (daylightSeconds % 3600) / 60;

    // display forecast 
    drawForecast(wmoCode,currTemp,currHumi,minTemp,maxTemp,currWind,feelTemp,gustWind,cldTotal,visib,uvIndex,wndDir,dewPo,sunriseHour,sunriseMin,sunsetHour,sunsetMin,daylightHours,daylightMinutes,cldLow,cldMid,cldHi,rainProba); //order matters
      return true;
  }

  return true;
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
  if (wmo == 1) return "Few Clouds";
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

  sprite.createSprite(340,70); 
  sprite.fillSprite(WS_BLACK); 
  sprite.setTextColor(TFT_SKYBLUE);
  now = rtc.getTimeStruct();
  dw = rtc.getDayofWeek();

  sprite.loadFont(arialround20);
  sprite.setTextDatum(CL_DATUM);
  sprintf(tempo,"%s %02d %s %4d",days[dw],now.tm_mday,months[now.tm_mon],(now.tm_year+1900)); 
  sprite.drawString(tempo,10,10);

  //sprite.setTextColor(TFT_DARKGREY);
  //sprite.setTextDatum(MC_DATUM);
  //sprintf(tempo,"UTC-7"); //~~~
  //sprite.drawString(tempo,30,40);

  sprite.setTextColor(TFT_WHITE);
  sprintf(tempo,"%02d:%02d",now.tm_hour,now.tm_min);
  sprite.loadFont(arialround54);
  sprite.drawString(tempo,10,50); 
  sprintf(tempo,":%02d",now.tm_sec); 
  sprite.loadFont(arialround20); 
  sprite.drawString(tempo,155,35); 

  sprite.setTextColor(TFT_BLUE);
  sprite.loadFont(arialround14);
  sprite.setTextDatum(CL_DATUM); 
  sprite.drawString(townName,220,8);

  sprite.setTextColor(TFT_YELLOW);
  sprintf(tempo,"C"); //~ 
  sprite.loadFont(weatherfont60); //~ 
  sprite.drawString(tempo,240,40); //~
  //sprite.setTextColor(TFT_BLUE);
  //sprintf(tempo,"E"); //~ 
  //sprite.loadFont(weatherfont60); //~ 
  //sprite.drawString(tempo,210,40); //~

      //for future am/pm when i figure it out
      sprite.setTextColor(TFT_DARKGREY);
      sprite.loadFont(arialround14);
      sprintf(tempo,"24hr");
      sprite.drawString(tempo,157,58); // 102,46

      //sprite.setTextColor(TFT_DARKGREY);
      //sprintf(tempo,"TZ: %4.6f ", timeZone);
      //sprite.drawString(tempo,110,55);

  sprite.pushSprite(0,0);
  sprite.deleteSprite();

  return true;
}

void drawForecast(int wmo, float currTemp, short currHumi, float minTemp, float maxTemp, float currWind, float feelTemp, float gustWind, short cldTotal, float visib, float uvIndex, float wndDir,float dewPo, int sunriseHour, int sunriseMin,int sunsetHour,int sunsetMin,int daylightHours, int daylightMinutes, short cldLow, short cldMid, short cldHi, short rainProba) {
  char tempo[10];
sprite.createSprite(320,170); 
  sprite.fillSprite(WS_BLACK);
  sprite.setTextColor(TFT_SKYBLUE); 
  sprite.loadFont(arialround20);
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(getDescriptionFromWMO(wmo),0,90); 

///>>>TEST AREA
      //sprite.setTextColor(TFT_DARKGREY);
      //sprintf(tempo,"TZ: %2d", timZon);
      //sprite.drawString(tempo,5,25);//115,20

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround20);
  sprintf(tempo,"d:%dh|%dm", daylightHours, daylightMinutes); 
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(tempo,320,30);

  sprite.setTextColor(TFT_LIGHTGREY);
  sprintf(tempo,"r:%02d:%02d", sunriseHour, sunriseMin);
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,0,30);

  sprite.setTextColor(TFT_LIGHTGREY);
  sprintf(tempo,"s:%02d:%02d", sunsetHour, sunsetMin);
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,120,30);

  
  //sprite.pushSprite(0,185);
  //sprite.deleteSprite();

  //Display town name 
  //sprite.createSprite(270,170); //   270,170    create space for sprite ? ? made it bigger sprite.createSprite(170,136);
  //sprite.fillSprite(WS_BLACK);
  ///sprite.setTextColor(TFT_BLUE);
  //sprite.loadFont(arialround14);
  //sprite.setTextDatum(CL_DATUM); //sprite.setTextDatum(TR_DATUM);
  //sprite.drawString(townName,5,20); //170,0    168,10

   //display Temp
  sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround44);
  sprintf(tempo,"%4.1f",currTemp);
  sprite.setTextDatum(BR_DATUM); //~~~
  sprite.drawString(tempo,200,75); 

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);
  sprintf(tempo,"°C"); // separated this from lines above to make °C smaller font
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,205,50);
      
      if (feelTemp < -27.00) { 
          sprite.setTextColor(TFT_RED);
      }
      else if (feelTemp < -21.00) { 
          sprite.setTextColor(TFT_ORANGE);
      }
      else if (feelTemp < -15.00) {  
          sprite.setTextColor(TFT_YELLOW);
      }
      else if (feelTemp < -9.00) { 
          sprite.setTextColor(TFT_SKYBLUE);
      }
      else {
          sprite.setTextColor(TFT_WHITE);
      }
  sprite.loadFont(arialround36);
  sprintf(tempo, "%3.0f", feelTemp);
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo, 130, 160);

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);
  sprintf(tempo,"°C"); // separated this from lines above to make °C smaller font
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,185,140); 

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround20);  
  sprintf(tempo,"%2d %%:Humi", currHumi);
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(tempo,320,90); 

      if (currWind > 55) {   // Check the higher threshold first
      sprite.setTextColor(TFT_RED);
      }
      else if (currWind > 40) { 
        sprite.setTextColor(TFT_ORANGE);
      }
      else if (currWind > 20) {
      sprite.setTextColor(TFT_YELLOW);
      }
      else {
      sprite.setTextColor(TFT_LIGHTGREY);
      }
sprite.loadFont(arialround20);
sprintf(tempo, "%3.0f      :Wind", currWind);
sprite.setTextDatum(BR_DATUM);
sprite.drawString(tempo, 320, 110);
sprite.loadFont(arialround14); // added this to make 'kph' smaller font
sprintf(tempo,"kph");
sprite.drawString(tempo, 265, 110);


      if (gustWind > 65) {   // Check the higher threshold first
          sprite.setTextColor(TFT_RED);
      }
      else if (gustWind > 45) { 
        sprite.setTextColor(TFT_ORANGE);
      }
      else if (gustWind > 30) {
          sprite.setTextColor(TFT_YELLOW);
      }
      else {
          sprite.setTextColor(TFT_LIGHTGREY);
      }
sprite.loadFont(arialround20);
sprintf(tempo, "%3.0f      :Gust", gustWind);
sprite.setTextDatum(BR_DATUM);
sprite.drawString(tempo, 320, 130);
sprite.loadFont(arialround14); // added this to make 'kph' smaller font
sprintf(tempo,"kph");
sprite.drawString(tempo, 265, 130);

if (wndDir >= 348.75 || wndDir < 11.25)  {
  sprintf(tempo," N"); //return TXT_N;
  }
  if (wndDir >=  11.25 && wndDir < 33.75)  {

  sprintf(tempo,"NNE"); //return TXT_NNE;
  }
  if (wndDir >=  33.75 && wndDir < 56.25)  {
  sprintf(tempo,"NE"); //return TXT_NE;
  }
  if (wndDir >=  56.25 && wndDir < 78.75)  {
  sprintf(tempo,"ENE"); //return TXT_ENE;
  }
  if (wndDir >=  78.75 && wndDir < 101.25) {
  sprintf(tempo," E"); //return TXT_E;
  }
  if (wndDir >= 101.25 && wndDir < 123.75) {
  sprintf(tempo,"ESE");//return TXT_ESE;
  }
  if (wndDir >= 123.75 && wndDir < 146.25) {
  sprintf(tempo,"SE");//return TXT_SE;
  }
  if (wndDir >= 146.25 && wndDir < 168.75) {
  sprintf(tempo,"SSE");//return TXT_SSE;
  }
  if (wndDir >= 168.75 && wndDir < 191.25) {
  sprintf(tempo," S");//return TXT_S;
  }
  if (wndDir >= 191.25 && wndDir < 213.75) {
  sprintf(tempo,"SSW");//return TXT_SSW;
  }
  if (wndDir >= 213.75 && wndDir < 236.25) {
  sprintf(tempo,"SW");//return TXT_SW;
  }
  if (wndDir >= 236.25 && wndDir < 258.75) {
  sprintf(tempo,"WSW");//return TXT_WSW;
  }
  if (wndDir >= 258.75 && wndDir < 281.25) {
  sprintf(tempo," W");//return TXT_W;
  }
  if (wndDir >= 281.25 && wndDir < 303.75) {
  sprintf(tempo,"WNW");//return TXT_WNW;
  }
  if (wndDir >= 303.75 && wndDir < 326.25) {
  sprintf(tempo,"NW"); //return TXT_NW;
  }
  if (wndDir >= 326.25 && wndDir < 348.75) {
  sprintf(tempo,"NNW");//return TXT_NNW;
  }
sprite.setTextColor(TFT_LIGHTGREY);
sprite.loadFont(arialround20);
sprite.drawString(tempo,280,150);
sprite.setTextColor(TFT_LIGHTGREY);
sprite.loadFont(arialround20);
sprintf(tempo,":Dir");  
sprite.setTextDatum(BR_DATUM);
sprite.drawString(tempo,320,150); 
/*
sprite.setTextColor(TFT_LIGHTGREY);
sprite.loadFont(arialround20);  
sprintf(tempo,"%4.0f°c:Dew", dewPo);
sprite.setTextDatum(BR_DATUM);
sprite.drawString(tempo,320,170);  
*/
sprite.setTextColor(TFT_LIGHTGREY);
sprite.loadFont(arialround20);  
sprintf(tempo,"Cld: %2d %%", cldTotal);
sprite.setTextDatum(BL_DATUM);
sprite.drawString(tempo,0,110);  

sprite.setTextColor(TFT_LIGHTGREY);
sprite.loadFont(arialround20);  
sprintf(tempo,"Vis: %4.1f", visib /1000);
sprite.setTextDatum(BL_DATUM);
sprite.drawString(tempo,0,130);  
sprite.loadFont(arialround14); // added this to make 'km' smaller font
sprintf(tempo,"km");
sprite.drawString(tempo, 90, 130);
/*
    if (rainProba > 75) {   // Check the higher threshold first
      sprite.setTextColor(TFT_GREENYELLOW);
    }
    else {
      sprite.setTextColor(TFT_LIGHTGREY);
    }
  sprite.loadFont(arialround20);  
  sprintf(tempo,"PoP: %2d %%",rainProba);
  //sprite.setTextDatum(CR_DATUM);
  //sprite.drawString(tempo,125,20);  
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,0,150); 
*/

      if (uvIndex >= 11) {   // Check the higher threshold first
        sprite.setTextColor(TFT_PURPLE);
        sprite.loadFont(arialround20);  
        sprintf(tempo,"XTRM");
      }
      else if (uvIndex >= 8) {
        sprite.setTextColor(TFT_RED);
        sprite.loadFont(arialround20);  
        sprintf(tempo,"VHIGH");
      }
      else if (uvIndex >= 6) {
        sprite.setTextColor(TFT_ORANGE);
        sprite.loadFont(arialround20);  
        sprintf(tempo,"High");
      }
      else if (uvIndex >= 3) {
        sprite.setTextColor(TFT_YELLOW);
        sprite.loadFont(arialround20);  
        sprintf(tempo,"Med");
      }
      else {
        sprite.setTextColor(TFT_DARKGREEN);
        sprite.loadFont(arialround20);  
        sprintf(tempo,"Low");
}
  //sprite.loadFont(arialround14);  
  //sprintf(tempo,"[%2.0f]",uvIndex);
  sprite.drawString(tempo,65,150); 
  sprite.setTextColor(TFT_LIGHTGREY); // 
  sprintf(tempo,"UVdx:"); 
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,0,150); 

  //sprite.setTextColor(TFT_LIGHTGREY);
  //sprite.loadFont(arialround14);  
  //sprintf(tempo,"PoP: %2d %%",rainProba);
  //sprite.setTextDatum(CR_DATUM);
  //sprite.drawString(tempo,0,115);  

  sprite.setTextColor(TFT_SKYBLUE);
  sprite.loadFont(arialround14);
  sprintf(tempo,"Lo: %4.1f°c",minTemp);
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,0,55);

  sprite.setTextColor(TFT_GREENYELLOW);
  sprite.loadFont(arialround14);
  sprintf(tempo,"Hi: %4.1f°c",maxTemp);
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(tempo,320,55);

  sprite.pushSprite(0,70);
  sprite.deleteSprite();

  //>>
}
