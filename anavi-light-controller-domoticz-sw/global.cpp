#include "global.h"

//MD5 of chip ID
char machineId[32] = "";

Adafruit_HTU21DF htu = Adafruit_HTU21DF();
Adafruit_APDS9960 apds;

unsigned long sensorPreviousMillis = 0;
const long sensorInterval = 5000;
const long mqttConnectionInterval = 60000;
unsigned long mqttConnectionPreviousMillis = millis();
WiFiClient espClient;
PubSubClient mqttClient(espClient);
