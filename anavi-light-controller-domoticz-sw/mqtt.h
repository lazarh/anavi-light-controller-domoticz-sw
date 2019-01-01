#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <DNSServer.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>        //https://github.com/knolleary/pubsubclient
#include <MD5Builder.h>
#include <Wire.h>

void mqttCallback(char* topic, byte* payload, unsigned int length);

void mqttReconnect();

void publishState();

void publishSensorData(const char* subTopic, const char* key, const float value);

void publishSensorData(const char* subTopic, const char* key, const String& value);

void publishSensorDataDomoticz(const int dev_id, const char* value);

void publishSensorDataDomoticz(const int dev_id, uint16_t value);

void calculateBrightness();
