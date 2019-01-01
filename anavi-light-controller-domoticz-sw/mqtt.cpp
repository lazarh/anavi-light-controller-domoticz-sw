#include "global.h"
#include "mqtt.h"

char domoticz_in[200] = "domoticz/in";
char domoticz_out[200] = "domoticz/out";
char key_idx[20] = "idx";
char key_nvalue[20] = "nvalue";
char key_svalue[20] = "svalue";

long lastMsg = 0;
char msg[50];
int value = 0;

char stat_power_topic[44] = "domoticz/qos";
char stat_color_topic[44] = "domoticz/qos";

bool power = false;

int lightRed = 255;
int lightGreen = 255;
int lightBlue = 255;
int currentRed = 255;
int currentGreen = 255;
int currentBlue = 255;
int brightnessLevel = 255;

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

void calculateBrightness()
{
  unsigned int maximumBrightness = 255;
  lightRed = (currentRed * brightnessLevel) / maximumBrightness;
  lightBlue = (currentBlue * brightnessLevel) / maximumBrightness;
  lightGreen = (currentGreen * brightnessLevel) / maximumBrightness;
}
