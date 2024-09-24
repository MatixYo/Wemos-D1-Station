# 1 "/var/folders/rd/t47ysqws1z37d388k0z_qjb40000gn/T/tmpydipelzt"
#include <Arduino.h>
# 1 "/Users/matix/Documents/PlatformIO/Projects/Wemos D1 Station/src/WeatherStationDemo.ino"
# 26 "/Users/matix/Documents/PlatformIO/Projects/Wemos D1 Station/src/WeatherStationDemo.ino"
#include <Arduino.h>
#include <config.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <coredecls.h>
#else
#include <WiFi.h>
#endif
#include <ESPHTTPClient.h>
#include <JsonListener.h>


#include <time.h>
#include <sys/time.h>

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





#define TZ 1
#define DST_MN 60


const int UPDATE_INTERVAL_SECS = 20 * 60;

const int UPDATE_AIRPLANES_INTERVAL_SECS = 30;


const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D2;
const int SDC_PIN = D1;
#else
const int SDA_PIN = 5;
const int SDC_PIN = 4;
#endif

#define FW_VER "v@1.0.4"
# 98 "/Users/matix/Documents/PlatformIO/Projects/Wemos D1 Station/src/WeatherStationDemo.ino"
const uint8_t MAX_FORECASTS = 4;

const boolean IS_METRIC = true;

const boolean DISABLE_OTA = true;


const String WDAY_NAMES[] = {"NIE", "PON", "WTO", "SRO", "CZW", "PIA", "SOB"};
const String MONTH_NAMES[] = {"STY", "LUT", "MAR", "KWI", "MAJ", "CZE", "LIP", "SIE", "WRZ", "PAŹ", "LIS", "GRU"};






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
time_t now;


bool readyForWeatherUpdate = false;

bool readyForAirplaneUpdate = false;

long timeSinceLastWUpdate = 0;

long timeSinceLastAUpdate = 0;


void configModeCallback(WiFiManager *myWiFiManager);
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawMessage(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);
void drawAirplane(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void autoBrightness(OLEDDisplay *display);
void otaUpdate();




FrameCallback frames[] = {drawDateTime, drawCurrentWeather, drawForecast};
FrameCallback framesAirplane[] = {drawDateTime, drawAirplane};

OverlayCallback overlays[] = {drawHeaderOverlay};
int numberOfOverlays = 1;
void setup();
void loop();
void onUpdateProgress(int progress, int totalt);
void updateAirplaneData(OLEDDisplay *display);
#line 161 "/Users/matix/Documents/PlatformIO/Projects/Wemos D1 Station/src/WeatherStationDemo.ino"
void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();


  display.init();
  display.clear();
  display.display();


  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);



  WiFiManager wifiManager;


  wifiManager.setAPCallback(configModeCallback);


  wifiManager.autoConnect();


  OTADRIVE.setInfo(APIKEY, FW_VER);
  OTADRIVE.onUpdateFirmwareProgress(onUpdateProgress);



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


  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");

  autoBrightness(&display);

  ui.setTargetFPS(60);

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);



  ui.setIndicatorPosition(BOTTOM);


  ui.setIndicatorDirection(LEFT_RIGHT);



  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, 3);

  ui.setOverlays(overlays, numberOfOverlays);

  ui.setTimePerFrame(8000);


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
  bool isNight = hour < 8 || hour > 22;
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
  airplanesClient.updateData(LAT, LNG, RADIUS);

  readyForAirplaneUpdate = false;
  drawProgress(display, 100, "Gotowe...");

  bool hasAirplanes = airplanesClient.hasAirplane();
  ui.setFrames(hasAirplanes ? framesAirplane : frames, hasAirplanes ? 2 : 3);
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
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, currentWeather.description);

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
  display->drawString(0, 55, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(128, 55, temp);
  display->drawHorizontalLine(0, 54, 128);
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

void drawAirplane(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  if (!airplanesClient.hasAirplane())
    return;

  const AirplaneData &airplane = airplanesClient.getVisibleAircraft();

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x, y, "Dys:");
  display->drawString(64 + x, y, "Wys:");
  display->drawString(x, 20 + y, "Hdg:");
  display->setFont(ArialMT_Plain_10);
  display->drawString(x, 10 + y, String(airplane.distance, 0) + "km");
  display->drawString(64 + x, 10 + y, String(airplane.alt_baro) + "ft");
  display->drawString(x, 30 + y, String(airplane.track) + "°");


}

void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

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