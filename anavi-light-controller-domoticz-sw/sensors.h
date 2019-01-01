#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <DNSServer.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>        //https://github.com/knolleary/pubsubclient
#include <MD5Builder.h>
#include <Wire.h>

void setupADPS9960();

bool isSensorAvailable(int sensorAddress);

void handleHTU21D();

void sensorWriteData(int i2cAddress, uint8_t data);

void handleBH1750();

void detectGesture();

void handleSensors();
