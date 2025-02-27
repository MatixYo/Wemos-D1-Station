#include <Arduino.h>

#define TZ "CET-1CEST,M3.5.0/2,M10.5.0/3" // Warsaw/Poland
#define HOSTNAME "ESP8266-OTA-"
#define APIKEY ""
#define OPEN_WEATHER_MAP_APP_ID ""
#define OPEN_WEATHER_MAP_LOCATION_ID "759734"
#define OPEN_WEATHER_MAP_LANGUAGE "pl"

// Rzeszow
const float LAT = 50.033849;
const float LON = 22.000820;
const int RADIUS = 15;

const int UPDATE_INTERVAL_SECS = 20 * 60;      // Update every 20 minutes
const int UPDATE_AIRPLANES_INTERVAL_SECS = 60; // Every minute

const boolean ENABLE_OTA = false;