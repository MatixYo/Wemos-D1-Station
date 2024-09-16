#include "AdsbExchangeClient.h"

#include <math.h> // Added for distance calculation

#define EARTH_RADIUS_KM 6371.0 // Earth's radius in kilometers

AdsbExchangeClient::AdsbExchangeClient()
{
}

void AdsbExchangeClient::updateVisibleAircraft(double lat, double lon)
{
  JsonStreamingParser parser;
  parser.setListener(this);
  WiFiClient client;

  this->stationLat = lat;
  this->stationLon = lon;

  const char host[] = "192.168.3.5";
  // get current timestamp
  String url = "/skyaware/data/aircraft.json?_=" + String(millis());

  const int httpPort = 80;
  if (!client.connect(host, httpPort))
  {
    Serial.println("connection failed");
    return;
  }

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  int retryCounter = 0;
  while (!client.available())
  {
    Serial.println(".");
    delay(1000);
    retryCounter++;
    if (retryCounter > 10)
    {
      return;
    }
  }

  int pos = 0;
  boolean isBody = false;
  char c;
  client.setNoDelay(false);
  while (client.available() || client.connected())
  {
    while (client.available())
    {
      c = client.read();
      if (c == '{' || c == '[')
      {
        isBody = true;
      }
      if (isBody)
      {
        parser.parse(c);
      }
    }
    client.stop();
  }
  endDocument();
}

String AdsbExchangeClient::getFrom()
{
  if (from[CURRENT].length() >= 4)
  {
    int firstComma = from[CURRENT].indexOf(",");
    return from[CURRENT].substring(5, firstComma);
  }
  return "";
}
String AdsbExchangeClient::getFromIcao()
{
  if (from[CURRENT].length() >= 4)
  {
    return from[CURRENT].substring(0, 4);
  }
  return "";
}
String AdsbExchangeClient::getTo()
{
  if (to[CURRENT].length() >= 4)
  {
    int firstComma = to[CURRENT].indexOf(",");
    return to[CURRENT].substring(5, firstComma);
  }
  return "";
}

String AdsbExchangeClient::getToIcao()
{
  if (to[CURRENT].length() >= 4)
  {
    return to[CURRENT].substring(0, 4);
  }
  return "";
}

String AdsbExchangeClient::getFlight()
{
  return flight[CURRENT];
}
double AdsbExchangeClient::getGs()
{
  return gs[CURRENT];
}

String AdsbExchangeClient::getAltitude()
{
  return altitude[CURRENT];
}
double AdsbExchangeClient::getDistance()
{
  return distance[CURRENT];
}
String AdsbExchangeClient::getAircraftType()
{
  return aircraftType[CURRENT];
}
String AdsbExchangeClient::getOperatorCode()
{
  return operatorCode[CURRENT];
}

double AdsbExchangeClient::getHeading()
{
  return heading[CURRENT];
}

void AdsbExchangeClient::whitespace(char c)
{
}

void AdsbExchangeClient::startDocument()
{
  counter = 0;
  currentMinDistance = 1000.0;
}

void AdsbExchangeClient::key(String key)
{
  currentKey = key;
}

void AdsbExchangeClient::value(String value)
{
  /*String from = "";
  String to = "";
  String altitude = "";
  String aircraftType = "";
  String currentKey = "";
  String operator = "";


 "Type": "A319",
 "Mdl": "Airbus A319 112",

 "From": "LSZH Z\u00c3\u00bcrich, Zurich, Switzerland",
 "To": "LEMD Madrid Barajas, Spain",
 "Op": "Swiss International Air Lines",
 "OpIcao": "SWR",
 "Dst": 6.23,
 "Year": "1996"
 */
  if (currentKey == "hex")
  {
    counter++;
  }
  else if (currentKey == "flight")
  {
    value.trim();
    flight[TEMP] = value;
  }
  else if (currentKey == "gs")
  {
    gs[TEMP] = value.toFloat();
  }
  else if (currentKey == "mag_heading")
  {
    heading[TEMP] = value.toFloat();
  }
  else if (currentKey == "alt_baro")
  {
    altitude[TEMP] = value;
  }
  else if (currentKey == "lat")
  {
    lat[TEMP] = value.toFloat();
  }
  else if (currentKey == "lon")
  {
    lon[TEMP] = value.toFloat();
  }
  else if (currentKey == "rssi")
  {
    distance[TEMP] = calculateDistance(this->stationLon, lon[TEMP], this->stationLat, lat[TEMP]);
    if (distance[TEMP] < currentMinDistance)
    {
      currentMinDistance = distance[TEMP];
      Serial.println("Found a closer aircraft");
      from[CURRENT] = from[TEMP];
      to[CURRENT] = to[TEMP];
      altitude[CURRENT] = altitude[TEMP];
      distance[CURRENT] = distance[TEMP];
      aircraftType[CURRENT] = aircraftType[TEMP];
      operatorCode[CURRENT] = operatorCode[TEMP];
      heading[CURRENT] = heading[TEMP];
      lat[CURRENT] = lat[TEMP];
      lon[CURRENT] = lon[TEMP];
      gs[CURRENT] = gs[TEMP];
      flight[CURRENT] = flight[TEMP];
    }
  }
}

int AdsbExchangeClient::getNumberOfVisibleAircrafts()
{
  return counter;
}

void AdsbExchangeClient::endArray()
{
}

void AdsbExchangeClient::endObject()
{
}

void AdsbExchangeClient::endDocument()
{
  Serial.println("Flights: " + String(counter));
  if (counter == 0 && lastSightingMillis < millis() - MAX_AGE_MILLIS)
  {
    for (int i = 0; i < 2; i++)
    {
      from[i] = "";
      to[i] = "";
      altitude[i] = "";
      distance[i] = 1000.0;
      aircraftType[i] = "";
      operatorCode[i] = "";
      heading[i] = 0.0;
    }
  }
  else if (counter > 0)
  {
    lastSightingMillis = millis();
  }
}

boolean AdsbExchangeClient::isAircraftVisible()
{
  return counter > 0 || lastSightingMillis > millis() - MAX_AGE_MILLIS;
}

void AdsbExchangeClient::startArray()
{
}

void AdsbExchangeClient::startObject()
{
}

double AdsbExchangeClient::calculateDistance(double lon1, double lon2, double lat1, double lat2)
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

  return distance; // Distance in kilometers
}
