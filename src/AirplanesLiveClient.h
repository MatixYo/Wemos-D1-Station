#pragma once

#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

#include <math.h> // Added for distance calculation

#include <map>
#include <functional>

struct AirplaneData
{
    String hex;
    String flight;
    String r;
    String t;
    String desc;
    int alt_baro;
    int alt_geom;
    int gs;
    int track;
    int baro_rate;
    String squawk;
    float lat;
    float lon;
    float distance;
};

class AirplanesLiveClient
{
private:
    double lat;
    double lng;
    int radius;
    bool hasVisibleAirplane;
    AirplaneData visibleAircraft;

    String fetchApi(double lat, double lng, int radius);
    double calculateDistance(double lat1, double lon1, double lat2, double lon2);
    void updateDistance(JsonDocument *ac);
    AirplaneData assignJsonToStruct(JsonVariant &ac);

public:
    AirplanesLiveClient();
    void updateData(double lat, double lng, int radius);
    const AirplaneData &getVisibleAircraft() const;
    bool hasAirplane();
};
