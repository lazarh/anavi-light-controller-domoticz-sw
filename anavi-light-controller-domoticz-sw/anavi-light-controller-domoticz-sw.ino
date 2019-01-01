#include "global.h"
#include "sensors.h"
#include "mqtt.h"

//flag for saving data
bool shouldSaveConfig = false;

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
  char line1[100] = "<p style=\"color: red;\">Machine ID:</p><p><b>%s</b></p><p>Copy and save the machine ID because";
  char line2[100] = " you will need it to control the device.</p>";
  sprintf(htmlMachineId, line1, line2, machineId);
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
