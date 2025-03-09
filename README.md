
# CYD-Enviromental-Information-Aggregator
**CYD (Cheap Yellow Display)** 
Collects data from Meteo Weather, Open Weather Map, Aurora Watch, World Air Quality Index project
Original code and assistance from https://github.com/marcosjl31 with many thanks
https://github.com/marcosjl31/WeatherStation

This particular PIO code is set up to run on **CYD** ESP32-**2432s028** (the one with the single micro usb)
I believe it will run on other CYD's with some configuration  changes

## Details
- USB Powered
- Deep sleep mode - ESP deep sleep mode shuts off processor and screen at night.
- Weather Data from Meteo
- Weather Data from Open Weather Map - you will need your own API key
- 24hr Clock with seconds
- Sunrise/set time, length of day in hrs+min.
- Condtions.
- Current temperature in °C
- Daily Hi/Lo temp in °C
- Current Wind chill/Feel like temp in °C temp, colour changes depending on conditions as per ECC standard
- Current Relative Humidity
- Current Sustained Wind speed and Wind gust in kph, colour changes if wind conditions are above normal (set parameters manually)
- Current Wind direction compass/intercardinal points
- Dewpoint in °C - Cloud cover in % - Visibility in km
- Possibility of Precipitation in %, over 75% text goes greenyellow
- UV index in coloured warnings as per international standard, green = low 1-3 / Purple = Extreme 11+.
- NEW Feb 13/25 - Local Aurora Borealis data with thanks to University of Alberta, Canada.  https://www.aurorawatch.ca  Probability of auroral displays occurring (PoAB) in % above 50% text goes yellow above 70% red.  Website data is updated hourly 11 minutes past the hour.
- NEW Feb 16/25 - AQI data from https://aqicn.org - You will need to obtain your own free API token here> https://aqicn.org/data-platform/token/
- NEW Mar 9/25 - Added Open Weather Map information in top right corner.  - you will need your own API key 


![signal-2025-03-09-154124](https://github.com/user-attachments/assets/9ca6cc61-ab27-4811-96f1-fe3ff406c281)
Yup, it's windy here today :)
![signal-2025-02-14-200904](https://github.com/user-attachments/assets/cbea3f9b-5333-438d-86f3-41a7fc57fca8)
![signal-2025-02-10-110216_002](https://github.com/user-attachments/assets/9a12e640-1086-4056-9009-626a194f2bd7)
![signal-2025-01-28-112736_002](https://github.com/user-attachments/assets/a062e056-a35f-4237-b371-10a3463e1ece)

![signal-2025-02-04-144343_002](https://github.com/user-attachments/assets/457ea7ab-623a-4fe4-96aa-cf23f1578aa7)
![CYD-2025-01-05-195444_002](https://github.com/user-attachments/assets/53f8c39c-2bd0-4377-be2f-cc53f758b670)
Original code




