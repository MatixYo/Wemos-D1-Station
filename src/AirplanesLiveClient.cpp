#include "AirplanesLiveClient.h"

#define EARTH_RADIUS_KM 6371.0 // Earth's radius in kilometers
#define FETCH_INTERVAL 10000

AirplanesLiveClient::AirplanesLiveClient()
{
}

const AirplaneData &AirplanesLiveClient::getVisibleAircraft() const
{
    return this->visibleAircraft;
}

bool AirplanesLiveClient::hasAirplane()
{
    return this->hasVisibleAirplane;
}

String AirplanesLiveClient::fetchApi(double lat, double lon, int radius)
{
    HTTPClient http;

    String url = "https://api.airplanes.live/v2/point/" + String(lat, 6) + "/" + String(lon, 6) + "/" + String(radius);

    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();

    Serial.println("Fetching: " + url);
    if (http.begin(*client, url.c_str()))
    {
        Serial.print("[HTTP] GET...\n");
        // start connection and send HTTP header
        int httpCode = http.GET();

        // httpCode will be negative on error
        if (httpCode > 0)
        {
            // HTTP header has been send and Server response header has been handled
            Serial.printf("[HTTP] GET... code: %d\n", httpCode);

            // file found at server
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
            {
                String payload = http.getString();
                Serial.println("[AirplanesLiveClient] Response length: " + String(payload.length()));
                return payload;
            }
        }
        else
        {
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
    }
    else
    {
        Serial.println("[HTTP] Unable to connect");
    }

    return "";
}

double AirplanesLiveClient::calculateDistance(double lat1, double lon1, double lat2, double lon2)
{
    // Convert degrees to radians
    double dLat = radians(lat2 - lat1);
    double dLon = radians(lon2 - lon1);

    // Haversine formula
    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(radians(lat1)) * cos(radians(lat2)) *
                   sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    // Calculate the distance
    double distance = EARTH_RADIUS_KM * c;

    Serial.println("[AirplanesLiveClient] Calculated distance..." + String(lat1) + "," + String(lon1) + " to " + String(lat2) + "," + String(lon2) + " = " + String(distance) + "km");

    return distance; // Distance in kilometers
}

void AirplanesLiveClient::updateDistance(JsonDocument *doc, double lat, double lon)
{
    for (JsonVariant ac : (*doc)["ac"].as<JsonArray>())
    {
        if (ac.containsKey("lat") && ac.containsKey("lon"))
        {
            double acLat = ac["lat"].as<double>();
            double acLon = ac["lon"].as<double>();
            double distance = calculateDistance(lat, lon, acLat, acLon);
            ac["distance"] = distance;
        }
        else
        {
            ac.clear();
        }
    }
}

AirplaneData AirplanesLiveClient::assignJsonToStruct(JsonVariant &airplane)
{
    AirplaneData airplaneData;

    // Check and assign each field from the JSON if it exists
    if (airplane.containsKey("hex"))
    {
        airplaneData.hex = airplane["hex"].as<const char *>();
    }
    if (airplane.containsKey("flight"))
    {
        airplaneData.flight = airplane["flight"].as<const char *>();
    }
    if (airplane.containsKey("r"))
    {
        airplaneData.r = airplane["r"].as<const char *>();
    }
    if (airplane.containsKey("t"))
    {
        airplaneData.t = airplane["t"].as<const char *>();
    }
    if (airplane.containsKey("desc"))
    {
        airplaneData.desc = airplane["desc"].as<const char *>();
    }
    if (airplane.containsKey("alt_baro"))
    {
        airplaneData.alt_baro = airplane["alt_baro"].as<int>();
    }
    if (airplane.containsKey("alt_geom"))
    {
        airplaneData.alt_geom = airplane["alt_geom"].as<int>();
    }
    if (airplane.containsKey("gs"))
    {
        airplaneData.gs = airplane["gs"].as<int>();
    }
    if (airplane.containsKey("track"))
    {
        airplaneData.track = airplane["track"].as<int>();
    }
    if (airplane.containsKey("baro_rate"))
    {
        airplaneData.baro_rate = airplane["baro_rate"].as<int>();
    }
    if (airplane.containsKey("squawk"))
    {
        airplaneData.squawk = airplane["squawk"].as<const char *>();
    }
    if (airplane.containsKey("lat"))
    {
        airplaneData.lat = airplane["lat"].as<float>();
    }
    if (airplane.containsKey("lon"))
    {
        airplaneData.lon = airplane["lon"].as<float>();
    }
    if (airplane.containsKey("distance"))
    {
        airplaneData.distance = airplane["distance"].as<float>();
    }

    return airplaneData; // Return the filled structure
}

void AirplanesLiveClient::updateData(double lat, double lon, int radius)
{
    JsonDocument doc;
    String json = this->fetchApi(lat, lon, radius);
    deserializeJson(doc, json);

    Serial.println("[AirplanesLiveClient] Total aircraft: " + String(doc["ac"].size()));

    this->updateDistance(&doc, lat, lon);

    // Find doc["ac"] with lowest ["distance"]
    double minDistance = std::numeric_limits<double>::max();
    int closestIndex = -1;

    // Iterate over each doc["ac"] element
    int index = 0;
    for (JsonVariant ac : doc["ac"].as<JsonArray>())
    {
        double distance = ac["distance"];
        if (distance < minDistance)
        {
            minDistance = distance;
            closestIndex = index;
        }
        index++;
    }

    this->hasVisibleAirplane = closestIndex != -1;
    if (index == -1)
    {
        Serial.println("[AirplanesLiveClient] No aircraft found.");
        return;
    }

    JsonVariant visibleAircraftJson = doc["ac"][closestIndex].as<JsonVariant>();
    Serial.println("[AirplanesLiveClient] Closest: " + String(visibleAircraftJson));
    this->visibleAircraft = this->assignJsonToStruct(visibleAircraftJson);
}
