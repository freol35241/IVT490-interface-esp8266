#include "config.h"

#include <ArduinoOTA.h>
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
SoftwareSerial ivtSerial(IVT490_SERIAL_RX);
bool IVT490_serial_connection_is_initialized = false;

// Global states
IVT490::IVT490State vp_state;

// Thermistor reader
IVT490::IVT490ThermistorReader<IVT490_ADC_R0> GT2_reader(IVT490_ADC_CS, 0);
SMA::Filter<float, IVT490_ADC_FILTER_WINDOW_COUNT> filter;

// Thermistor emulator
IVT490::IVT490ThermistorEmulator<IVT490_DIGPOT_RESOLUTION, IVT490_DIGIPOT_MAX_RESISTANCE> GT2_emulator(IVT490_DIGIPOT_CS);

// Controller
IVT490::Controller<GENERAL_CONTROL_VALUES_VALIDITY> controller;

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
  mqttClient.subscribe((MQTT_BASE_TOPIC + "/controller/set/feed_temperature_target").c_str(), 0);
  mqttClient.subscribe((MQTT_BASE_TOPIC + "/controller/set/indoor_temperature_target").c_str(), 0);
  mqttClient.subscribe((MQTT_BASE_TOPIC + "/controller/set/outdoor_temperature_offset").c_str(), 0);
  mqttClient.subscribe((MQTT_BASE_TOPIC + "/controller/feedback/indoor_temperature").c_str(), 0);
  mqttClient.subscribe((MQTT_BASE_TOPIC + "/controller/set/vacation_mode").c_str(), 0);
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

  if (String(topic).endsWith("/controller/set/feed_temperature_target"))
  {
    auto value = String(payload).toFloat();

    if (value == 0)
    {
      LOG_ERROR("Failed to parse payload as a float");
      return;
    }

    controller.set_feed_temperature_target(value);
  }
  else if (String(topic).endsWith("/controller/set/outdoor_temperature_offset"))
  {
    auto value = String(payload).toFloat();

    if (value == 0)
    {
      LOG_ERROR("Failed to parse payload as a float");
      return;
    }

    controller.set_outdoor_temperature_offset(value);
  }
  else if (String(topic).endsWith("/controller/set/indoor_temperature_target"))
  {
    auto value = String(payload).toFloat();

    if (value == 0)
    {
      LOG_ERROR("Failed to parse payload as a float");
      return;
    }

    controller.set_indoor_temperature_target(value);
  }
  else if (String(topic).endsWith("/controller/feedback/indoor_temperature"))
  {
    auto value = String(payload).toFloat();

    if (value == 0)
    {
      LOG_ERROR("Failed to parse payload as a float");
      return;
    }

    controller.set_indoor_temperature(value);
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

void publish_json_object(String &topic, DynamicJsonDocument &doc)
{
  // Publish the whole state as a single JSON blob
  String json;
  serializeJson(doc, json);

  LOG_DEBUG(json);

  mqttClient.publish(
      (topic).c_str(),
      0,
      false,
      json.c_str());

  // Publish on individual topics
  JsonObject root = doc.as<JsonObject>();
  for (auto pair : root)
  {
    LOG_DEBUG(pair.key().c_str(), pair.value().as<String>());

    mqttClient.publish(
        (topic + String("/") + pair.key().c_str()).c_str(),
        0,
        false,
        pair.value().as<String>().c_str());
  }
}

void setup()
{
  Serial.begin(115200);

  // Disable vacation mode on boot
  pinMode(IVT490_EXT_IN_RELAY_PIN, OUTPUT);
  digitalWrite(IVT490_EXT_IN_RELAY_PIN, LOW);

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

  // Configure controller
  controller.set_heating_curve_slope(IVT490_HEATING_CURVE_SLOPE);
  controller.set_indoor_temperature_target(20.0);
  controller.set_indoor_temperature_weight(IVT490_INDOOR_TEMPERATURE_FEEDBACK_CONTROL_WEIGHT);
  controller.set_summer_temperature_limit(IVT490_SUMMER_TEMPERATURE_LIMIT);

  // Read ADCs continuously
  app.onRepeat(IVT490_ADC_SAMPLING_INTERVAL, []()
               {
                 LOG_DEBUG("Reading ADCs...");
                 auto value = GT2_reader.read();
                 LOG_DEBUG("    GT2_sensor: ", value);
                 auto filtered_value = filter(value);
                 LOG_DEBUG("    GT2_sensor (filtered): ", filtered_value);
                 vp_state.GT2_sensor = filtered_value;
                 controller.set_outdoor_temperature(filtered_value); });

  // Run control code
  app.onRepeat(IVT490_CONTROL_INTERVAL, []()
               {
                 LOG_DEBUG("Running control code...");

                auto [control_value, vacation_mode] = controller.get_control_values();

                 // Set the control value
                 GT2_emulator.set_target_value(control_value);

                 // Make sure EXT_IN relay is in correct position
                 digitalWrite(IVT490_EXT_IN_RELAY_PIN, vacation_mode); });

  // Serial listener to IVT490
  app.onAvailable(ivtSerial, []()
                  {
                    auto raw = ivtSerial.readStringUntil('\n');
                    LOG_INFO("Received serial data from IVT490:", raw);

                    LOG_INFO("Publishing raw output to MQTT broker...");
                    mqttClient.publish(
                        (MQTT_BASE_TOPIC + String("/state/raw")).c_str(),
                        0,
                        false,
                        raw.c_str());

                    if (parse_IVT490(raw, vp_state) < 0)
                    {
                      LOG_ERROR("Failed parsing serial message from IVT490!");
                      return;
                    }

                    LOG_INFO("Successfully parsed serial message from IVT490.");

                    if (!IVT490_serial_connection_is_initialized){
                      IVT490_serial_connection_is_initialized = true;
                      LOG_INFO("Serial connection to IVT490 initialized correctly, enabling state publishing");
                      app.onRepeat(GENERAL_STATE_PUBLISH_INTERVAL, []
                                  {
                                      LOG_INFO("Publishing to MQTT broker...");

                                      // IVT490 state
                                      auto doc = IVT490::serialize_IVT490State(vp_state);
                                      auto topic = MQTT_BASE_TOPIC + String("/state");
                                      publish_json_object(topic, doc);

                                      doc.clear();

                                      // Controller state
                                      doc = controller.serialize();
                                      topic = MQTT_BASE_TOPIC + String("/controller/state");
                                      publish_json_object(topic, doc);
                                      });
                    }

                    LOG_INFO("Adjusting thermistor emulator corrections");
                    GT2_emulator.adjust_correction(vp_state.GT2_heatpump); });

  // Configure OTA
  ArduinoOTA.setHostname(OTA_HOSTNAME);
#ifdef OTA_PASSWORD
  ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
  ArduinoOTA.begin();
  app.onTick([]()
             { ArduinoOTA.handle(); });

  // Reset once a day to avoid mysterious fails...
  app.onDelay(24 * 3600 * 1000, ESP.restart);

  LOG_INFO("Setup complete, waiting for serial connection to IVT490 to initialize...");
}

void loop()
{
  app.tick();
}
