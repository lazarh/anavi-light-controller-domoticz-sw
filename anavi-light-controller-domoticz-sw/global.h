#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <DNSServer.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>        //https://github.com/knolleary/pubsubclient
#include <MD5Builder.h>
#include <Wire.h>

#include "Adafruit_HTU21DF.h"
#include "Adafruit_APDS9960.h"

#define th_devid 24 // domoticz virtual sensor temp + humd dev_id
#define led_devid 26 // domoticz virtual sensor led_switch dev_id
#define lux_devid 27 // domoticz virtual sensor lux dev_id

// Configure pins
#define pinAlarm 16
#define pinButton 0
#define pinLedRed 12
#define pinLedGreen 13
#define pinLedBlue 14

//define your default values here, if there are different values in config.json, they are overwritten.
#define mqtt_server "192.168.0.200"
#define mqtt_port   "1883"
#define workgroup   "domoticz"

// MQTT username and password
#define username ""
#define password ""

extern char machineId[32];
extern Adafruit_HTU21DF htu;
extern Adafruit_APDS9960 apds;

extern unsigned long sensorPreviousMillis;
extern const long sensorInterval;
extern const long mqttConnectionInterval;
extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern unsigned long mqttConnectionPreviousMillis;
