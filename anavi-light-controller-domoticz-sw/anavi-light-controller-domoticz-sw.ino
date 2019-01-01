#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <PubSubClient.h>        //https://github.com/knolleary/pubsubclient

#include <MD5Builder.h>

#include <Wire.h>
#include "Adafruit_HTU21DF.h"
#include "Adafruit_APDS9960.h"

Adafruit_HTU21DF htu = Adafruit_HTU21DF();

Adafruit_APDS9960 apds;

//Configure supported I2C sensors
const int sensorHTU21D =  0x40;
const int sensorBH1750 = 0x23;

// Configure pins
const int pinAlarm = 16;
const int pinButton = 0;
const int pinLedRed = 12;
const int pinLedGreen = 13;
const int pinLedBlue = 14;

bool power = false;

int lightRed = 255;
int lightGreen = 255;
int lightBlue = 255;
int currentRed = 255;
int currentGreen = 255;
int currentBlue = 255;
int brightnessLevel = 255;

unsigned long sensorPreviousMillis = 0;
const long sensorInterval = 5000;

unsigned long mqttConnectionPreviousMillis = millis();
const long mqttConnectionInterval = 60000;

float sensorTemperature = 0;
float sensorHumidity = 0;
uint16_t sensorAmbientLight = 0;

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "192.168.0.200";
char mqtt_port[6] = "1883";
char workgroup[32] = "domoticz";
// MQTT username and password
char username[20] = "";
char password[20] = "";

//MD5 of chip ID
char machineId[32] = "";

// domoticz virtual sensor temp + humd dev_id
int th_devid = 24;

// domoticz virtual sensor led_switch dev_id
int led_devid = 26;

// domoticz virtual sensor lux dev_id
int lux_devid = 27;

char domoticz_in[200] = "domoticz/in";
char domoticz_out[200] = "domoticz/out";
char key_idx[20] = "idx";
char key_nvalue[20] = "nvalue";
char key_svalue[20] = "svalue";

//flag for saving data
bool shouldSaveConfig = false;

// MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

char stat_power_topic[44] = "domoticz/qos";;
char stat_color_topic[44] = "domoticz/qos";;

//callback notifying us of the need to save config
void saveConfigCallback ()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  //LED
  pinMode(pinAlarm, OUTPUT);
  //Button
  pinMode(pinButton, INPUT);

  //RGB LED Strip
  pinMode(pinLedRed, OUTPUT);
  pinMode(pinLedGreen, OUTPUT);
  pinMode(pinLedBlue, OUTPUT);

  digitalWrite(pinAlarm, HIGH);

  // Turn all 3 colors of the LED strip
  // This way the setup and testing will be easier
  analogWrite(pinLedRed, 255);
  analogWrite(pinLedGreen, 255);
  analogWrite(pinLedBlue, 255);

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin())
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        const size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success())
        {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(workgroup, json["workgroup"]);
          strcpy(username, json["username"]);
          strcpy(password, json["password"]);

        }
        else
        {
          Serial.println("failed to load json config");
        }
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
  //end read

  // Machine ID
  calculateMachineId();


  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_workgroup("workgroup", "workgroup", workgroup, 32);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT username", username, 20);
  WiFiManagerParameter custom_mqtt_pass("pass", "MQTT password", password, 20);

  char htmlMachineId[200];
  sprintf(htmlMachineId, "<p style=\"color: red;\">Machine ID:</p><p><b>%s</b></p><p>Copy and save the machine ID because you will need it to control the device.</p>", machineId);
  WiFiManagerParameter custom_text_machine_id(htmlMachineId);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_workgroup);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_text_machine_id);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("ANAVI Light Controller", ""))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  digitalWrite(pinAlarm, LOW);

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(workgroup, custom_workgroup.getValue());
  strcpy(username, custom_mqtt_user.getValue());
  strcpy(password, custom_mqtt_pass.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["workgroup"] = workgroup;
    json["username"] = username;
    json["password"] = password;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  // Sensors
  htu.begin();

  // MQTT
  Serial.print("MQTT Server: ");
  Serial.println(mqtt_server);
  Serial.print("MQTT Port: ");
  Serial.println(mqtt_port);
  Serial.print("MQTT Workgroup: ");
  Serial.println(workgroup);
  // Print MQTT Username
  Serial.print("MQTT Username: ");
  Serial.println(username);
  // Hide password from the log and show * instead
  char hiddenpass[20] = "";
  for (size_t charP = 0; charP < strlen(password); charP++)
  {
    hiddenpass[charP] = '*';
  }
  hiddenpass[strlen(password)] = '\0';
  Serial.print("MQTT Password: ");
  Serial.println(hiddenpass);

  const int mqttPort = atoi(mqtt_port);
  mqttClient.setServer(mqtt_server, mqttPort);
  mqttClient.setCallback(mqttCallback);

  mqttReconnect();

  Serial.println("");
  Serial.println("-----");
  Serial.print("Machine ID: ");
  Serial.println(machineId);
  Serial.println("-----");
  Serial.println("");

  setupADPS9960();
}

void setupADPS9960()
{
  if (apds.begin())
  {
    //gesture mode will be entered once proximity mode senses something close
    apds.enableProximity(true);
    apds.enableGesture(true);
  }
}

void factoryReset()
{
  if (false == digitalRead(pinButton))
  {
    Serial.println("Hold the button to reset to factory defaults...");
    for (int iter = 0; iter < 30; iter++)
    {
      digitalWrite(pinAlarm, HIGH);
      delay(100);
      digitalWrite(pinAlarm, LOW);
      delay(100);
    }
    if (false == digitalRead(pinButton))
    {
      Serial.println("Disconnecting...");
      WiFi.disconnect();

      // NOTE: the boot mode:(1,7) problem is known and only happens at the first restart after serial flashing.

      Serial.println("Restarting...");
      // Clean the file system with configurations
      SPIFFS.format();
      // Restart the board
      ESP.restart();
    }
    else
    {
      // Cancel reset to factory defaults
      Serial.println("Reset to factory defaults cancelled.");
      digitalWrite(pinAlarm, LOW);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  // Convert received bytes to a string
  char text[length + 1];
  snprintf(text, length + 1, "%s", payload);
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] len[");
  Serial.print(length);
  Serial.println("] ");

  if (strcmp(topic, domoticz_out) == 0)
  {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& data = jsonBuffer.parseObject(text);

    if (data.containsKey(String("idx")))
    {
      const int idx = data["idx"];
      if  (idx == led_devid)
      {
        const int r = data[String("Color")][String("r")];
        const int g = data[String("Color")][String("g")];
        const int b = data[String("Color")][String("b")];
        currentRed = ((0 <= r) && (255 >= r)) ? r : 0;
        currentGreen = ((0 <= g) && (255 >= g)) ? g : 0;
        currentBlue = ((0 <= b) && (255 >= b)) ? b : 0;

        if (data.containsKey("Level"))
        {
          const int level = data["Level"];
          if ( (0 <= level) && (255 >= level) )
          {
            brightnessLevel = level;
          }
        }

        calculateBrightness();

        if (data.containsKey("nvalue"))
        {
          const int nvalue = data[String("nvalue")];
          if (nvalue == 0)
          {
            power = false;
          }
          else
          {
            power = true;
          }
        }

        publishState();

        Serial.print("Red: ");
        Serial.println(lightRed);
        Serial.print("Green: ");
        Serial.println(lightGreen);
        Serial.print("Blue: ");
        Serial.println(lightBlue);
        Serial.print("Power: ");
        Serial.println(power);

        // Set colors of RGB LED strip
        if (power)
        {
          analogWrite(pinLedRed, lightRed);
          analogWrite(pinLedGreen, lightGreen);
          analogWrite(pinLedBlue, lightBlue);
        }
        else
        {
          analogWrite(pinLedRed, 0);
          analogWrite(pinLedGreen, 0);
          analogWrite(pinLedBlue, 0);
        }
      }
    }
  }
}

void calculateBrightness()
{
  unsigned int maximumBrightness = 255;
  lightRed = (currentRed * brightnessLevel) / maximumBrightness;
  lightBlue = (currentBlue * brightnessLevel) / maximumBrightness;
  lightGreen = (currentGreen * brightnessLevel) / maximumBrightness;
}

void calculateMachineId()
{
  MD5Builder md5;
  md5.begin();
  char chipId[25];
  sprintf(chipId, "%d", ESP.getChipId());
  md5.add(chipId);
  md5.calculate();
  md5.toString().toCharArray(machineId, 32);
}

void mqttReconnect()
{
  // Loop until we're reconnected
  for (int attempt = 0; attempt < 3; ++attempt)
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    //String clientId = "ESP8266Client-";
    //clientId += String(random(0xffff), HEX);
    const String clientId = "light-controller-1";
    // Attempt to connect
    if (true == mqttClient.connect(clientId.c_str(), username, password))
    {
      Serial.println("connected");

      // Subscribe to MQTT topics
      mqttClient.subscribe(domoticz_out);
      // subscribing to topic
      Serial.print("Subscribing to topic \"");
      Serial.print(domoticz_out);
      Serial.println("\"");
      break;

    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publishState()
{
  StaticJsonBuffer<150> jsonBuffer;
  char payload[150] = {0};
  JsonObject& json = jsonBuffer.createObject();
  const char* state = power ? "ON" : "OFF";
  json["state"] = state;
  json["brightness"] = brightnessLevel;

  JsonObject& color = json.createNestedObject("color");
  color["r"] = power ? currentRed : 0;
  color["g"] = power ? currentGreen : 0;
  color["b"] = power ? currentBlue : 0;

  json.printTo((char*)payload, json.measureLength() + 1);

  Serial.print("[");
  Serial.print(stat_color_topic);
  Serial.print("] ");
  Serial.println(payload);
  mqttClient.publish(stat_color_topic, payload, true);

  Serial.print("[");
  Serial.print(stat_power_topic);
  Serial.print("] ");
  Serial.println(state);
}

void publishSensorData(const char* subTopic, const char* key, const float value)
{
  StaticJsonBuffer<100> jsonBuffer;
  char payload[100];
  JsonObject& json = jsonBuffer.createObject();
  json[key] = value;
  json.printTo((char*)payload, json.measureLength() + 1);
  char topic[200];
  sprintf(topic, "%s/%s/%s", workgroup, machineId, subTopic);
  mqttClient.publish(topic, payload, true);
}

void publishSensorData(const char* subTopic, const char* key, const String& value)
{
  StaticJsonBuffer<100> jsonBuffer;
  char payload[100];
  JsonObject& json = jsonBuffer.createObject();
  json[key] = value;
  json.printTo((char*)payload, json.measureLength() + 1);
  char topic[200];
  sprintf(topic, "%s/%s/%s", workgroup, machineId, subTopic);
  mqttClient.publish(topic, payload, true);
}

void publishSensorDataDomoticz(const int dev_id, const char* value)
{
  StaticJsonBuffer<500> jsonBuffer;
  char payload[500];
  JsonObject& json = jsonBuffer.createObject();
  json[key_idx] = dev_id;
  json[key_nvalue] = 0; // always 0
  json[key_svalue] = value;
  json.printTo((char*)payload, json.measureLength() + 1);
  mqttClient.publish(domoticz_in, payload, true);
}

void publishSensorDataDomoticz(const int dev_id, uint16_t value)
{
  StaticJsonBuffer<500> jsonBuffer;
  char payload[500];
  JsonObject& json = jsonBuffer.createObject();
  json[key_idx] = dev_id;
  json[key_nvalue] = value; // always 0
  json[key_svalue] = "";
  json.printTo((char*)payload, json.measureLength() + 1);
  mqttClient.publish(domoticz_in, payload, true);
}

bool isSensorAvailable(int sensorAddress)
{
  // Check if I2C sensor is present
  Wire.beginTransmission(sensorAddress);
  return 0 == Wire.endTransmission();
}

void handleHTU21D()
{
  // Check if temperature has changed
  const float tempTemperature = htu.readTemperature();
  int sensorHumidityNum = 0;

  // Check if humidity has changed
  const float tempHumidity = htu.readHumidity();

  char htu21d_value[100];

  if ((1 <= abs(tempTemperature - sensorTemperature)) ||
      (1 <= abs(tempHumidity - sensorHumidity)))
  {
    // Print new temprature value
    sensorTemperature = tempTemperature;
    Serial.print("Temperature: ");
    Serial.print(sensorTemperature);
    Serial.println("C");

    // Publish new temperature value through MQTT
    publishSensorData("temperature", "temperature", sensorTemperature);

    // Print new humidity value
    sensorHumidity = tempHumidity;
    Serial.print("Humidity: ");
    Serial.print(sensorHumidity);
    Serial.println("%");

    // Publish new humidity value through MQTT
    publishSensorData("humidity", "humidity", sensorHumidity);

    // Mapping for Humidity_status:
    // 0    = Normal
    // 1    <> 46-70%   = Comfortable
    // 2    < 46        = Dry
    // 3    > 70%       = Wet

    if ((sensorHumidity >= 46) && (sensorHumidity <= 70))
    {
      sensorHumidityNum = 1;
    } else if (sensorHumidity < 46) 
    {
      sensorHumidityNum = 2;
    } else if (sensorHumidity > 70) 
    {
      sensorHumidityNum = 3;
    }
    
    sprintf(htu21d_value, "%.2f;%.2f", sensorTemperature, sensorHumidity, sensorHumidityNum);
    publishSensorDataDomoticz(th_devid, htu21d_value);
  }
}

void sensorWriteData(int i2cAddress, uint8_t data)
{
  Wire.beginTransmission(i2cAddress);
  Wire.write(data);
  Wire.endTransmission();
}

void handleBH1750()
{
  Wire.begin();
  // Power on sensor
  sensorWriteData(sensorBH1750, 0x01);
  // Set mode continuously high resolution mode
  sensorWriteData(sensorBH1750, 0x10);

  uint16_t tempAmbientLight;
  char ambientLightLux[1000] = "";

  Wire.requestFrom(sensorBH1750, 2);
  tempAmbientLight = Wire.read();
  tempAmbientLight <<= 8;
  tempAmbientLight |= Wire.read();
  // s. page 7 of datasheet for calculation
  tempAmbientLight = tempAmbientLight / 1.2;

  if (1 <= abs(tempAmbientLight - sensorAmbientLight))
  {
    // Print new humidity value
    sensorAmbientLight = tempAmbientLight;
    Serial.print("Light: ");
    Serial.print(tempAmbientLight);
    Serial.println("Lux");

    // Publish new humidity value through MQTT
    publishSensorData("light", "light", sensorAmbientLight);
    sprintf(ambientLightLux, "%20d", sensorAmbientLight);
    publishSensorDataDomoticz(lux_devid, ambientLightLux);
  }
}

void detectGesture()
{
  //read a gesture from the device
  const uint8_t gestureCode = apds.readGesture();
  // Skip if gesture has not been detected
  if (0 == gestureCode)
  {
    return;
  }
  String gesture = "";
  switch (gestureCode)
  {
    case APDS9960_DOWN:
      gesture = "down";
      break;
    case APDS9960_UP:
      gesture = "up";
      break;
    case APDS9960_LEFT:
      gesture = "left";
      break;
    case APDS9960_RIGHT:
      gesture = "right";
      break;
  }
  Serial.print("Gesture: ");
  Serial.println(gesture);
  // Publish the detected gesture through MQTT
  publishSensorData("gesture", "gesture", gesture);
}

void handleSensors()
{
  if (isSensorAvailable(sensorHTU21D))
  {
    handleHTU21D();
  }
  if (isSensorAvailable(sensorBH1750))
  {
    handleBH1750();
  }
}

void loop()
{
  // put your main code here, to run repeatedly:
  mqttClient.loop();

  // Reconnect if there is an issue with the MQTT connection
  const unsigned long mqttConnectionMillis = millis();
  if ( (false == mqttClient.connected()) && (mqttConnectionInterval <= (mqttConnectionMillis - mqttConnectionPreviousMillis)) )
  {
    mqttConnectionPreviousMillis = mqttConnectionMillis;
    mqttReconnect();
  }

  const unsigned long currentMillis = millis();
  if (sensorInterval <= (currentMillis - sensorPreviousMillis))
  {
    sensorPreviousMillis = currentMillis;
    handleSensors();
  }

  // Handle gestures at a shorter interval
  if (isSensorAvailable(APDS9960_ADDRESS))
  {
    detectGesture();
  }

  // Press and hold the button to reset to factory defaults
  factoryReset();
}
