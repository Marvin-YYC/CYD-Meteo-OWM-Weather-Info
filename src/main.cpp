#include <Arduino.h>
#include "WeatherStation.h"
#include <arduino-timer.h>

bool isDSTActive = false;  // Global variable to store DST status
bool getAuroraData(void*);  
void drawAurora(float auroraValue); 
void drawAQI(int aqiValue, int stationIndex);  // AQI Multiple station
bool getOWMData(void*);
void drawOWMValue();
void drawOWMValue(String owmIcon,int owmCode,String owmDesc,String owmCond,float owmTemp,int owmHumi,float owmWind,float owmFeelTemp,float owmGust,int owmCldCvr,int owmVisib,float owmWndDir);

int lastUpdateHour = -1;
int lastUpdateMinute = -1;

//bool getaqiData(void*); // AQI Single station
//void drawAQI(int aqiValue); // AQI Single station

#define TFT_BL 21
#define BACKLIGHT_PIN 21

//--- Time object
auto timer = timer_create_default();

void setup() {
  Serial.begin(115200);
  Serial.begin(115200);
  bool getAuroraData(void*);
  bool getaqiData(void*);
  //bool getCanAirData(void*);

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
  ledcWrite(0, 150); // Adjust brightness (0-255) 128 adjust brightness level here<<< 80
//////~~~~~~~~

  tft.fillScreen(WS_BLACK);
  drawTime(NULL);     //  fetch data once immediately on boot
  getMeteoForecast(NULL);
  bool getAuroraData(void*);
  getAuroraData(nullptr);  
  bool getaqiData(void*);
  getaqiData(nullptr);
  bool getOWMData(void*);
  getOWMData(nullptr);
  
  //bool getCanAirData(void*);
  //getCanAirData(nullptr);

  //--- Create Timers for main Weather Station functions
  timer.every(500,drawTime);                // Every 500ms, display time
  timer.every(15*60*1000,getTime);          // Fetch time data every 15mn
  timer.every(20*60*1000,getMeteoForecast); // Fetch Open-Meteo data Every 20min
  timer.every(20*60*1000,getAuroraData);    // Fetch aurora data every 20 minutes ||| FYI: Aurora Watch website updates at 11 minutes past the hour.
  timer.every(20*60*1000,getaqiData);       // Fetch aqi data every 20 minutes
  timer.every(20*60*1000,getOWMData);       // Fetch OWM data every 20 minutes
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
  //Timer is not accurate when in deep sleep. It may wake up a several minutes early/late.
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
          delay(500);
          if (WiFi.status() != WL_CONNECTED) {
              Serial.println("Failed to reconnect.");
              return true;  // Keep the timer running
          }
      }

    HTTPClient http;
    http.setTimeout(500); 
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
      drawAurora(auroraValue);  
  } else {
      Serial.print("HTTP Request Failed! Code: ");
      Serial.println(httpResponseCode);
  }

  http.end();
  yield();  // Allow background tasks to run
  return true; 
}

void drawAurora(float auroraValue) {
  char tempo[20];
  Serial.println("Drawing Aurora with Sprites...");
  sprite.createSprite(120, 20);// 
  sprite.fillSprite(WS_BLACK); 

        if (auroraValue >= 80) { 
          sprite.setTextColor(TFT_RED);
        }
        else if (auroraValue > 65) { 
        sprite.setTextColor(TFT_YELLOW);
        }
        else if (auroraValue > 50) { 
          sprite.setTextColor(TFT_GREENYELLOW);
        }
        else {
        sprite.setTextColor(TFT_LIGHTGREY);
        }
  sprite.loadFont(arialround20); 
  sprite.setCursor(0, 3);  
  sprintf(tempo, "PoAB: %2d%%", (int)auroraValue);  
  sprite.print(tempo);  
  sprite.pushSprite(0, 220);  
  delay(500); 
  sprite.deleteSprite();       
}
//////////////////////////////////

const int numStations = sizeof(aqicnURLs) / sizeof(aqicnURLs[2]);  // Number of stations ZERO if only one, ONE if two stations

bool getaqiData(void *) {
    Serial.println("Fetching AQI Data...");

    if (WiFi.status() != WL_CONNECTED) {  
        Serial.println("WiFi not connected, trying to reconnect...");
        WiFi.disconnect();
        WiFi.reconnect();
        delay(500);  
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Failed to reconnect.");
            return true;  
        }
    }

    for (int i = 0; i < numStations; i++) {  
        HTTPClient http;
        http.setTimeout(500);  
        http.begin(aqicnURLs[i]);  

        int httpResponseCode = http.GET();
        Serial.print("HTTP Response Code for Station ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(httpResponseCode);

        if (httpResponseCode > 0) {
            String payload = http.getString();
            Serial.println("AQI Data: " + payload);
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

          if (!error) {
              int aqiValue = doc["data"]["aqi"];  
              Serial.print("Station ");
              Serial.print(i);
              Serial.print(" AQI: ");
              Serial.println(aqiValue);

              drawAQI(aqiValue, i);  // Pass station index for proper positioning
          } else {
              Serial.println("Failed to parse JSON");
          }
          } else {
              Serial.print("Error fetching data for station ");
              Serial.println(i);
          }
          http.end();
          delay(500);  // Small delay between requests
      }

    yield();
    return true;  
}

void drawAQI(int aqiValue, int stationIndex) {
  char tempo[20];
  Serial.println("Drawing AQI with Sprites...");
  sprite.createSprite(220, 20); 
  sprite.fillSprite(WS_BLACK);  

  if (aqiValue <= 15) {
      sprite.setTextColor(TFT_DARKGREEN);
  } else if (aqiValue <= 25) {
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
      sprite.setTextColor(TFT_RED); 
      sprite.fillSprite(TFT_YELLOW);
  }
  sprite.loadFont(arialround14); 
  sprite.setTextDatum(BR_DATUM);
  sprite.setCursor(0, 5);  
  sprintf(tempo,"aqI: %d",aqiValue); 
  sprite.print(tempo);
  // Adjust position based on station index
  int xPos = 140 + (stationIndex * 60);  // Right side position Spacing
  sprite.pushSprite(xPos, 220); //220
  delay(500); 
  sprite.deleteSprite();  
}

  bool getOWMData(void *) { 
  HTTPClient http;
  int httpResponseCode;
  JsonDocument jsonDoc;
  String payload;
  DeserializationError error;
  String owmIcon;
  float owmTemp;
  int owmHumi;
  float owmWind;
  float owmGust; 
  float owmFeelTemp; 
  float owmVisib;
  int owmCldCvr;
  int owmCode;
  String owmDesc;
  String owmCond;

  float owmWndDir;
  
  // get OWM Data
  http.begin(owmServerURL);
  httpResponseCode = http.GET();
  
  if (httpResponseCode > 0){
    payload = http.getString();
    Serial.println(httpResponseCode);  
    Serial.println(payload);
    delay(50);
  } else{
    Serial.print("ERROR: bad HTTP1 request: ");
    Serial.println(httpResponseCode);
    delay(1000);
  }

  error = deserializeJson(jsonDoc,payload);
  if (!error) {
    owmIcon = jsonDoc["weather"][0]["icon"].as<String>();
    owmCode = jsonDoc["weather"][0]["id"];
    owmDesc = jsonDoc["weather"][0]["description"].as<String>();
    owmTemp = jsonDoc["main"]["temp"];  
    owmHumi = jsonDoc["main"]["humidity"]; 
    owmCldCvr = jsonDoc["clouds"]["all"];
    owmVisib = jsonDoc["visibility"];
    owmCond = jsonDoc["weather"][0]["main"].as<String>();
    owmWind = jsonDoc["wind"]["speed"];
    owmGust = jsonDoc["wind"]["gust"];
    owmFeelTemp = jsonDoc["main"]["feels_like"];
    int owmWndDir = 0;

        if (jsonDoc["wind"]["deg"].is<int>()) {
          owmWndDir = jsonDoc["wind"]["deg"].as<int>();  // Extract wind direction in degrees
            }
        if (owmIcon.endsWith("d")) {
          Serial.println("Day icon detected ☀️");
        } else if (owmIcon.endsWith("n")) {
          Serial.println("Night icon detected 🌙");
            }
        delay(500);        
    http.end();

    // display forecast  owmDesc
    drawOWMValue(owmIcon,owmCode,owmDesc,owmCond,owmTemp,owmHumi,owmWind,owmFeelTemp,owmGust,owmCldCvr,owmVisib,owmWndDir);
      return true;
    }
      return true;
  }


const char* getIconOWM(int owmCode, String owmIcon) {
  if (owmCode == 800 && owmIcon.endsWith("d")) { sprite.setTextColor(TFT_GOLD); return "N"; }  // Clear day/sun
  if (owmCode == 800 && owmIcon.endsWith("n")) {sprite.setTextColor(TFT_SILVER); return "I"; }  // Clear night/moon
  if ((owmCode == 801 || owmCode == 802) && owmIcon.endsWith("d") ) { sprite.setTextColor(TFT_GOLD); return "C"; }  // Few Clouds day/sun
  if ((owmCode == 801 || owmCode == 802) && owmIcon.endsWith("n")) { sprite.setTextColor(TFT_SILVER); return "G"; }  // Few Clouds night/moon
  if (owmCode == 803) { sprite.setTextColor(TFT_SILVER); return "P"; }  // Partly Cloudy
  if (owmCode == 804) { sprite.setTextColor(TFT_SKYBLUE); return "O"; }  // Overcast/Cloudy
  if (owmCode == 701 || owmCode == 711 || owmCode == 721 || owmCode == 741) { sprite.setTextColor(TFT_LIGHTGREY); return "F"; }  // Fog/Mist
  if (owmCode == 500 || owmCode == 520 || owmCode == 521) { sprite.setTextColor(TFT_SKYBLUE); return "M"; }  // Light rain
  if (owmCode == 501 || owmCode == 502 || owmCode == 503 || owmCode == 504 || owmCode == 522) { sprite.setTextColor(TFT_SKYBLUE); return "B"; }  // Rain
  if (owmCode >= 300 && owmCode <= 321) { sprite.setTextColor(TFT_SKYBLUE); return "M"; }  // Drizzle / Light rain
  if (owmCode == 511 || owmCode == 611 || owmCode == 612 || owmCode == 613 || owmCode == 615|| owmCode >= 300 && owmCode <= 321) { sprite.setTextColor(TFT_SKYBLUE); return "ME"; }  // Freezing rain
  if (owmCode == 600 || owmCode == 620 ) { sprite.setTextColor(TFT_WHITE); return "L"; }  // Light snow
  if (owmCode == 601 || owmCode == 621) { sprite.setTextColor(TFT_WHITE); return "D"; }  // Moderate Snow
  if (owmCode == 622 || owmCode == 602) { sprite.setTextColor(TFT_WHITE); return "D"; }  // Heavy Snow
  if (owmCode >= 200 && owmCode <= 231) { sprite.setTextColor(TFT_ORANGE); return "K"; }  // Storms
  if (owmCode == 781) { sprite.setTextColor(TFT_RED); return "AA"; }  // TORNADO
  sprite.setTextColor(TFT_DARKGREY);
  return "J";  // Default: Unknown (umbrella)
}

const char* getWindDir(int degrees) { // Cardinal
  if (degrees < 23 || degrees >= 338) return "N";    // North
  if (degrees < 68) return "NE";                     // Northeast
  if (degrees < 113) return "E";                     // East
  if (degrees < 158) return "SE";                    // Southeast
  if (degrees < 203) return "S";                     // South
  if (degrees < 248) return "SW";                    // Southwest
  if (degrees < 293) return "W";                     // West
  return "NW";                                       // Northwest
}

  /**/
  void drawOWMValue(String owmIcon,int owmCode,String owmDesc,String owmCond,float owmTemp,int owmHumi,float owmWind,float owmFeelTemp,float owmGust,int owmCldCvr,int owmVisib,float owmWndDir) {
  char tempo[64];

  Serial.println("Drawing Open Weather with Sprites...");
  sprite.createSprite(142, 70);
  sprite.fillSprite(WS_BLACK); //WS_BLACK

        //display Temp
        // Extract integer and decimal to allow for smaller decimal font
      int intPart = (int)owmTemp;
      int decPart = abs((int)((owmTemp - intPart) * 10 + 0.5));  // Round correctly
      // Convert integer part to string
      String intString = String(intPart);
      // Ensure only one negative sign (attach it to the integer part)
          if (owmTemp < 0) {
            intString = "-" + String(abs(intPart));  // Add negative sign and decimal "." manually
      }
      // Ensure only one decimal place and prevent "10"
      if (decPart == 10) {  
        intPart += (owmTemp < 0) ? -1 : 1;  // Carry over the rounding
        decPart = 0;  // Reset decimal part to zero
        intString = String(intPart);  // Update integer part
      }
  intString += ".";
  sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround26); 
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(intString,126,14); // 126,14 127,14  128,24
  //sprintf(tempo,"%2.1f°",owmTemp);  //orig
  //sprite.drawString(tempo,142,13);  //orig
  sprite.loadFont(arialround20);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(String(decPart),128,16);// 128,16 142,14
  sprite.setTextColor(TFT_LIGHTGREY); 
  sprite.loadFont(arialround14);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString("°", 128,7); // 128,7

          if (owmFeelTemp < -27.00) { 
            sprite.setTextColor(TFT_RED);
        }
        else if (owmFeelTemp < -21.00) { 
            sprite.setTextColor(TFT_ORANGE);
        }
        else if (owmFeelTemp < -15.00) {  
            sprite.setTextColor(TFT_YELLOW);
        }
        else if (owmFeelTemp < -9.00) { 
            sprite.setTextColor(TFT_SKYBLUE);
        }
        else if (owmFeelTemp < -1.00) { 
          sprite.setTextColor(TFT_WHITE);
        }
        else if (owmFeelTemp < 10.00) { 
          sprite.setTextColor(TFT_LIGHTGREY);
        }
        else if (owmFeelTemp < 20.00) { 
        sprite.setTextColor(TFT_DARKGREY);
        }
          else if (owmFeelTemp >= 25.00) { 
        sprite.setTextColor(TFT_MAGENTA);
        }
        else {
            sprite.setTextColor(TFT_DARKGREY);
        }
  //sprite.setTextDatum(CR_DATUM);
  //sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround20); 
  sprintf(tempo,"%2.0f",owmFeelTemp);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,136,35); //142,35
  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString("°", 142,31);

  //sprite.setTextColor(TFT_WHITE);
  //sprite.loadFont(arialround14); 
  //sprintf(tempo,"%2d%%",owmHumi);
  //sprite.drawString(tempo,130,47);

      if (owmWind * 3.6 > 64) {   
        sprite.setTextColor(TFT_RED);
        }
        else if (owmWind * 3.6 > 44) { 
          sprite.setTextColor(TFT_ORANGE);
        }
        else if (owmWind * 3.6 > 19) {
        sprite.setTextColor(TFT_YELLOW);
        }
        else {
        sprite.setTextColor(TFT_WHITE);
        }
  //sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround14); 
  sprintf(tempo,"%2.0f /",owmWind * 3.6); // mulitlply m/s by 3.6 to obtain km/hr
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,120,64); 

      if (owmGust * 3.6 > 69) {   
        sprite.setTextColor(TFT_RED);
    }
    else if (owmGust * 3.6 > 45) { 
      sprite.setTextColor(TFT_ORANGE);
    }
    else if (owmGust * 3.6 > 25) {
        sprite.setTextColor(TFT_YELLOW);
    }
    else {
        sprite.setTextColor(TFT_WHITE);
    }
  //sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround14); 
  sprintf(tempo,"%2.0f",owmGust * 3.6); // mulitlply m/s by 3.6 to obtain km/hr
  sprite.drawString(tempo,141,64);

  sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround14); 
  sprintf(tempo, "%2d%%", owmHumi);
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,142,52); //CL.76,36  70,32   6,25

  sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround14); 
  sprintf(tempo, "[%s]", getWindDir(owmWndDir));
  sprite.setTextDatum(CR_DATUM);
  sprite.drawString(tempo,87,64); //85,64  70,32 6,25
  
  sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround14); 
  sprintf(tempo,"%2.0f",owmCldCvr * 0.1); // converted cloud cover to make smaller, 100% = 10 80% = 8, etc.   original - sprintf(tempo,"%2d",owmCldCvr);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,2,22);  //4,64

      if (owmVisib * 0.001 > 9.99 ) { 
      sprintf(tempo,">%2.0f km",owmVisib * 0.001); // *0.001 meters to km. OWM does not provide visibility data greater than 10km.  This removes decimal place and adds '>' when vis is greater than 9.99km
            }
      else {
      sprintf(tempo,"%2.1f km",owmVisib * 0.001);
            }
  sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround14); 
  //sprintf(tempo,"%2.1f",owmVisib* 0.001);
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,2,64); //59,64

  sprite.loadFont(weatherfont60); 
  sprintf(tempo, "%s", getIconOWM(owmCode,owmIcon));
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,36,15);  //35,15
/*
  sprite.setTextColor(TFT_DARKGREY);
  sprite.loadFont(arialround14); 
  sprintf(tempo,"%d",owmCode); //just using this for debug weather code
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,2,6);
*/
  sprite.setTextColor(TFT_DARKGREY);
  sprite.loadFont(arialround09); 
  sprintf(tempo, "%s", owmCond.c_str()); // short description
  sprite.setTextDatum(CL_DATUM);
  sprite.drawString(tempo,2,6);
  //sprite.drawString(tempo,4,52); //4,38
/*
  sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround14); 
  //sprintf(tempo, "%s", owmDesc.c_str());  // long description
  sprite.setTextDatum(CL_DATUM);
  //sprite.drawString(tempo,4,40);//4,50
  sprite.drawString(owmDesc,4,40);
  sprite.drawLine(0, 0, 0, 70, TFT_LIGHTGREY);
*/
  sprite.setTextColor(TFT_CYAN);
  sprite.loadFont(arialround14); 
  sprite.setTextDatum(CL_DATUM);
  String desc1 = owmDesc.substring(0, 15);  // Adjust these lengths per font size //0,18
  String desc2 = owmDesc.length() > 15 ? owmDesc.substring(15, 43) : ""; // >18  owmDesc.substring(18, 35) : "";
  sprite.drawString(desc1, 2, 43);  // First line  //(desc1, 4, 38); //35
  if (desc2.length() > 0) {
    sprite.drawString(desc2, 2, 53);  // Second line //(desc2, 4, 48); // 45
  }
    sprite.pushSprite(178,0); // 190,0
    delay(500); 
    sprite.deleteSprite();       // Free memory
  }

/*
// For single station AQI information
bool getaqiData(void *) {
  Serial.println("Fetching AQI Data...");

  if (WiFi.status() != WL_CONNECTED) {  
      Serial.println("WiFi not connected, trying to reconnect...");
      WiFi.disconnect();
      WiFi.reconnect();
      delay(500); 
      if (WiFi.status() != WL_CONNECTED) {
          Serial.println("Failed to reconnect.");
          return true;  // Keep the timer running
      }
  }
  HTTPClient http;
  http.setTimeout(500);  // Set timeout to avoid freezing
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
*/
/*
// For single station AQI information
void drawAQI(int aqiValue) {
  char tempo[20];
  Serial.println("Drawing AQI with Sprites...");
  sprite.createSprite(200, 20);// width,height sprite.createSprite(120, 25);
  sprite.fillSprite(WS_BLACK);  //sprite.fillSprite(WS_BLACK); 

      if (aqiValue <= 15) {
        sprite.setTextColor(TFT_DARKGREEN);
      } else if (aqiValue <= 25) {
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
        sprite.setTextColor(TFT_RED); 
        sprite.fillSprite(TFT_YELLOW);
    }
  sprite.loadFont(arialround20); 
  sprite.setCursor(40, 3);  // 27,3  Use setCursor instead of setTextDatum postion of cursor within the sprite
  sprintf(tempo, "%d :AQI", (int)aqiValue);  // Convert float to int
  sprite.print(tempo);  // Print text to sprite
  sprite.pushSprite(205, 220);  // Push sprite to display position  ) 0 full left 220 bottom left. (right number is up / down, left number is left/right)
  delay(500); 
  sprite.deleteSprite();       // Free memory
}
*/

bool getTime(void *) {
  HTTPClient http;
  int httpResponseCode;
  JsonDocument jsonDoc;
  String payload;
  DeserializationError error;
  const char * datetime;
  DateTime dt;
  bool isDSTActive = false;

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
    isDSTActive = jsonDoc["dstActive"]; 
    Serial.print("DST Active: ");
    Serial.println(isDSTActive ? "true" : "false");  //  Log DST status
    http.end();
    }
  return true;
} 

bool getMeteoForecast(void *) {
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
  short cldTotal;
  float dewPo;
  float uvIndex;
  float wndDir;
  int isDay, cape, litPot;
  int daylightSeconds;
  String sunriseStr;
  String sunsetStr;
  int rainProba;
  int meteoCode;
  String Compass;
  
  // get forecast
  http.begin(meteoWeatherServer);
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

    meteoCode = jsonDoc["current"]["weather_code"];
    currTemp = jsonDoc["current"]["temperature_2m"];  
    currHumi = jsonDoc["current"] ["relative_humidity_2m"]; 
    cldTotal = jsonDoc["current"]["cloud_cover"];
    visib = jsonDoc["current"]["visibility"];
    wndDir = jsonDoc["current"]["wind_direction_10m"];
    dewPo = jsonDoc["current"]["dew_point_2m"];
    currWind = jsonDoc["current"]["wind_speed_10m"];
    gustWind = jsonDoc["current"]["wind_gusts_10m"];
    feelTemp = jsonDoc["current"]["apparent_temperature"];
    //cldLow = jsonDoc["current"]["cloud_cover_low"];
    //cldMid = jsonDoc["current"]["cloud_cover_mid"];
    //cldHi = jsonDoc["current"]["cloud_cover_high"];
    litPot = jsonDoc["current"]["lightning_potential"];
    cape = jsonDoc["current"]["cape"];
    isDay = jsonDoc["current"]["is_day"];
    String sunriseStr = jsonDoc["daily"]["sunrise"][0];
    String sunsetStr  = jsonDoc["daily"]["sunset"][0];
    minTemp = jsonDoc["daily"]["temperature_2m_min"][0];
    maxTemp = jsonDoc["daily"]["temperature_2m_max"][0];
    uvIndex = jsonDoc["daily"]["uv_index_max"][0];
    int daylightSeconds = jsonDoc["daily"]["daylight_duration"][0];
    rainProba = jsonDoc["daily"]["precipitation_probability_max"][0];
    String timZon = jsonDoc["timezone_abbreviation"];

    //&hourly=precipitation_probability,uv_index,

    http.end();

int sunriseHour, sunriseMin, sunsetHour, sunsetMin;
sscanf(sunriseStr.c_str(), "%*d-%*d-%*dT%d:%d", &sunriseHour, &sunriseMin);
sscanf(sunsetStr.c_str(), "%*d-%*d-%*dT%d:%d", &sunsetHour, &sunsetMin);


    // Convert daylight duration
    int daylightHours = daylightSeconds / 3600;
    int daylightMinutes = (daylightSeconds % 3600) / 60;

    // display forecast 
    drawMeteoForecast(meteoCode,currTemp,currHumi,minTemp,maxTemp,currWind,feelTemp,gustWind,cldTotal,visib,uvIndex,wndDir,dewPo,sunriseHour,sunriseMin,sunsetHour,sunsetMin,daylightHours,daylightMinutes,rainProba,isDay,cape,litPot); //order matters
      return true;
  }
  
  return true;
}

const char* getIconFromMeteo(int meteo, int isDay) {
  if (meteo == 0 & isDay == 1) return (sprite.setTextColor(TFT_GOLD), "N");  // Clear day/sun
  if (meteo == 0 & isDay == 0) return (sprite.setTextColor(TFT_SILVER), "I");  // Clear night/moon
  if (meteo == 1 & isDay == 1) return (sprite.setTextColor(TFT_GOLD), "C");  // Few Clouds day/sun
  if (meteo == 1 & isDay == 0) return (sprite.setTextColor(TFT_SILVER), "G");  // Few Clouds night/moon
  if (meteo == 2) return (sprite.setTextColor(TFT_SILVER), "P");  // Partly Cloudy
  if (meteo == 3) return (sprite.setTextColor(TFT_SKYBLUE), "O");  // Overcast/Cloudy
  if (meteo == 45) return ((TFT_LIGHTGREY), "F");  // Fog
  if (meteo == 48) return ((TFT_WHITE), "F");  // Freezing Fog
  if (meteo == 51) return (sprite.setTextColor(TFT_SKYBLUE), "M");  // Light Drizzle
  if (meteo == 53) return (sprite.setTextColor(TFT_SKYBLUE), "M");  // Mod Drizzle
  if (meteo == 55) return (sprite.setTextColor(TFT_SKYBLUE), "M");  // Hvy Drizzle
  if (meteo == 56) return (sprite.setTextColor(TFT_GREENYELLOW), "M");  // Lt Frzng Drizzle
  if (meteo == 57) return (sprite.setTextColor(TFT_GREENYELLOW), "M");  // Hvy Frzng Drizzle
  if (meteo == 61) return (sprite.setTextColor(TFT_SKYBLUE), "M");  // Light Rain
  if (meteo == 63) return (sprite.setTextColor(TFT_SKYBLUE), "M");  // Mod Rain
  if (meteo == 65) return (sprite.setTextColor(TFT_SKYBLUE), "M");  // Hvy Rain
  if (meteo == 66) return (sprite.setTextColor(TFT_GREENYELLOW), "M");  // Lt Frzng Rain
  if (meteo == 67) return (sprite.setTextColor(TFT_GREENYELLOW), "M");  // Hvy Frzng Rain
  if (meteo == 71) return (sprite.setTextColor(TFT_WHITE),"L");  // Light Snow
  if (meteo == 73) return (sprite.setTextColor(TFT_WHITE),"L");  // Mod Snow
  if (meteo == 75) return (sprite.setTextColor(TFT_WHITE),"D");  // Heavy Snow
  if (meteo == 77) return (sprite.setTextColor(TFT_WHITE),"L");  // Snow Grains
  if (meteo == 80 ) return (sprite.setTextColor(TFT_SKYBLUE), "M");  // Lt Rain Shwrs
  if (meteo == 81 ) return (sprite.setTextColor(TFT_SKYBLUE), "M");  // Mod Rain Shwrs
  if (meteo == 82 ) return (sprite.setTextColor(TFT_GREENYELLOW), "K");  // Violent Rain Shwrs
  if (meteo == 85 ) return (sprite.setTextColor(TFT_WHITE), "L");  // Lt Snow Shwrs
  if (meteo == 86 ) return (sprite.setTextColor(TFT_WHITE), "L");  // Hvy Snow Shwrs
  if (meteo == 95 ) return (sprite.setTextColor(TFT_YELLOW), "K");  // Thunderstorm
  if (meteo == 96 ) return (sprite.setTextColor(TFT_ORANGE), "K");  // T-Storm w/Hail
  if (meteo == 99 ) return (sprite.setTextColor(TFT_RED), "K");  // T-Storm w/Hvy Hail
  return (sprite.setTextColor(TFT_DARKGREY), "J");  // Unknown J = UMBRELLA
}
// return Short Weather Description from meteo code
String getDescFromMeteo(int meteo) {
  if (meteo == 0) return "Clear";
  if (meteo == 1) return "Few Clouds";
  if (meteo == 2) return "Partly Cloudy";
  if (meteo == 3) return "Overcast";
  if (meteo == 45) return "Fog";
  if (meteo == 48) return "Freezing Fog";
  if (meteo == 51) return "Light Drizzle";
  if (meteo == 53) return "Mod Drizzle";
  if (meteo == 55) return "Hvy Drizzle";
  if (meteo == 56) return "Lt Frzng Drizzle";
  if (meteo == 57) return "Hvy Frzng Drizzle";
  if (meteo == 61) return "Light Rain";
  if (meteo == 63) return "Mod Rain";
  if (meteo == 65) return "Heavy Rain";
  if (meteo == 66) return "Lt Frzng Rain";
  if (meteo == 67) return "Hvy Frzng Rain";
  if (meteo == 71) return "Light Snow";
  if (meteo == 73) return "Mod Snow";
  if (meteo == 75) return "Heavy Snow";
  if (meteo == 77) return "Snow Grains";
  if (meteo == 80) return "Lt Rain Shwrs";
  if (meteo == 81) return "Mod Rain Shwrs";
  if (meteo == 82) return "Violent Rain Shwrs";
  if (meteo == 85) return "Lt Snow Shwrs";
  if (meteo == 86) return "Hvy Snow Shwrs";
  if (meteo == 95) return "Thunderstorm";
  if (meteo == 96) return "T-Storm w/Hail";
  if (meteo == 99) return "T-Storm w/Hvy Hail";
  return "? ? ? ?";
}


bool drawTime(void *){
  struct tm now;
  int dw;
  char tempo[20];
  isDSTActive = now.tm_isdst > 0;  // time zone status

  //char timeStr[20];
  //String amPm = (now.tm_hour >= 12) ? "PM" : "AM";
  //int hour12 = now.tm_hour % 12;
  //if (hour12 == 0) hour12 = 12; // Convert 0 AM to 12 AM

  sprite.createSprite(178,70); 
  sprite.fillSprite(WS_BLACK);
  sprite.setTextColor(TFT_SKYBLUE);
  now = rtc.getTimeStruct();
  dw = rtc.getDayofWeek();

  sprite.loadFont(arialround20);
  sprite.setTextDatum(CL_DATUM);
  sprintf(tempo,"%s %02d %s %4d",days[dw],now.tm_mday,months[now.tm_mon],(now.tm_year+1900)); 
  sprite.drawString(tempo,0,10); 

  sprite.setTextColor(TFT_WHITE);
  sprintf(tempo,"%02d:%02d",now.tm_hour,now.tm_min); 
  sprite.loadFont(arialround54);
  sprite.drawString(tempo,0,50); 
  sprintf(tempo,":%02d",now.tm_sec); //~ add seconds
  sprite.loadFont(arialround20); //~ make seconds smaller font
  sprite.drawString(tempo,145,35); //moves seconds to the right of centre
    
  if (isDSTActive) {
      sprintf(tempo, "MDT"); // MDT = Mountain Daylight Time - use your own time zone here (daylight time) ie: PDT, CDT, etc, or use DT/DST
  } else {
      sprintf(tempo, "MST"); // MST = Mountain Standard Time - use your own time zone here (standard time) ie: PST, CST, etc or use ST
  }
  sprite.setTextColor(TFT_LIGHTGREY); 
  sprite.loadFont(arialround14);
  sprite.drawString(tempo,146,58); 

  sprite.drawLine(177, 0, 177, 70, TFT_LIGHTGREY);
  sprite.pushSprite(0,0);
  sprite.deleteSprite();

  return true;
}

void drawMeteoForecast(int meteo, float currTemp, short currHumi, float minTemp, float maxTemp, float currWind, float feelTemp, float gustWind, short cldTotal, float visib, float uvIndex, float wndDir,float dewPo, int sunriseHour, int sunriseMin,int sunsetHour,int sunsetMin,int daylightHours, int daylightMinutes, short rainProba, int isDay, int cape, int litPot) {
  char tempo[10];
  struct tm now = rtc.getTimeStruct();
  lastUpdateHour = now.tm_hour;
  lastUpdateMinute = now.tm_min;
  
  sprite.createSprite(320,170); 
  sprite.fillSprite(WS_BLACK);
  sprite.setTextColor(TFT_CYAN); 
  sprite.loadFont(arialround20); 
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(getDescFromMeteo(meteo),0,110); 

  sprite.setTextColor(TFT_LIGHTGREY);
  sprintf(tempo,"%02d:%02d  /", sunriseHour, sunriseMin);
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,0,22);

  sprite.setTextColor(TFT_LIGHTGREY);
  sprintf(tempo," %02d:%02d", sunsetHour, sunsetMin);
  sprite.setTextDatum(BL_DATUM); 
  sprite.drawString(tempo,70,22);

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround20);
  sprintf(tempo,"= %dh %0d'", daylightHours, daylightMinutes); 
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,135,22);  

        if (isDay == 1) { 
        sprite.loadFont(arialround20);
        sprite.setTextDatum(BL_DATUM);
        sprite.fillCircle (245, 11, 7, TFT_GOLD); // Little sun to show sun has risen
                }
     else {
        sprite.loadFont(arialround20);
        sprite.setTextDatum(BL_DATUM);
        sprite.fillCircle (245, 11, 7, TFT_SILVER);  // Little full moon to show sun has set
        sprite.fillCircle (249, 11, 5, WS_BLACK); //251 // add this to make the moon crescent shape
      }  
// Shows time stamp of last update
  char timeBuffer[20];
  sprintf(timeBuffer, "%02d:%02d", now.tm_hour, now.tm_min);
  sprite.loadFont(arialround14);
  sprite.setTextColor(TFT_WHITE);
  sprite.drawString(timeBuffer, 280,19); //280,20
  sprite.setTextDatum(BR_DATUM);
  sprite.drawRoundRect(278, 3, 42, 17, 3, TFT_LIGHTGREY);
  //sprite.drawRoundRect(277, 2, 44, 19, 3, TFT_LIGHTGREY);

  sprite.drawLine(0, 0, 320, 0, TFT_LIGHTGREY);
  sprite.drawLine(0, 22, 320, 22, TFT_LIGHTGREY);
  
        //display Temp
        // Extract integer and decimal to allow for smaller decimal font
      int intPart = (int)currTemp;
      int decPart = abs((int)((currTemp - intPart) * 10 + 0.5));  // Round correctly
      // Convert integer part to string
      String intString = String(intPart);
      // Ensure only one negative sign (attach it to the integer part)
      if (currTemp < 0) {
          intString = "-" + String(abs(intPart));  // Add negative sign manually
}
  intString += ".";
  sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround44);
  //sprintf(tempo,"%4.1f",currTemp);
  sprite.setTextDatum(BR_DATUM); //~~~
   //sprite.drawString(tempo,215,70); // 200,75 150,45    was 165,45 moved right make room for °C  //sprite.drawString(tempo,100,75); Changed to BR_DATUM as to keep justtifcation when only two digits.
  sprite.drawString(intString, 195, 70);
  sprite.loadFont(arialround26);
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(String(decPart),200,65); //210,65

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);
  sprintf(tempo,"°C"); // separated this from lines above to make °C in a smaller font
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,190,40); // 184,40   192,40  220,45   205,50 168,35 full right - middle of digit 165,45

  //>>>>>>>>>>>Weather conditions icon
  sprite.loadFont(weatherfont96);
  sprintf(tempo,getIconFromMeteo(meteo,isDay));
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo, 140, 120); //130,120 | 140,110

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
            else if (feelTemp < -1.00) { 
              sprite.setTextColor(TFT_WHITE);
            }
            else if (feelTemp < 10.00) { 
              sprite.setTextColor(TFT_LIGHTGREY);
            }
            else if (feelTemp < 20.00) { 
            sprite.setTextColor(TFT_DARKGREY);
            }
               else if (feelTemp >= 25.00) { 
            sprite.setTextColor(TFT_MAGENTA); 
            }
            else {
                sprite.setTextColor(TFT_DARKGREY);
            }
  sprite.loadFont(arialround26);
  sprintf(tempo, "%3.0f", feelTemp);
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(tempo, 250, 50); // 190,177  130, 170    30,160

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);
  sprintf(tempo,"°"); // separated this from lines above to make °C smaller font
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,251,39); // 191,157  185,150   185,140   168,35 full right - middle of digit 165,45

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround20);  
  sprintf(tempo,"%2d%%:RH", currHumi);
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(tempo,320,70); //sprite.drawString(tempo,60,110);

      if (currWind > 64) {   // Check the higher threshold first
      sprite.setTextColor(TFT_RED);
      }
      else if (currWind > 44) { 
        sprite.setTextColor(TFT_ORANGE);
      }
      else if (currWind > 19) {
      sprite.setTextColor(TFT_YELLOW);
      }
      else {
      sprite.setTextColor(TFT_LIGHTGREY);
      }
sprite.loadFont(arialround20);
sprintf(tempo, "%3.0f :Wind", currWind);
sprite.setTextDatum(BR_DATUM);
sprite.drawString(tempo, 320, 90);
//sprite.loadFont(arialround14); // added this to make 'kph' smaller font
//sprintf(tempo,"kph");
//sprite.drawString(tempo, 268, 110);


      if (gustWind > 69) {   // Check the higher threshold first
          sprite.setTextColor(TFT_RED);
      }
      else if (gustWind > 45) { 
        sprite.setTextColor(TFT_ORANGE);
      }
      else if (gustWind > 25) {
          sprite.setTextColor(TFT_YELLOW);
      }
      else {
          sprite.setTextColor(TFT_LIGHTGREY);
      }
sprite.loadFont(arialround20);
sprintf(tempo, "%3.0f :Gust", gustWind);
sprite.setTextDatum(BR_DATUM);
sprite.drawString(tempo, 320, 110);
//sprite.loadFont(arialround14); // added this to make 'kph' smaller font
//sprintf(tempo,"kph");
//sprite.drawString(tempo, 268, 130);

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
sprite.drawString(tempo,280,130);
sprite.setTextColor(TFT_LIGHTGREY);
sprite.loadFont(arialround20);
sprintf(tempo,":Dir");  //sprintf(tempo,"Dir: %4.1f", wndDir);
sprite.setTextDatum(BR_DATUM);
sprite.drawString(tempo,320,130); // 170,130
/*
sprite.setTextColor(TFT_LIGHTGREY);
sprite.loadFont(arialround20);  
sprintf(tempo,"%4.0f°c:Dew", dewPo);
sprite.setTextDatum(BR_DATUM);
sprite.drawString(tempo,320,170);  
*/
sprite.setTextColor(TFT_LIGHTGREY);
sprite.loadFont(arialround20);  
sprintf(tempo,"Cld: %2d%%", cldTotal);
sprite.setTextDatum(BL_DATUM);
sprite.drawString(tempo,0,70);  

sprite.setTextColor(TFT_LIGHTGREY);
sprite.loadFont(arialround20);  
sprintf(tempo,"Vis: %4.1f", visib /1000);
sprite.setTextDatum(BL_DATUM);
sprite.drawString(tempo,0,90);  
sprite.loadFont(arialround14); // added this to make 'km' smaller font
sprintf(tempo,"km");
sprite.drawString(tempo, 85, 90);

///////////////////////////////////

    if (rainProba > 69) {  
      sprite.setTextColor(TFT_GREENYELLOW);
    }
    else {
      sprite.setTextColor(TFT_LIGHTGREY);
    }
  sprite.loadFont(arialround20);  
  sprintf(tempo,"PoP:%3d%%",rainProba);
  //sprite.setTextDatum(CR_DATUM);
  //sprite.drawString(tempo,125,20);  
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,0,130); 
/*
  sprite.setTextColor(TFT_DARKGREY);
  sprite.loadFont(arialround14);  
  sprintf(tempo,"Lpot:%3dj",litPot);
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,190,35); 
*/

  if(cape > 999) { // 3500
    sprite.setTextColor(TFT_RED);
    sprite.loadFont(weatherfont60);
    sprintf(tempo,"A");
    sprite.setTextDatum(BR_DATUM);
    sprite.drawString(tempo,126,54); // 120,56
  }
  else if (cape > 499) { // 2500
    sprite.setTextColor(TFT_ORANGE);
    sprite.loadFont(weatherfont60);
    sprintf(tempo,"A");
    sprite.setTextDatum(BR_DATUM);
    sprite.drawString(tempo,126,54);
  }
  else if (cape > 399) { // 1000
    sprite.setTextColor(TFT_YELLOW);
    sprite.loadFont(weatherfont60);
    sprintf(tempo,"A");
    sprite.setTextDatum(BR_DATUM);
    sprite.drawString(tempo,126,54);
  }
  else if (cape > 199) {
    sprite.setTextColor(TFT_GREENYELLOW);
    sprite.loadFont(weatherfont60);
    sprintf(tempo,"A");
    sprite.setTextDatum(BR_DATUM);
    sprite.drawString(tempo,126,54);
  }
  else if (cape > 99) {
    sprite.setTextColor(TFT_WHITE);
    //sprite.loadFont(weatherfont60);
    //sprintf(tempo,"A");
    //sprite.setTextDatum(BR_DATUM);
    //sprite.drawString(tempo,120,56);
  }
  else {
    sprite.setTextColor(TFT_LIGHTGREY);
    //sprite.loadFont(weatherfont60);
    //sprintf(tempo,"A");
    //sprite.setTextDatum(BR_DATUM);
    //sprite.drawString(tempo,120,56);
  }
  sprite.loadFont(arialround20);  
  sprintf(tempo,"%4d :PE",cape); // Convective Available Potential Energy
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(tempo,320,150); //320,147

      if (uvIndex >= 11) {   // Check the higher threshold first
        sprite.setTextColor(TFT_PURPLE);
        //sprite.loadFont(arialround20);  
        //sprintf(tempo,"XTRM");
      }
      else if (uvIndex >= 8) {  //??
        sprite.setTextColor(TFT_RED);
        //sprite.loadFont(arialround20);  
        //sprintf(tempo,"VHigh");
      }
      else if (uvIndex >= 6) { //??
        sprite.setTextColor(TFT_ORANGE);
        //sprite.loadFont(arialround20);  
        //sprintf(tempo,"High");
      }
      else if (uvIndex >= 3) {  //??
        sprite.setTextColor(TFT_GREENYELLOW);
        //sprite.loadFont(arialround20);  
        //sprintf(tempo,"Mod");
      }
      else {
        sprite.setTextColor(TFT_DARKGREEN);
        //sprite.loadFont(arialround20);  
        //sprintf(tempo,"Low");
}
  //sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround20);  
  sprintf(tempo,"UV: %2.1f",uvIndex);
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,0,150);

      if (minTemp <= -15) {   // If min forecast temp is lower than or = -15 number is blue
        sprite.setTextColor(TFT_SKYBLUE);
      }
    else {
        sprite.setTextColor(TFT_LIGHTGREY);
      }
  //sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround20);  
  sprintf(tempo,"%2.0f°",minTemp);
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(tempo,53,46); //53,60

   if (maxTemp >= 27) {   // If max forecast temp is higher than or = 27 number is magenta
        sprite.setTextColor(TFT_MAGENTA);
      }
    else {
        sprite.setTextColor(TFT_LIGHTGREY);
      }
  //sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround20);  
  sprintf(tempo,"%2.0f°",maxTemp);
  sprite.setTextDatum(BR_DATUM);
  sprite.drawString(tempo,316,46); //316,60
  sprite.drawRoundRect(0, 24, 56, 22, 5, TFT_SKYBLUE); // little box around daily low temp
  sprite.drawRoundRect(263, 24, 56, 22, 5, TFT_MAGENTA); // little box around daily high temp
  //drawRoundRect( left/right = x,  up/down = y, width, height, round, colour),
  sprite.pushSprite(0,70);//150,50
  sprite.deleteSprite();

}
/////////////////
