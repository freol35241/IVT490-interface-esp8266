#include "config.h"

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ReactESP.h>
#include <AsyncMqttClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <DebugLog.h>

#include "IVT490.h"
#include "SMA.h"

reactesp::ReactESP app;

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

// IVT490 serial connection
SoftwareSerial ivtSerial(IVT490_SERIAL_TX, IVT490_SERIAL_RX);

// Global states
IVT490::IVT490State vp_state;

// Thermistor reader
IVT490::IVT490ThermistorReader<IVT490_R0_RESISTANCE> GT2_reader(IVT490_ADC_CS, 0);
SMA::Filter<float, 10> filter;

// Thermistor emulator
IVT490::IVT490ThermistorEmulator<8, 100000> GT2_emulator(&vp_state.GT2_heatpump, IVT490_THERMISTOR_EMULATOR_CS);
unsigned long GT1_control_value_last_received_at = 0;

void connectToWifi()
{
  LOG_INFO("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PW);
}

void connectToMqtt()
{
  LOG_INFO("Connecting to MQTT...");
  mqttClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
  LOG_INFO("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
  LOG_INFO("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttConnect(bool sessionPresent)
{
  LOG_INFO("Connected to MQTT.");
  mqttClient.subscribe((MQTT_BASE_TOPIC + "/indoor_temperature").c_str(), 0);
  mqttClient.subscribe((MQTT_BASE_TOPIC + "/indoor_temperature_set").c_str(), 0);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  LOG_INFO("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  LOG_INFO("Subscribe acknowledged.");
  LOG_DEBUG("  packetId: ", packetId);
  LOG_DEBUG("  qos: ", qos);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  LOG_INFO("MQTT message received on topic: ", topic);
  LOG_DEBUG("  qos: ", properties.qos);
  LOG_DEBUG("  dup: ", properties.dup);
  LOG_DEBUG("  retain: ", properties.retain);
  LOG_DEBUG("  len: ", len);
  LOG_DEBUG("  index: ", index);
  LOG_DEBUG("  total: ", total);

  static float GT2_target;

  if (String(topic).endsWith("control/GT1_target"))
  {
    auto value = String(payload).toFloat();

    if (value == 0)
    {
      LOG_ERROR("Failed to parse payload as a float");
      return;
    }

    LOG_INFO("New GT1_target value: ", value);
    GT1_control_value_last_received_at = millis();

    GT2_target = IVT490::inverse_heating_curve(
        IVT490_HEATING_CURVE_SLOPE,
        value);
    LOG_INFO("  New GT2_target value: ", GT2_target);

    GT2_emulator.update_target_ptr(&GT2_target);
  }
  else
  {
    LOG_ERROR("Received MQTT message on topic which we do not know how to handle. This should not happen!");
  }
}

void onMqttPublish(uint16_t packetId)
{
  LOG_INFO("Publish acknowledged.");
  LOG_ERROR("  packetId: ", packetId);
}

void setup()
{
  Serial.begin(115200);

  // IVT490 serial connection
  // Baud rate = 9600
  // IVT490 should output every 60th second
  ivtSerial.begin(9600);

  // Attach wifi handlers
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  // MQTT handlers
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  // Set connection details
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  mqttClient.setCredentials(MQTT_USER, MQTT_PW);

  // Connect to wifi and subsequently to mqtt broker
  connectToWifi();

  // Initialize emulator to follow sensor value
  GT2_emulator.update_target_ptr(&vp_state.GT2_sensor);

  // Read ADCs continuously
  app.onRepeat(1000, []()
               {
                 LOG_INFO("Reading ADCs...");
                 auto value = GT2_reader.read();
                 LOG_DEBUG("    GT2_sensor: ", value);
                 auto filtered_value = filter(value);
                 LOG_DEBUG("    GT2_sensor (filtered): ", filtered_value);
                 vp_state.GT2_sensor = filtered_value; });

  app.onRepeat(1000, []()
               {
                 if (millis() - GT1_control_value_last_received_at > IVT490_CONTROL_VALUES_VALIDITY)
                 {
                   GT2_emulator.update_target_ptr(&vp_state.GT2_sensor);
                 } });

  // Serial listener to IVT490
  app.onAvailable(ivtSerial, []()
                  {
                    auto raw = ivtSerial.readStringUntil('\n');
                    LOG_INFO("Received serial data from IVT490: ", raw);

                    if (parse_IVT490(raw, vp_state) < 0)
                    {
                      LOG_ERROR("Failed parsing serial message from IVT490!");
                      return;
                    }

                    LOG_INFO("Successfully parsed serial message from IVT490.");

                    LOG_INFO("Updating thermistor emulator");
                    GT2_emulator.adjust();

                    auto doc = IVT490::serialize_IVT490State(vp_state);

                    LOG_INFO("Publishing to MQTT broker...");

                    // Publish the whole state as a single JSON blob
                    String json;
                    serializeJson(doc, json);

                    LOG_DEBUG(json);

                    mqttClient.publish(
                        (MQTT_BASE_TOPIC + String("/state")).c_str(),
                        0,
                        false,
                        json.c_str());

                    // Publish on individual topics
                    JsonObject root = doc.as<JsonObject>();
                    for (auto pair : root)
                    {
                      LOG_DEBUG(pair.key().c_str(), pair.value().as<String>());

                      mqttClient.publish(
                          (MQTT_BASE_TOPIC + String("/state") + pair.key().c_str()).c_str(),
                          0,
                          false,
                          pair.value().as<String>().c_str());
                    } });

  LOG_INFO("Setup complete, app started successfully...");
}

void loop()
{
  app.tick();
}