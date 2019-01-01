#include "sensors.h"
#include "global.h"
#include "mqtt.h"

//Configure supported I2C sensors
const int sensorHTU21D =  0x40;
const int sensorBH1750 = 0x23;

float sensorTemperature = 0;
float sensorHumidity = 0;
uint16_t sensorAmbientLight = 0;

void setupADPS9960()
{
  if (apds.begin())
  {
    //gesture mode will be entered once proximity mode senses something close
    apds.enableProximity(true);
    apds.enableGesture(true);
  }
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
    
    sprintf(htu21d_value, "%.2f;%.2f;%d", sensorTemperature, sensorHumidity, sensorHumidityNum);
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
