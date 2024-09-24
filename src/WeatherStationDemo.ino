/**The MIT License (MIT)

Copyright (c) 2018 by Daniel Eichhorn - ThingPulse

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at https://thingpulse.com
*/

#include <Arduino.h>
#include <config.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <coredecls.h> // settimeofday_cb()
#else
#include <WiFi.h>
#endif
#include <ESPHTTPClient.h>
#include <JsonListener.h>

// time
#include <time.h>     // time() ctime()
#include <sys/time.h> // struct timeval

#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"

#include <WiFiManager.h>

#include <otadrive_esp.h>

#include <AirplanesLiveClient.h>

#define FW_VER "v@1.1.2"

/***************************
 * Begin Settings
 **************************/

#define TZ 1      // (utc+) TZ in hours
#define DST_MN 60 // use 60mn for summer time in some countries

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D2;
const int SDC_PIN = D1;
#else
const int SDA_PIN = 5; // D3;
const int SDC_PIN = 4; // D4;
#endif
// OpenWeatherMap Settings
// Sign up here to get an API key:
// https://docs.thingpulse.com/how-tos/openweathermap-key/
/*
Go to https://openweathermap.org/find?q= and search for a location. Go through the
result set and select the entry closest to the actual location you want to display
data for. It'll be a URL like https://openweathermap.org/city/2657896. The number
at the end is what you assign to the constant below.
 */

// Pick a language code from this list:
// Arabic - ar, Bulgarian - bg, Catalan - ca, Czech - cz, German - de, Greek - el,
// English - en, Persian (Farsi) - fa, Finnish - fi, French - fr, Galician - gl,
// Croatian - hr, Hungarian - hu, Italian - it, Japanese - ja, Korean - kr,
// Latvian - la, Lithuanian - lt, Macedonian - mk, Dutch - nl, Polish - pl,
// Portuguese - pt, Romanian - ro, Russian - ru, Swedish - se, Slovak - sk,
// Slovenian - sl, Spanish - es, Turkish - tr, Ukrainian - ua, Vietnamese - vi,
// Chinese Simplified - zh_cn, Chinese Traditional - zh_tw.

const uint8_t MAX_FORECASTS = 4;

const boolean IS_METRIC = true;

const boolean DISABLE_OTA = false;

// Adjust according to your language
const String WDAY_NAMES[] = {"NIE", "PON", "WTO", "SRO", "CZW", "PIA", "SOB"};
const String MONTH_NAMES[] = {"STY", "LUT", "MAR", "KWI", "MAJ", "CZE", "LIP", "SIE", "WRZ", "PAŹ", "LIS", "GRU"};

/***************************
 * End Settings
 **************************/
// Initialize the oled display for address 0x3c
// sda-pin=14 and sdc-pin=12
SSD1306Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi ui(&display);

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapCurrent currentWeatherClient;

OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast forecastClient;

AirplanesLiveClient airplanesClient;

#define TZ_MN ((TZ) * 60)
#define TZ_SEC ((TZ) * 3600)
#define DST_SEC ((DST_MN) * 60)

const float pi = 3.141;

time_t now;

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

bool readyForAirplaneUpdate = false;

long timeSinceLastWUpdate = 0;

long timeSinceLastAUpdate = 0;

// declaring prototypes
void configModeCallback(WiFiManager *myWiFiManager);
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawMessage(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);
void drawAirplane1(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawAirplane2(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void autoBrightness(OLEDDisplay *display);
void drawHeading(OLEDDisplay *display, int x, int y, double heading, int radius);
String replaceChars(String input);
void otaUpdate();

// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = {drawDateTime, drawCurrentWeather, drawForecast, drawMessage};
FrameCallback framesAirplane[] = {drawDateTime, drawAirplane1, drawAirplane2, drawCurrentWeather, drawForecast};

OverlayCallback overlays[] = {drawHeaderOverlay};
int numberOfOverlays = 1;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  // display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // Uncomment for testing wifi manager
  // wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  // or use this for auto generated name ESP + ChipID
  wifiManager.autoConnect();

  // Initiate OTA
  OTADRIVE.setInfo(APIKEY, FW_VER);
  OTADRIVE.onUpdateFirmwareProgress(onUpdateProgress);

  // Manual Wifi
  // WiFi.begin(WIFI_SSID, WIFI_PWD);
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Lacze sie z WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();

    counter++;
  }

  // Get time from network time service
  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");

  autoBrightness(&display);

  ui.setTargetFPS(60);

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, 4);

  ui.setOverlays(overlays, numberOfOverlays);

  ui.setTimePerFrame(8000);

  // Inital UI takes care of initalising the display too.
  ui.init();

  Serial.println("");

  otaUpdate();

  updateData(&display);
  updateAirplaneData(&display);
}

void loop()
{
  autoBrightness(&display);

  otaUpdate();

  if (millis() - timeSinceLastWUpdate > (1000L * UPDATE_INTERVAL_SECS))
  {
    readyForWeatherUpdate = true;
    timeSinceLastWUpdate = millis();
  }

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED)
  {
    updateData(&display);
  }

  if (millis() - timeSinceLastAUpdate > (1000L * UPDATE_AIRPLANES_INTERVAL_SECS))
  {
    readyForAirplaneUpdate = true;
    timeSinceLastAUpdate = millis();
  }

  if (readyForAirplaneUpdate && ui.getUiState()->frameState == FIXED)
  {
    updateAirplaneData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0)
  {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }
}

struct tm *getTimeInfo()
{
  now = time(nullptr);
  struct tm *timeInfo;
  timeInfo = localtime(&now);
  return timeInfo;
}

void autoBrightness(OLEDDisplay *display)
{
  int hour = getTimeInfo()->tm_hour;
  bool isNight = (hour < 8 || hour > 22) && time(nullptr) > 100000;
  display->setBrightness(isNight ? 1 : 255);
}

void drawProgress(OLEDDisplay *display, int percentage, String label)
{
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawString(64, 54, "Wersja: " FW_VER);
  display->drawProgressBar(2, 30, 124, 10, percentage);
  display->display();
}

void onUpdateProgress(int progress, int totalt)
{
  static int last = 0;
  int progressPercent = (100 * progress) / totalt;
  Serial.print("*");
  if (last != progressPercent)
  {
    drawProgress(&display, progressPercent, "Aktualizuje soft...");
    // print every 10%
    if (progressPercent % 10 == 0)
    {
      Serial.printf("%d", progressPercent);
    }
  }
  last = progressPercent;
}

void updateData(OLEDDisplay *display)
{
  drawProgress(display, 30, "Aktualizuje pogode...");
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID);
  drawProgress(display, 50, "Aktualizuje prognoze...");
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);

  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Gotowe...");
  delay(1000);
}

void updateAirplaneData(OLEDDisplay *display)
{
  drawProgress(display, 30, "Aktualizuje samoloty...");

  float randomLat = LAT + (float)random(-100, 100) / 100000;
  float randomLon = LON + (float)random(-100, 100) / 100000;
  airplanesClient.updateData(randomLat, randomLon, RADIUS);

  readyForAirplaneUpdate = false;
  drawProgress(display, 100, "Gotowe...");

  bool hasAirplanes = airplanesClient.hasAirplane();
  ui.setFrames(hasAirplanes ? framesAirplane : frames, hasAirplanes ? 5 : 4);
}

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  struct tm *timeInfo;
  timeInfo = getTimeInfo();
  char buff[16];

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = WDAY_NAMES[timeInfo->tm_wday];

  sprintf_P(buff, PSTR("%s, %02d/%02d/%04d"), WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_year + 1900);
  display->drawString(64 + x, 5 + y, String(buff));
  display->setFont(ArialMT_Plain_24);

  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(64 + x, 15 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  String description = replaceChars(currentWeather.description);

  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, description);

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(60 + x, 5 + y, temp);

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(32 + x, 0 + y, currentWeather.iconMeteoCon);
}

void drawForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex)
{
  time_t observationTimestamp = forecasts[dayIndex].observationTime;
  struct tm *timeInfo;
  timeInfo = localtime(&observationTimestamp);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, WDAY_NAMES[timeInfo->tm_wday]);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  String temp = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, temp);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
  now = time(nullptr);
  struct tm *timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 55, 128);
}

int counter = 0;
unsigned long lastMessageUpdate = millis();
unsigned long updateMessageEvery = 1000;
String messages[] = {"Swieci slonce", "Pada cieply deszcz", "Jest tecza", "Nie ma burzy", "Jest cieplo"};

void drawMessage(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 9 + y, "Z Toba zawsze");
  display->drawString(64 + x, 24 + y, messages[counter]);

  if ((millis() - lastMessageUpdate) > updateMessageEvery)
  {
    lastMessageUpdate = millis();
    counter = (counter + 1) % (sizeof(messages) / sizeof(String));
  }
}

void drawHeading(OLEDDisplay *display, int x, int y, double heading, int radius)
{
  int degrees[] = {0, 165, 195, 0};
  display->drawCircle(x, y, radius + 3);
  for (int i = 0; i < 3; i++)
  {
    int x1 = cos((-450 + (heading + degrees[i])) * pi / 180.0) * radius + x;
    int y1 = sin((-450 + (heading + degrees[i])) * pi / 180.0) * radius + y;
    int x2 = cos((-450 + (heading + degrees[i + 1])) * pi / 180.0) * radius + x;
    int y2 = sin((-450 + (heading + degrees[i + 1])) * pi / 180.0) * radius + y;
    display->drawLine(x1, y1, x2, y2);
  }
}

void drawAirplane1(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  if (!airplanesClient.hasAirplane())
    return;

  const AirplaneData &airplane = airplanesClient.getVisibleAircraft();

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x, y, "Typ:");
  display->drawString(x, 26 + y, "Znaki:");
  display->setFont(ArialMT_Plain_16);
  display->drawString(x, 10 + y, airplane.t);
  display->drawString(x, 36 + y, airplane.r);

  drawHeading(display, x + 112, y + 26, airplane.track, 12);
}

void drawAirplane2(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  if (!airplanesClient.hasAirplane())
    return;

  const AirplaneData &airplane = airplanesClient.getVisibleAircraft();

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x, y, "Odleglosc:");
  display->drawString(x, 27 + y, "Wysokosc:");
  display->drawString(66 + x, y, "Kurs:");
  display->drawString(66 + x, 27 + y, "Predkosc:");
  display->setFont(ArialMT_Plain_16);
  display->drawString(x, 10 + y, String(airplane.distance, 0) + "km");
  display->drawString(x, 37 + y, String(airplane.alt_baro) + "ft");
  display->drawString(66 + x, 10 + y, String(airplane.track) + "°");
  display->drawString(66 + x, 37 + y, String(airplane.gs) + "kts");
}

void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  // if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Prosze sie polaczyc z WiFi");
  display.drawString(64, 25, myWiFiManager->getConfigPortalSSID());
  display.drawString(64, 40, "aby sie polaczyc z pogoda");
  display.display();
}

void otaUpdate()
{
  // Every 1h
  if (!OTADRIVE.timeTick(3600) || DISABLE_OTA)
    return;
  Serial.println("Checking for updates");
  auto inf = OTADRIVE.updateFirmwareInfo();
  if (inf.available)
  {
    Serial.printf("\nNew version available, %dBytes, %s\n", inf.size, inf.version.c_str());
    OTADRIVE.updateFirmware();
  }
  else
  {
    Serial.println("\nNo newer version\n");
  }
}

String replaceChars(String input)
{
  // Define Polish and English character mappings
  String polishChars[] = {"ą", "ć", "ę", "ł", "ń", "ó", "ś", "ź", "ż",
                          "Ą", "Ć", "Ę", "Ł", "Ń", "Ó", "Ś", "Ź", "Ż"};
  String englishChars[] = {"a", "c", "e", "l", "n", "o", "s", "z", "z",
                           "A", "C", "E", "L", "N", "O", "S", "Z", "Z"};

  // Iterate through the input string and replace Polish characters
  for (unsigned int i = 0; i < input.length(); i++)
  {
    for (unsigned int j = 0; j < sizeof(polishChars) / sizeof(polishChars[0]); j++)
    {
      if (input.indexOf(polishChars[j]) != -1)
      {
        input.replace(polishChars[j], englishChars[j]);
      }
    }
  }
  return input; // Return the modified string
}