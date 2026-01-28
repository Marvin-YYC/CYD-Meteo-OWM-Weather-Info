#include <Arduino.h>
#include "WeatherStation.h"
#include <arduino-timer.h>

bool isDSTActive = false;  // Global variable to store DST status
bool getAuroraData(void*);  
void drawAurora(float auroraValue); 
void drawAQI(int aqiValue, int stationIndex);  // AQI Multiple station
bool getaqiData(void*);
bool getOWMData(void*);
void drawOWMValue();
void drawOWMValue(String owmIcon,int owmCode,String owmDesc,String owmCond,float owmTemp,int owmHumi,float owmWind,float owmFeelTemp,float owmGust,int owmCldCvr,int owmVisib,float owmWndDir,const char *lastUpdate);
bool getTime(void*);
bool getMeteoForecast(void*);
int lastUpdateHour = -1;
int lastUpdateMinute = -1;
int lastAuroraHour12 = -1;  // Last hour we ran at :12
int lastAuroraHour16 = -1;  // Last hour we ran at :16

bool isFetching = false;
bool jobTimeSync = false;
bool jobMeteo = false;
bool jobAurora = false;
bool jobAQI = false;
bool jobOWM = false;

// --- Time sync control ---
unsigned long lastTimeSync = 0;                    // timestamp of last sync
const unsigned long timeSyncInterval = 30 * 60 * 1000; // 30 minutes (in ms)

//bool getaqiData(void*); // AQI Single station
//void drawAQI(int aqiValue); // AQI Single station

// Job flags (set by scheduler, consumed in loop)
//volatile bool jobTimeSync = false;
//volatile bool jobMeteo    = false;
//volatile bool jobAurora   = false;
//volatile bool jobAQI      = false;
//volatile bool jobOWM      = false;
//volatile bool isFetching  = false;  // Prevent running >1 network job at once
unsigned long lastCheckMillis = 0;  // For the schedule checker (millis-based)
const unsigned long CHECK_EVERY_MS = 15000; // check every 15s
// “Run-once per window” guards
int lastTimeSyncQuarter = -1;    // 0,1,2,3 for :00,:15,:30,:45
int lastMeteoKey        = -1;    // hour*100 + bucket(0/20/40)
int lastAuroraHour      = -1;    // once per hour
int lastAQIKey          = -1;    // hour*100 + minute(5/25/45)
int lastOWMKey          = -1;    // hour*100 + minute(10/30/50)

bool ensureWiFiConnected(const char* tag) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("%s WiFi disconnected. Reconnecting...\n", tag);
    WiFi.reconnect();

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
      delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.printf("%s WiFi reconnect failed.\n", tag);
      return false;
    }
  }
  return true;
}

void scheduleJobsByClock() {
  unsigned long nowMs = millis();
  if (nowMs - lastCheckMillis < CHECK_EVERY_MS) return;
  lastCheckMillis = nowMs;

  struct tm now;
  if (!getLocalTime(&now)) {
    Serial.println("Failed to get time");
    return;
  }
  int hr = now.tm_hour;
  int mn = now.tm_min;
  bool timeSyncWindow = ( (mn >= 14 && mn < 15) || (mn >= 44 && mn < 45) );
  int halfHourKey = hr * 2 + (mn >= 30 ? 1 : 0);  // Unique key per half-hour

  if (timeSyncWindow) {
    if (halfHourKey != lastTimeSyncQuarter) {
      lastTimeSyncQuarter = halfHourKey;
      jobTimeSync = true;
      Serial.println("[JOB] Time sync scheduled");
    }
  }

 /*
// ----- Time sync @ :00, :15, :30, :45 (1-minute windows: [M, M+1))
  int quarter = (mn / 15); // 0..3
  bool timeSyncWindow = ( (mn >= 0 && mn < 1) || (mn >= 15 && mn < 16) ||
                          (mn >= 30 && mn < 31) || (mn >= 45 && mn < 46) );
  if (timeSyncWindow) {
    int key = hr * 10 + quarter;          // unique per hour & quarter
    if (key != lastTimeSyncQuarter) {
      lastTimeSyncQuarter = key;
      jobTimeSync = true;
      Serial.println("[JOB] Time sync scheduled");
    }
  }
*/
  
  // ----- Meteo @ :00, :20, :40 (windows [M, M+1))
  int meteoBucket = -1;
  if (mn >= 0 && mn < 1)   meteoBucket = 0;
  else if (mn >= 20 && mn < 21) meteoBucket = 20;
  else if (mn >= 40 && mn < 41) meteoBucket = 40;
  //if (mn >= 10 && mn < 11)   meteoBucket = 10;
  //else if (mn >= 30 && mn < 31) meteoBucket = 30;
  //else if (mn >= 50 && mn < 51) meteoBucket = 50;
  if (meteoBucket >= 0) {
    int key = hr * 100 + meteoBucket;
    if (key != lastMeteoKey) {
      lastMeteoKey = key;
      jobMeteo = true;
      Serial.println("[JOB] Meteo scheduled");
    }
  }
  if (mn >= 12 && mn < 13) {
    if (hr != lastAuroraHour12) {
      lastAuroraHour12 = hr;
      jobAurora = true;
      Serial.println("[JOB] Aurora scheduled at :12");
    }
  }
  int aqiMinute = -1;
  if (mn >= 2 && mn < 3)     aqiMinute = 2;
  else if (mn >= 22 && mn < 23) aqiMinute = 22;
  else if (mn >= 42 && mn < 43) aqiMinute = 42;
  if (aqiMinute >= 0) {
    int key = hr * 100 + aqiMinute;
    if (key != lastAQIKey) {
      lastAQIKey = key;
      jobAQI = true;
      Serial.println("[JOB] AQI scheduled");
    }
  }
  // ----- OWM @ :01, :21, :41 (windows [M, M+1))
  int owmMinute = -1;
  if (mn >= 1 && mn < 2)      owmMinute = 1;
  else if (mn >= 21 && mn < 22) owmMinute = 21;
  else if (mn >= 41 && mn < 42) owmMinute = 41;
  if (owmMinute >= 0) {
    int key = hr * 100 + owmMinute;
    if (key != lastOWMKey) {
      lastOWMKey = key;
      jobOWM = true;
      Serial.println("[JOB] OWM scheduled");
    }
  }
}

void processJobsOnce() {
  if (isFetching) return;  // Don’t start a new job if one is running

  // --- Time Sync Job ---
  if (jobTimeSync) {
    jobTimeSync = false;

    // Skip if recently synced
    if (millis() - lastTimeSync < timeSyncInterval) {
      Serial.println("[SKIP] Time sync (recently done)");
    } else {
      isFetching = true;
      Serial.println("[RUN] Time sync");
      getTime(nullptr);
      lastTimeSync = millis();  // record successful sync time
      isFetching = false;
    }
    return;
  }

  // --- Meteo (Weather) Job ---
  if (jobMeteo) {
    jobMeteo = false;
    isFetching = true;
    Serial.println("[RUN] Meteo");

    // Pass a flag so it can decide whether to call getTime internally
    bool allowTimeSync = (millis() - lastTimeSync > timeSyncInterval);
    getMeteoForecast((void*)allowTimeSync);
  // modified function below
    // If meteo also updated the time, record it
    if (allowTimeSync) lastTimeSync = millis();
    isFetching = false;
    return;
  }
  // --- Aurora Job ---
  if (jobAurora) {
    jobAurora = false;
    isFetching = true;
    Serial.println("[RUN] Aurora");
    getAuroraData(nullptr);
    isFetching = false;
    return;
  }
  // --- AQI Job ---
  if (jobAQI) {
    jobAQI = false;
    isFetching = true;
    Serial.println("[RUN] AQI");
    getaqiData(nullptr);
    isFetching = false;
    return;
  }
  // --- OWM Job ---
  if (jobOWM) {
    jobOWM = false;
    isFetching = true;
    Serial.println("[RUN] OWM");
    getOWMData(nullptr);
    isFetching = false;
    return;
  }
}
bool getMeteoForecast(bool allowTimeSync) {
  if (allowTimeSync) {
    Serial.println("[INFO] Meteo requested time sync (allowed)");
    getTime(nullptr);
  } else {
    Serial.println("[INFO] Meteo skipped time sync (recently done)");
  }
  // ... your existing meteo data fetching code here ...
  return true;
}

#define TFT_BL 21
#define BACKLIGHT_PIN 21
//--- Time object
auto timer = timer_create_default();
////
void showStartupScreen() {
    tft.fillScreen(TFT_BLACK);  // Background color
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM); // Middle center
    tft.loadFont(weatherfont80);
    tft.drawString("C L N K M", tft.width() / 2, tft.height() / 2 - 90);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.loadFont(arialround20); // Big font for title
    tft.setTextDatum(MC_DATUM); // Middle center
    tft.drawString("THIRD PLANET", tft.width() / 2, tft.height() / 2 - 40);
    tft.drawString("Weather Information Aggregator", tft.width() / 2, tft.height() / 2 - 20);
    tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
    tft.loadFont(arialround14); // Big font for title
    tft.drawString("Open-Meteo Weather, Open Weather Map", tft.width() / 2, tft.height() / 2 + 0);
    tft.drawString("UofA- AuroraWatch.ca, AQI- aqicn.org", tft.width() / 2, tft.height() / 2 + 20);
    tft.unloadFont();
    tft.loadFont(arialround14); // Smaller font for subtitle
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Loading ......", tft.width() / 2, tft.height() / 2 + 90);

    delay(2000); // Show splash screen for 2 seconds
}
////
void setup() {
  Serial.begin(115200);
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);  // Adjust as needed
  showStartupScreen(); // Display the splash screen first

  // (Optional) kick off an immediate first run:
  jobTimeSync = true;   // get correct time ASAP
  jobMeteo    = true;    // show something on boot
  jobAQI      = true;
  jobOWM      = true;
  jobAurora   = true;    // you can omit if you only want it at :12
  bool getAuroraData(void*);
  bool getaqiData(void*);

  while (!Serial) {}; 
  Serial.print("WeatherStation");
  Serial.println(VW_Version);
  
    delay(1000);

  //--- WiFi connection using Captive_AP.
  wifiManager.setConfigPortalTimeout(2000); //  3 seconds max
  IPAddress _ip,_gw,_mask,_dns;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _mask.fromString(static_mask);
  _dns.fromString(static_dns);

  wifiManager.setHostname(staHostname);
  wifiManager.setSTAStaticIPConfig(_ip,_gw,_mask,_dns);

  if(!wifiManager.autoConnect("Access_Weather","12345678")) { //this is the ID and Password for the Wifi Manager (PW must be 8 characters)
    Serial.println("Failed to connect and hit timeout");
    delay(2000); // 3 seconds max
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
  timer.every(500,drawTime);                // Every 500ms, display time
}

void loop() {
  server.handleClient();
  timer.tick();
  struct tm now = rtc.getTimeStruct();
  scheduleJobsByClock();
  processJobsOnce();
/*
// Enter deep sleep - SHORT SLEEP TEST
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
          delay(300);
          if (WiFi.status() != WL_CONNECTED) {
              Serial.println("Failed to reconnect.");
              return true;  // Keep the timer running
          }
      }

    HTTPClient http;
    http.setTimeout(5000); 
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

const int numStations = sizeof(aqicnURLs) / sizeof(aqicnURLs[2]);  // Number of stations ZERO if only one, ONE if two stations
bool getaqiData(void *) {
    Serial.println("Fetching AQI Data...");

    if (WiFi.status() != WL_CONNECTED) {  
        Serial.println("WiFi not connected, trying to reconnect...");
        WiFi.disconnect();
        WiFi.reconnect();
        delay(900);  // not less than 500ms
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Failed to reconnect.");
            return true;  
        }
    }

    for (int i = 0; i < numStations; i++) {  
        HTTPClient http;
        http.setTimeout(5000);  
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
  sprite.createSprite(220, 20); // 220,20
  sprite.fillSprite(WS_BLACK);  // WS_BLACK

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
  sprite.loadFont(arialround26); 
  sprite.setTextDatum(BR_DATUM);
  sprite.setCursor(0, 1);  // 15,3
  sprintf(tempo,"%d",aqiValue); 
  sprite.print(tempo);
  //int xPos = 140 + (stationIndex * 60);  // 201 Right side position Spacing: 41 pixels apart   //int xPos = 200 + (stationIndex * 40); 
  int xPos = 172 + (stationIndex * 52);
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
  //timestamp
  long owmTime = 0;
  long tzOffset = 0;
  char lastUpdate[20]; // buffer for formatted time string

  
  // get OWM Data
  http.begin(owmServerURL);
  httpResponseCode = http.GET();
  
  if (httpResponseCode > 0){
    payload = http.getString();
    Serial.println(httpResponseCode);  
    Serial.println(payload);
    delay(5000);
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
              // ⏰ Extract time info
              owmTime   = jsonDoc["dt"] | 0;         // UTC timestamp
              tzOffset  = jsonDoc["timezone"] | 0;   // seconds offset from UTC
              time_t localTime = owmTime + tzOffset;
              struct tm *timeinfo = gmtime(&localTime);
              strftime(lastUpdate, sizeof(lastUpdate), "%H:%M", timeinfo); //strftime(lastUpdate, sizeof(lastUpdate), "%H:%M %d-%m", timeinfo);
              
              Serial.print("Last Update: ");
              Serial.println(lastUpdate);

        if (owmIcon.endsWith("d")) {
          Serial.println("Day icon detected ☀️");
        } else if (owmIcon.endsWith("n")) {
          Serial.println("Night icon detected 🌙");
            }
        delay(500);        
    http.end();

    // display forecast  owmDesc
    drawOWMValue(owmIcon,owmCode,owmDesc,owmCond,owmTemp,owmHumi,owmWind,owmFeelTemp,owmGust,owmCldCvr,owmVisib,owmWndDir,lastUpdate);
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
  if (owmCode == 500 || owmCode == 501 || owmCode == 520 || owmCode == 521) { sprite.setTextColor(TFT_SKYBLUE); return "M"; }  // Light + Moderate rain
  if (owmCode == 502 || owmCode == 503 || owmCode == 504 || owmCode == 522) { sprite.setTextColor(TFT_SKYBLUE); return "B"; }  // Heavy Rain
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
  void drawOWMValue(String owmIcon,int owmCode,String owmDesc,String owmCond,float owmTemp,int owmHumi,float owmWind,float owmFeelTemp,float owmGust,int owmCldCvr,int owmVisib,float owmWndDir,const char *lastUpdate) {
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
  sprite.loadFont(arialround09); 
  sprintf(tempo,"%2d%%",owmCldCvr); // shows cloud cover three digit w/ % 
  //sprintf(tempo,"%2.0f",owmCldCvr * 0.1); // converted cloud cover two digit no % to make smaller, 100% = 10 80% = 8, etc.   original - sprintf(tempo,"%2d",owmCldCvr);
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

  sprite.setTextColor(TFT_WHITE);
  sprite.loadFont(arialround09); 
  sprite.drawString(String("") + lastUpdate, 2, 6); // time stamp of last OWM update, not the time of fetch
  //sprintf(tempo, "%s", owmCond.c_str()); // short description
  sprite.setTextDatum(CL_DATUM);
  //sprite.drawString(tempo,2,6);
  //sprite.drawString(tempo,4,52); //4,38

// long description
  sprite.setTextColor(TFT_CYAN); 
  sprite.loadFont(arialround14); 
  sprite.setTextDatum(CL_DATUM);
  String desc1 = owmDesc.substring(0, 16);  // Adjust these lengths per font size //0,18
  String desc2 = owmDesc.length() > 16 ? owmDesc.substring(16, 43) : ""; // >18  owmDesc.substring(18, 35) : "";
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
  http.setTimeout(5000);  // Set timeout to avoid freezing
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

// --- Get time and DST info from API ---
bool getTime(void *) {
  HTTPClient http;
  int httpResponseCode;
  JsonDocument jsonDoc;
  String payload;
  DeserializationError error;
  const char *datetime;
  DateTime dt;

  http.begin(timeServer);
  httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    payload = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(payload);
  } else {
    Serial.print("ERROR: bad HTTP request: ");
    Serial.println(httpResponseCode);
    http.end();
    delay(500);
    return true;
  }

  error = deserializeJson(jsonDoc, payload);
  if (!error) {

    // time.now field name
    datetime = jsonDoc["datetime"];
    dt = parseISO8601(String(datetime));

    rtc.setTime(
      dt.second,
      dt.minute,
      dt.hour,
      dt.day,
      dt.month,
      dt.year
    );

    // time.now DST flag
    isDSTActive = jsonDoc["dst"];
    Serial.print("DST Active: ");
    Serial.println(isDSTActive ? "true" : "false");
  } else {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
  }

  http.end();
  return true;
}

bool getMeteoForecast(void *) {
    // Optional: sync time before fetching meteo
  getTime(nullptr);   // ✅ only runs once per meteo update
    // --- Your current meteo fetching logic here ---
  Serial.println("Fetching Meteo data...");

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
  if (meteo == 65) return (sprite.setTextColor(TFT_SKYBLUE), "B");  // Hvy Rain
  if (meteo == 66) return (sprite.setTextColor(TFT_GREENYELLOW), "M");  // Lt Frzng Rain
  if (meteo == 67) return (sprite.setTextColor(TFT_GREENYELLOW), "M");  // Hvy Frzng Rain
  if (meteo == 71) return (sprite.setTextColor(TFT_WHITE),"L");  // Light Snow
  if (meteo == 73) return (sprite.setTextColor(TFT_WHITE),"L");  // Mod Snow
  if (meteo == 75) return (sprite.setTextColor(TFT_WHITE),"D");  // Heavy Snow
  if (meteo == 77) return (sprite.setTextColor(TFT_WHITE),"L");  // Snow Grains
  if (meteo == 80 ) return (sprite.setTextColor(TFT_SKYBLUE), "M");  // Lt Rain Shwrs
  if (meteo == 81 ) return (sprite.setTextColor(TFT_SKYBLUE), "M");  // Mod Rain Shwrs
  if (meteo == 82 ) return (sprite.setTextColor(TFT_GREENYELLOW), "B");  // Violent Rain Shwrs
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
  if (meteo == 51) return "Lt Drizzle";
  if (meteo == 53) return "Mod Drizzle";
  if (meteo == 55) return "Hvy Drizzle";
  if (meteo == 56) return "Lt Frz Drizzle";
  if (meteo == 57) return "Hvy Frz Drzle";
  if (meteo == 61) return "Light Rain";
  if (meteo == 63) return "Mod Rain";
  if (meteo == 65) return "Heavy Rain";
  if (meteo == 66) return "Lt Frzg Rain";
  if (meteo == 67) return "Hvy Frzg Rain";
  if (meteo == 71) return "Light Snow";
  if (meteo == 73) return "Mod Snow";
  if (meteo == 75) return "Heavy Snow";
  if (meteo == 77) return "Snow Grains";
  if (meteo == 80) return "Lt Rain Shwr";
  if (meteo == 81) return "Mod Rain Shwr";
  if (meteo == 82) return "Vlnt Rain Shwr";
  if (meteo == 85) return "Lt Snow Shwr";
  if (meteo == 86) return "Hvy Snow Shwr";
  if (meteo == 95) return "Thunderstorm";
  if (meteo == 96) return "T-Storm Hail";
  if (meteo == 99) return "T-Stm Hvy Hail";
  return "? ? ? ?";
}

// --- Draw Time on TFT ---
bool drawTime(void *) {
  struct tm now;
  int dw;
  char tempo[20];

  now = rtc.getTimeStruct();
  dw = rtc.getDayofWeek();

  sprite.createSprite(178, 70); 
  sprite.fillSprite(WS_BLACK);
  sprite.setTextColor(TFT_SKYBLUE);

  sprite.loadFont(arialround20);
  sprite.setTextDatum(CL_DATUM);
  sprintf(tempo, "%s %02d %s %4d", days[dw], now.tm_mday, months[now.tm_mon], (now.tm_year + 1900)); 
  sprite.drawString(tempo, 0, 10);

  sprite.setTextColor(TFT_WHITE);
  sprintf(tempo, "%02d:%02d", now.tm_hour, now.tm_min); 
  sprite.loadFont(arialround54);
  sprite.drawString(tempo, 0, 50);

  sprintf(tempo, ":%02d", now.tm_sec);
  sprite.loadFont(arialround20);
  sprite.drawString(tempo, 145, 35);

  // ✅ DST display based on API info
  if (isDSTActive) {
    sprintf(tempo, "MDT"); // Daylight Time // MDT = Mountain Daylight Time - use your own time zone here (daylight time) ie: PDT, CDT, etc, or use DT
  } else {
    sprintf(tempo, "MST"); // Standard Time // MST = Mountain Standard Time - use your own time zone here (standard time) ie: PST, CST, etc or use ST
  }

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);
  sprite.drawString(tempo, 146, 58);

  sprite.drawLine(177, 0, 177, 70, TFT_LIGHTGREY);
  sprite.pushSprite(0, 0);
  sprite.deleteSprite();

  return true;
}

void drawMeteoForecast(int meteo, float currTemp, short currHumi, float minTemp, float maxTemp, float currWind, float feelTemp, float gustWind, short cldTotal, float visib, float uvIndex, float wndDir,float dewPo, int sunriseHour, int sunriseMin,int sunsetHour,int sunsetMin,int daylightHours, int daylightMinutes, short rainProba, int isDay, int cape, int litPot) {
  char tempo[10];
  struct tm now = rtc.getTimeStruct();
  lastUpdateHour = now.tm_hour;
  lastUpdateMinute = now.tm_min;
  
  sprite.createSprite(320,150); // (320,170); // CHANGED SIZE OF SPRITE SO IT UPDATES BETTER
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
  //sprite.setTextColor(TFT_LIGHTGREY);
  //sprite.loadFont(arialround14);
  //sprintf(tempo,"[%d]", isDay); 
  //sprite.setTextDatum(BL_DATUM);
  //sprite.drawString(tempo,240,22);  

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
/*
      if (currTemp > 33.00) { 
          sprite.setTextColor(TFT_RED);
          sprite.loadFont(weatherfont60);
          sprintf(tempo,"H");
          sprite.setTextDatum(BR_DATUM);
          sprite.drawString(tempo,235,54);
          }
        else if (currTemp > 28.00) { 
          sprite.setTextColor(TFT_YELLOW);
          sprite.loadFont(weatherfont60);
          sprintf(tempo,"H");
          sprite.setTextDatum(BR_DATUM);
          sprite.drawString(tempo,235,54);
          } 
        else if (currTemp > 19.5) { 
          sprite.setTextColor(TFT_DARKGREEN);
          sprite.loadFont(weatherfont60);
          sprintf(tempo,"H");
          sprite.setTextDatum(BR_DATUM);
          sprite.drawString(tempo,235,54);
          }
        else if (currTemp < -14.9) { 
          sprite.setTextColor(TFT_BLUE);
          sprite.loadFont(weatherfont60);
          sprintf(tempo,"E");
          sprite.setTextDatum(BR_DATUM);
          sprite.drawString(tempo,235,54);
          }
        else{
          sprite.setTextColor(TFT_BLACK);
          }
*/
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
  sprite.drawString(tempo, 250, 50); //<<
  //sprite.drawString(tempo, 172, 177); // 182,177

  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.loadFont(arialround14);
  sprintf(tempo,"°"); // separated this from lines above to make °C smaller font
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,251,39);
  //sprite.drawString(tempo,173,157); // 183,157  168,35 full right - middle of digit 165,45

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

  if(cape >= 3000) { // 3500
    sprite.setTextColor(TFT_RED);
    sprite.loadFont(weatherfont60);
    sprintf(tempo,"A");
    sprite.setTextDatum(BR_DATUM);
    sprite.drawString(tempo,126,54); // 120,56
  }
  else if (cape >= 2000) { // 2500
    sprite.setTextColor(TFT_ORANGE);
    sprite.loadFont(weatherfont60);
    sprintf(tempo,"A");
    sprite.setTextDatum(BR_DATUM);
    sprite.drawString(tempo,126,54);
  }
  else if (cape >= 800) { // 1000
    sprite.setTextColor(TFT_YELLOW);
    sprite.loadFont(weatherfont60);
    sprintf(tempo,"A");
    sprite.setTextDatum(BR_DATUM);
    sprite.drawString(tempo,126,54);
  }
  else if (cape >= 500) {
    sprite.setTextColor(TFT_GREENYELLOW);
    sprite.loadFont(weatherfont60);
    sprintf(tempo,"A");
    sprite.setTextDatum(BR_DATUM);
    sprite.drawString(tempo,126,54);
  }
  else if (cape >= 200) {
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
  sprintf(tempo,"UV: %2.1f",uvIndex); /// meteo = weather interp code
  sprite.setTextDatum(BL_DATUM);
  sprite.drawString(tempo,0,150);
//
sprite.setTextColor(TFT_LIGHTGREY);
sprite.loadFont(arialround09);  
sprintf(tempo,"cde:%2d", meteo); // this shows weather code for diagnostics
sprite.setTextDatum(BL_DATUM);
sprite.drawString(tempo,80,150);  
//
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

