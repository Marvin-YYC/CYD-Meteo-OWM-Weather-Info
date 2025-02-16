
# CYD-Meteo-Weather-Info
**CYD (Cheap Yellow Display)** Meteo Weather Information Aggregator
Original code and assistance from  https://github.com/marcosjl31 with many thanks
https://github.com/marcosjl31/WeatherStation

This particular PIO code is set up to run on **CYD** ESP32-**2432s028** (the one with the single micro usb)
I believe  it will run on other CYD's with some configuration  changes

This is a work in progress, any help or suggestions will be greatly appreciated.  
I am not using the weather icons in my code, as they used too much realestate for my application.  I left them in the sketch in the event I figure out how to use them, or if someone else wants to use them.  I have also added a weather icon font for future use.  The weather icon (from the font) in the top right corner is just for show at the moment.  I am also hoping to add AQHI data once I figure that out.  :) 

## Details
- USB Powered
- Deep sleep mode - ESP deep sleep mode shuts off processor and screen at night.
- Weather Data from Meteo
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
- NEW Feb 13/25 - Local Aurora Borealis data thanks to University of Alberta, Canada.  https://www.aurorawatch.ca  Probability of auroral displays occurring (PoAB) in % above 50% text goes yellow above 70% red.  Website data is updated hourly.
- NEW Feb 16/25 - AQI data from https://aqicn.org - You will need to obtain your own free API token here> https://aqicn.org/data-platform/token/

- 
![signal-2025-02-14-200904](https://github.com/user-attachments/assets/cbea3f9b-5333-438d-86f3-41a7fc57fca8)
![signal-2025-02-10-110216_002](https://github.com/user-attachments/assets/9a12e640-1086-4056-9009-626a194f2bd7)
![signal-2025-01-28-112736_002](https://github.com/user-attachments/assets/a062e056-a35f-4237-b371-10a3463e1ece)
![signal-2025-02-04-144343_004](https://github.com/user-attachments/assets/b4d51d56-6036-4f6c-97fc-86047ec1f44b)
![signal-2025-02-04-144343_002](https://github.com/user-attachments/assets/457ea7ab-623a-4fe4-96aa-cf23f1578aa7)
![CYD-2025-01-05-195444_002](https://github.com/user-attachments/assets/53f8c39c-2bd0-4377-be2f-cc53f758b670)



