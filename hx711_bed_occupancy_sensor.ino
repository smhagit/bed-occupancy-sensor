/*******************************************************************************************
* Features:
*   - WiFi connection/ reconnection
*   - MQTT connection/ reconnection
*   - Publish data to MQTT topic
*   - Two HX711 LoadCell sensors
* Created for a ESP8266/ NodeMCU project
* 
* 
* ******************************************************************************************
*/


/** HX711 Load Cell Sensor **/
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/
#include <HX711_ADC.h>
const int HX711_DATA_LEFT = D7;
const int HX711_SLCK_LEFT = D6;

const int HX711_DATA_RIGHT = D3;
const int HX711_SLCK_RIGHT = D2;

HX711_ADC LoadCell_left(HX711_DATA_LEFT, HX711_SLCK_LEFT);
HX711_ADC LoadCell_right(HX711_DATA_RIGHT, HX711_SLCK_RIGHT);

 

/** Some libraries **/
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/
#include <ArduinoJson.h>

/** WLAN **/
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/
#include <ESP8266WiFi.h>
#include <Ticker.h>

#define WIFI_SSID "<YOUR WIFI SSID>"
#define WIFI_PASSWORD "<WIFI PASSWORD>"

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;



/** MQTT **/
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/
#include <AsyncMqttClient.h>

#define MQTT_HOST IPAddress(192, 168, 2, 83)
#define MQTT_PORT 1883
static const char mqttUser[] = "bedsensor";
static const char mqttPassword[] = "<MQTT USER PASSWORD>";

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;




/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/

static bool WIFI_STATUS = false;
static bool MQTT_STATUS = false;

static const char MQTT_PUBLISH_TOPIC_LEFT[] = "esp8266/sensor/bed-occupancy/left/state";
static const char MQTT_SENSOR_NAME_LEFT[] = "bed-ocupancy-left";
static const char MQTT_NAME_LEFT[] = "Left bed occupancy sensor";


static const char MQTT_PUBLISH_TOPIC_RIGHT[] = "esp8266/sensor/bed-occupancy/right/state";
static const char MQTT_SENSOR_NAME_RIGHT[] = "bed-ocupancy-right";
static const char MQTT_NAME_RIGHT[] = "Right bed occupancy sensor";


/**
 * SETUP
 */
void setup() {
  Serial.begin(9600);
  delay(3000);

  Serial.println("== [SETUP]: started ==");

 
  // Register MQTT Events/Callbacks 
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(mqttUser, mqttPassword);
  

  
  // Setup WiFi
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
    WiFi.onEvent([](WiFiEvent_t e) {
        Serial.printf("Event wifi -----> %d\n", e);

        switch(e) {
          case WIFI_EVENT_STAMODE_GOT_IP:
            Serial.println("WiFi connected");
            Serial.println("IP address: ");
            Serial.println(WiFi.localIP());

            connectToMqtt();
            Serial.println("== [SETUP]: MQTT ready ==");

            break;
          case WIFI_EVENT_STAMODE_DISCONNECTED:
            Serial.println("WiFi lost connection");
            break;
        }

    });
  connectToWifi();
  Serial.println("== [SETUP]: WiFi ready ==");


  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step

  // Setup HX711 left
  LoadCell_left.begin();
  LoadCell_left.start(stabilizingtime, _tare);
  if (LoadCell_left.getTareTimeoutFlag() || LoadCell_left.getSignalTimeoutFlag()) {
    Serial.println("== [SETUP]: Error: check loadcell_left wiring and/or pins ==");
  }
  else {
    LoadCell_left.setCalFactor(1.0); // user set calibration value (float)
    Serial.println("== [SETUP]: loadcell_left ready ==");
  }
  while (!LoadCell_left.update());
  //calibrate(); //start calibration procedure
  //Serial.println("LoadCell: calibrate");


  delay(200);

    // Setup HX711 left
  LoadCell_right.begin();
  LoadCell_right.start(stabilizingtime, _tare);
  if (LoadCell_right.getTareTimeoutFlag() || LoadCell_right.getSignalTimeoutFlag()) {
    Serial.println("== [SETUP]: Error: check loadcell_right wiring and/or pins ==");
  }
  else {
    LoadCell_right.setCalFactor(1.0); // user set calibration value (float)
    Serial.println("== [SETUP]: loadcell_right ready ==");
  }
  while (!LoadCell_right.update());
  //calibrate(); //start calibration procedure
  //Serial.println("LoadCell: calibrate");
  

  Serial.println("== [SETUP]: completed ==");
}




/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/

/**
 * LOOP
 */
void loop() {
  
  const int serialPrintInterval = 2; //increase value to slow down serial print activity
  bool newDataReady = false;
  float loadCellWeight_left;
  float loadCellWeight_right;

  if (LoadCell_left.update()) newDataReady = true;
  // get smoothed value from the dataset:
  if (newDataReady) {
      loadCellWeight_left = LoadCell_left.getData();
      Serial.print("LoadCell_LEFT output val: ");
      Serial.println(loadCellWeight_left);
  }
  newDataReady = false;
  delay(300);


  if (LoadCell_right.update()) newDataReady = true;
  // get smoothed value from the dataset:
  if (newDataReady) {
      loadCellWeight_right = LoadCell_right.getData();
      Serial.print("LoadCell_RIGHT output val: ");
      Serial.println(loadCellWeight_right);
  }
  newDataReady = false;
  delay(300);

  Serial.print("Start publishing to MQTT...");


  publishToMqtt(MQTT_SENSOR_NAME_LEFT, MQTT_NAME_LEFT, MQTT_PUBLISH_TOPIC_LEFT, loadCellWeight_left);
  publishToMqtt(MQTT_SENSOR_NAME_RIGHT, MQTT_NAME_RIGHT, MQTT_PUBLISH_TOPIC_RIGHT, loadCellWeight_right);


  delay(1000 * 2);
  return;
  
}

/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/



/**
 * Publish data to mqtt
 */
void publishToMqtt(const char* sensorName, const char* mqttName, const char* topic, float value) {
  // Publish sensor data via MQTT
  StaticJsonDocument<400> payload;
  payload["sensor"] = sensorName;
  payload["name"] = mqttName;
  payload["time"] = 1351824120;
  payload["loadCellValue"] = value;

  // Convert payload to a json String and then into a char array for MQTT transport
  String jsonString;
  serializeJson(payload, jsonString);
  char mqttPayload[jsonString.length()+1];
  jsonString.toCharArray(mqttPayload, jsonString.length()+1);

  // Debug print the MQTT json payload
  Serial.println(mqttPayload);

  // Publish
  uint16_t packetIdPub1 = mqttClient.publish(topic, 1, true, mqttPayload);
  Serial.println("[MQTT]: Publish sensor data via MQTT");

  if (MQTT_STATUS) {
    Serial.println("[MQTT]: STATUS OK");    
  } else {
    Serial.println("[MQTT]: STATUS NOT!! OK");
  }
}



/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/



/**
 * MQTT connect
 */
void connectToMqtt() {
  Serial.println("[MQTT]: Connecting to MQTT...");
  mqttClient.connect();
}

/**
 * MQTT is connected
 */
void onMqttConnect(bool sessionPresent) {
  Serial.println("[MQTT]: Connected to MQTT");
  MQTT_STATUS = true;

  // uint16_t packetIdSub = mqttClient.subscribe("esp8266/sensor/SENSOR_NAME/command/ping", 0);
  // Serial.print("Subscribing to COMMAND topic");
}

/**
 * 
 */
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT");
  String text;
  switch( reason) {
  case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
     text = "TCP_DISCONNECTED"; 
     break; 
  case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
     text = "MQTT_UNACCEPTABLE_PROTOCOL_VERSION"; 
     break; 
  case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
     text = "MQTT_IDENTIFIER_REJECTED";  
     break;
  case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE: 
     text = "MQTT_SERVER_UNAVAILABLE"; 
     break;
  case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
     text = "MQTT_MALFORMED_CREDENTIALS"; 
     break;
  case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
     text = "MQTT_NOT_AUTHORIZED"; 
     break;
  }

  Serial.printf(" [%8u] Disconnected from the broker reason = %s\n", millis(), text.c_str() );

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
    MQTT_STATUS = false;
  }
}

/**
 * MQTT receive a Message
 */
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.println(payload);
}


/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/


/**
 * WiFi connect
 */
void connectToWifi() {
  Serial.println("[WiFi]: Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

/**
 * WiFi is connected
 */
void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("[WiFi]: Connected to Wi-Fi.");
  WIFI_STATUS = true;
  connectToMqtt();
}

/**
 * WiFi is disconnected
 */
void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("[WiFi]: Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}
