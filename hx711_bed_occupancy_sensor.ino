/*******************************************************************************************
* Basic starter template as a good point to start a new script
* Features:
*   - WiFi connection/ reconnection
*   - MQTT connection/ reconnection
*   - Publish and subscrice to MQTT topics to send data or listen for remote calls
* Created for a ESP8266/ NodeMCU project
* 
* Attention: !!! THIS IS WORK IN PROGRESS !!!
* 
* ******************************************************************************************
*/


/** YOUR CUSTOM SENSOR CODE **/
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/
// ...
// ...
// ...
// ...
/** HX711 Load Cell Sensor **/
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/
#include <HX711_ADC.h>
const int HX711_DATA = D5;
const int HX711_SLCK = D6;

HX711_ADC LoadCell(HX711_DATA, HX711_SLCK);

 

/** Some libraries **/
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/
#include <ArduinoJson.h>

/** WLAN **/
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/
#include <ESP8266WiFi.h>
#include <Ticker.h>

#define WIFI_SSID "YOU_WLAN_SSID"
#define WIFI_PASSWORD "YOUR_WLAN_PASSWORD"

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;



/** MQTT **/
/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/
#include <AsyncMqttClient.h>

#define MQTT_HOST IPAddress(192, 168, 2, 83)
#define MQTT_PORT 1883
static const char mqttUser[] = "YOUR_USER";
static const char mqttPassword[] = "YOUR_PASSWORD";

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;




/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/

static bool WIFI_STATUS = false;
static bool MQTT_STATUS = false;

static const char MQTT_PUBLISH_TOPIC[] = "esp8266/sensor/bed-occupancy/weight";
static const char MQTT_SENSOR_NAME[] = "esp-bed-ocupancy";
static const char MQTT_NAME[] = "Bed occupancy weight sensor";


/**
 * SETUP
 */
void setup() {
  Serial.begin(9600);
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
  connectToWifi();
  Serial.println("WiFi connected!");


  
  // YOUR CUSTOM SENSOR INITIALIZATION STFF
  // ---    ---    ---    ---    ---    ---    ---    ---    ---    ---    ---    
  // ...
  // ...
  // ...
  // ...
  LoadCell.begin();
  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("LoadCell: Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else {
    LoadCell.setCalFactor(1.0); // user set calibration value (float), initial value 1.0 may be used for this sketch
    Serial.println("LoadCell: Startup is complete");
  }
  while (!LoadCell.update());
  //calibrate(); //start calibration procedure
  //Serial.println("LoadCell: calibrate");

 
  
  Serial.println("== [SETUP]: completed ==");
}




/** -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- **/

/**
 * LOOP
 */
void loop() {
  // GET DATA FROM YOUR SENSOR
  // ---    ---    ---    ---    ---    ---    ---    ---    ---    ---    ---    
  const int SENSOR_DATA = 123455;
  // ...
  // ...
  // ...
  // ...

  const int serialPrintInterval = 2; //increase value to slow down serial print activity
  bool newDataReady = false;
  float loadCellWeight;

  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
      loadCellWeight = LoadCell.getData();
      Serial.print("Load_cell output val: ");
      Serial.println(loadCellWeight);
  }

  delay(300);
  return;


  // Publish sensor data via MQTT
  StaticJsonDocument<400> payload;
  payload["sensor"] = MQTT_SENSOR_NAME;
  payload["name"] = MQTT_NAME;
  payload["time"] = 1351824120;
  payload["loadCellWeight"] = loadCellWeight;

  // Convert payload to a json String and then into a char array for MQTT transport
  String jsonString;
  serializeJson(payload, jsonString);
  char mqttPayload[jsonString.length()+1];
  jsonString.toCharArray(mqttPayload, jsonString.length()+1);

  // Debug print the MQTT json payload
  Serial.println(mqttPayload);

  // Publish
  uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUBLISH_TOPIC, 1, true, mqttPayload);
  Serial.println("[MQTT]: Publish sensor data via MQTT");

  delay(1000 * 2);
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

  uint16_t packetIdSub = mqttClient.subscribe("esp8266/sensor/SENSOR_NAME/command/ping", 0);
  Serial.print("Subscribing to COMMAND topic");
}

/**
 * 
 */
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT");

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
