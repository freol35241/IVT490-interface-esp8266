#include "config.h"

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Ticker.h>
#include <ReactESP.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <DebugLog.h>

#include "IVT490.h"
#include "Controller.h"

// Firmware updates. Both paths below write a new image, and both take this
// password; there is deliberately no way to build without one.
//
// ArduinoOTA (espota) has the board connect BACK to the uploading host, which
// a firewall between the board's network and the host's blocks. The HTTP
// update server is host-initiated, so it works wherever the board's port 80
// is reachable:
//   curl -u "admin:$OTA_PASSWORD" -F "firmware=@.pio/build/d1_mini_lite/firmware.bin" \
//        http://<board>/update
#ifndef OTA_PASSWORD
#error "Define OTA_PASSWORD in config.h: it guards both update paths"
#endif
#ifndef OTA_USERNAME
#define OTA_USERNAME "admin"
#endif

// MQTT robustness settings. Each can be overridden in config.h.
//
// PubSubClient builds every packet whole in this buffer before writing it,
// which is the point of using it: AsyncMqttClient wrote packets into the TCP
// send buffer piecemeal, and a reconnect's SUBSCRIBE could land inside a
// half-written PUBLISH (issue #29). The buffer must hold the largest packet:
// the whole-document /ivt490/state blob plus its topic.
#ifndef MQTT_BUFFER_SIZE
#define MQTT_BUFFER_SIZE 2048
#endif
#ifndef MQTT_KEEPALIVE_S
#define MQTT_KEEPALIVE_S 15 // AsyncMqttClient's default, kept
#endif
#ifndef MQTT_RECONNECT_INTERVAL
#define MQTT_RECONNECT_INTERVAL 5000 // milliseconds between connection attempts
#endif
// Restart if Wi-Fi or MQTT has been down this long. Checked from loop() with
// millis(), so it does not depend on the ReactESP scheduler.
#ifndef CONNECTIVITY_WATCHDOG_TIMEOUT
#define CONNECTIVITY_WATCHDOG_TIMEOUT (5UL * 60 * 1000) // milliseconds
#endif
// The daily restart, now checked from loop() as well.
#ifndef DAILY_RESTART_INTERVAL
#define DAILY_RESTART_INTERVAL (24UL * 3600 * 1000) // milliseconds
#endif
#ifndef DIAGNOSTICS_PUBLISH_INTERVAL
#define DIAGNOSTICS_PUBLISH_INTERVAL 60000 // milliseconds
#endif

reactesp::ReactESP app;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

// Topic prefix and client id, built once. MQTT_BASE_TOPIC is a String in
// config.h; copying it here keeps config.h unchanged.
char base_topic[96];
char client_id[24];

// Connection bookkeeping
uint32_t last_mqtt_attempt = 0;
uint32_t last_connected = 0;
uint32_t mqtt_connects = 0;
uint32_t publish_failures = 0;
uint32_t oversize_skips = 0;

// IVT490 serial connection
SoftwareSerial ivtSerial(IVT490_SERIAL_RX);
bool IVT490_serial_connection_is_initialized = false;

// Global state
struct AppState {
  IVT490::State ivt490;
} app_state;

// Thermistor readers
IVT490::ThermistorReader GT2_reader(IVT490_ADC_CS, 0, IVT490_ADC_CH0_R0);
IVT490::ThermistorReader GT3_2_reader(IVT490_ADC_CS, 1, IVT490_ADC_CH1_R0);

// Thermistor emulators
IVT490::ThermistorEmulator GT2_emulator(IVT490_DIGIPOT_GT2_CS, IVT490_DIGIPOT_GT2_RESOLUTION, IVT490_DIGIPOT_GT2_MAX_RESISTANCE);
IVT490::ThermistorEmulator GT3_2_emulator(IVT490_DIGIPOT_GT32_CS, IVT490_DIGIPOT_GT32_RESOLUTION, IVT490_DIGIPOT_GT32_MAX_RESISTANCE);

// Controller
Controller controller(GENERAL_CONTROL_VALUES_VALIDITY, IVT490_HEATING_CURVE_SLOPE);

// Publish one whole packet, or nothing.
//
// PubSubClient refuses an oversize packet before writing anything, so that
// case is checked here and skipped. Any other failure means the write to the
// socket fell short. The stream may then hold part of a packet, so the
// connection is dropped and rebuilt rather than written to again: the broker
// discards a partial packet when the connection closes, instead of reading
// the next packet as its tail.
bool mqtt_publish(const char *topic, const char *payload, bool retained = false)
{
  if (!mqttClient.connected())
  {
    return false;
  }

  size_t needed = 5 + 2 + strlen(topic) + strlen(payload); // header + topic length + topic + payload
  if (needed > MQTT_BUFFER_SIZE)
  {
    oversize_skips++;
    LOG_ERROR("Skipping publish larger than MQTT_BUFFER_SIZE on topic: ", topic);
    return false;
  }

  if (!mqttClient.publish(topic, payload, retained))
  {
    publish_failures++;
    LOG_ERROR("Publish failed, dropping the connection to resync: ", topic);
    mqttClient.disconnect();
    return false;
  }
  return true;
}

void connectToWifi()
{
  LOG_INFO("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PW);
}

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
  LOG_INFO("Connected to Wi-Fi.");
  last_mqtt_attempt = 0; // connect to MQTT at the next loop() rather than waiting
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
  LOG_INFO("Disconnected from Wi-Fi.");
  wifiReconnectTimer.once(2, connectToWifi);
}

void subscribe(const char *suffix)
{
  char topic[160];
  snprintf(topic, sizeof(topic), "%s%s", base_topic, suffix);
  mqttClient.subscribe(topic, 0);
}

// Called from loop(), never from a callback, so nothing else can be writing
// to the socket while the connection and its subscriptions are set up.
void connectToMqtt()
{
  LOG_INFO("Connecting to MQTT...");
  if (!mqttClient.connect(client_id, MQTT_USER, MQTT_PW))
  {
    LOG_ERROR("MQTT connection failed, state: ", mqttClient.state());
    return;
  }

  mqtt_connects++;
  LOG_INFO("Connected to MQTT.");
  subscribe("/controller/set/feed_temperature_target");
  subscribe("/controller/set/indoor_temperature_target");
  subscribe("/controller/set/indoor_temperature_actual");
  subscribe("/controller/set/outdoor_temperature_offset");
  subscribe("/controller/set/operating_mode");
}

bool topic_ends_with(const char *topic, const char *suffix)
{
  size_t topic_len = strlen(topic);
  size_t suffix_len = strlen(suffix);
  return topic_len >= suffix_len && strcmp(topic + topic_len - suffix_len, suffix) == 0;
}

void onMqttMessage(char *topic, byte *payload_bytes, unsigned int length)
{
  LOG_INFO("MQTT message received on topic: ", topic);
  LOG_DEBUG("  len: ", length);

  // PubSubClient's payload is not NUL-terminated. Every value handled here is
  // a short number, so a bounded copy is enough.
  char payload[32];
  size_t n = length < sizeof(payload) - 1 ? length : sizeof(payload) - 1;
  memcpy(payload, payload_bytes, n);
  payload[n] = '\0';

  // A value of 0 is rejected below as a parse failure, as before: String's
  // toFloat()/toInt(), which this replaces, return 0 for unparseable input.
  if (topic_ends_with(topic, "/controller/set/feed_temperature_target"))
  {
    float value = atof(payload);

    if (value == 0)
    {
      LOG_ERROR("Failed to parse payload as a float");
      return;
    }

    controller.set_feed_temperature_target(value);
  }
  else if (topic_ends_with(topic, "/controller/set/outdoor_temperature_offset"))
  {
    float value = atof(payload);

    if (value == 0)
    {
      LOG_ERROR("Failed to parse payload as a float");
      return;
    }

    controller.set_outdoor_temperature_offset(value);
  }
  else if (topic_ends_with(topic, "/controller/set/indoor_temperature_target"))
  {
    float value = atof(payload);

    if (value == 0)
    {
      LOG_ERROR("Failed to parse payload as a float");
      return;
    }

    controller.set_indoor_temperature_target(value);
  }
  else if (topic_ends_with(topic, "/controller/set/indoor_temperature_actual"))
  {
    float value = atof(payload);

    if (value == 0)
    {
      LOG_ERROR("Failed to parse payload as a float");
      return;
    }

    controller.set_indoor_temperature(value);
  }
  else if (topic_ends_with(topic, "/controller/set/operating_mode"))
  {
    long value = atol(payload);

    if (value == 0)
    {
      LOG_ERROR("Failed to parse payload as a int");
      return;
    }
    else if (value > 3 || value < 1){
      LOG_ERROR("Could not cast to an OperatingMode:", value);
      return;
    }

    controller.set_operating_mode(static_cast<OperatingMode>(value));
  }
  else
  {
    LOG_ERROR("Received MQTT message on topic which we do not know how to handle:", topic);
  }
}

// The whole-document blob for the outermost call. Static so it is not a
// fresh heap allocation every publish interval.
char json_buffer[MQTT_BUFFER_SIZE];

// One document for both state publishes, which run one after the other.
// Static for the same reason: it replaces a 4096- and a 1024-byte heap
// allocation every GENERAL_STATE_PUBLISH_INTERVAL. Sized from the members
// the serializers write, with headroom; a fill that overflows is skipped and
// counted in oversize_skips rather than published short.
constexpr size_t STATE_DOC_CAPACITY =
    2 * JSON_OBJECT_SIZE(IVT490::State::STATE_DOC_SLOTS > Controller::STATE_DOC_SLOTS
                             ? IVT490::State::STATE_DOC_SLOTS
                             : Controller::STATE_DOC_SLOTS);
StaticJsonDocument<STATE_DOC_CAPACITY> state_doc;
size_t state_doc_peak = 0; // largest memoryUsage() seen, for diagnostics

void publish_json_object(char *topic, size_t capacity, const JsonObject root);

void publish_state_doc(char *topic, size_t capacity)
{
  state_doc_peak = max(state_doc_peak, state_doc.memoryUsage());
  if (state_doc.overflowed())
  {
    oversize_skips++;
    LOG_ERROR("State document overflowed, skipped: ", topic);
    return;
  }
  publish_json_object(topic, capacity, state_doc.as<JsonObject>());
}

// One IVT490 sentence, gathered without blocking. A line that does not fit is
// dropped whole (counted in serial_overflows) rather than parsed in pieces.
// readStringUntil() built a String per line and blocked up to its 1 s
// timeout, returning a fragment if the line arrived slower than that.
#ifndef SERIAL_LINE_BUFFER_SIZE
#define SERIAL_LINE_BUFFER_SIZE 512
#endif
char serial_line[SERIAL_LINE_BUFFER_SIZE];
size_t serial_line_len = 0;
bool serial_line_overflowed = false;
uint32_t serial_overflows = 0;

// Publish `root` as one JSON blob on `topic`, then each leaf on its own
// subtopic, recursing into nested objects (which also get their own blob).
// The topic tree and the payload text are unchanged from the String-based
// version: serializeJson() of a number or bool gives the same text as
// JsonVariant::as<String>() did.
//
// `topic` is a buffer of `capacity` bytes; subtopics are appended in place
// and the original length restored afterwards, so no String is built.
void publish_json_object(char *topic, size_t capacity, const JsonObject root)
{
  if (!mqttClient.connected())
  {
    return;
  }

  // Publish the whole state as a single JSON blob
  if (measureJson(root) < sizeof(json_buffer))
  {
    serializeJson(root, json_buffer, sizeof(json_buffer));
    LOG_DEBUG(json_buffer);
    mqtt_publish(topic, json_buffer);
  }
  else
  {
    oversize_skips++;
    LOG_ERROR("Skipping JSON blob larger than its buffer on topic: ", topic);
  }

  // Publish on individual topics
  size_t topic_len = strlen(topic);
  char value_text[32];
  for (auto pair : root)
  {
    if (!mqttClient.connected())
    {
      break; // a failed publish dropped the connection; the rest would fail too
    }

    const char *key = pair.key().c_str();
    auto value = pair.value();

    int written = snprintf(topic + topic_len, capacity - topic_len, "/%s", key);
    if (written < 0 || (size_t)written >= capacity - topic_len)
    {
      topic[topic_len] = '\0';
      LOG_ERROR("Subtopic too long, skipped: ", key);
      continue;
    }

    if (value.is<JsonObject>())
    {
      publish_json_object(topic, capacity, value.as<JsonObject>());
    }
    else
    {
      serializeJson(value, value_text, sizeof(value_text));
      LOG_DEBUG(key, value_text);
      mqtt_publish(topic, value_text);
    }

    topic[topic_len] = '\0';
  }
}

// Board health, on its own subtree so nothing that reads ivt490/state or
// controller/state sees it. Free heap and fragmentation are what issue #29
// needs to see; reset_reason says whether a restart was ours.
void publish_diagnostics()
{
  if (!mqttClient.connected())
  {
    return;
  }

  char topic[160];
  char value[48];
  struct
  {
    const char *name;
    uint32_t value;
  } numbers[] = {
      {"free_heap", ESP.getFreeHeap()},
      {"max_free_block", ESP.getMaxFreeBlockSize()},
      {"heap_fragmentation", ESP.getHeapFragmentation()},
      {"uptime_s", millis() / 1000},
      {"mqtt_connects", mqtt_connects},
      {"publish_failures", publish_failures},
      {"oversize_skips", oversize_skips},
      {"serial_overflows", serial_overflows},
      {"state_doc_peak", state_doc_peak},
  };
  for (auto &n : numbers)
  {
    snprintf(topic, sizeof(topic), "%s/diagnostics/%s", base_topic, n.name);
    snprintf(value, sizeof(value), "%lu", (unsigned long)n.value);
    mqtt_publish(topic, value);
  }

  snprintf(topic, sizeof(topic), "%s/diagnostics/wifi_rssi", base_topic);
  snprintf(value, sizeof(value), "%d", WiFi.RSSI());
  mqtt_publish(topic, value);

  snprintf(topic, sizeof(topic), "%s/diagnostics/reset_reason", base_topic);
  strlcpy(value, ESP.getResetReason().c_str(), sizeof(value));
  mqtt_publish(topic, value);
}

void setup()
{
  Serial.begin(115200);

  // Disable vacation mode on boot
  pinMode(IVT490_EXT_IN_RELAY_PIN, OUTPUT);
  digitalWrite(IVT490_EXT_IN_RELAY_PIN, LOW);

  strlcpy(base_topic, String(MQTT_BASE_TOPIC).c_str(), sizeof(base_topic));
  // The id AsyncMqttClient generated, so the broker sees the same client.
  snprintf(client_id, sizeof(client_id), "esp8266-%06x", ESP.getChipId());

  // IVT490 serial connection
  // Baud rate = 9600
  // IVT490 should output every 60th second
  ivtSerial.begin(9600);

  // Attach wifi handlers
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  // MQTT
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(onMqttMessage);
  mqttClient.setKeepAlive(MQTT_KEEPALIVE_S);
  if (!mqttClient.setBufferSize(MQTT_BUFFER_SIZE))
  {
    LOG_ERROR("Could not allocate the MQTT buffer");
  }

  // Connect to wifi; loop() connects to the broker once Wi-Fi is up
  connectToWifi();

  // Configure controller
  controller.set_indoor_temperature_target(20.0);
  controller.set_indoor_temperature_weight(IVT490_INDOOR_TEMPERATURE_FEEDBACK_CONTROL_WEIGHT);
  controller.set_summer_temperature_limit(IVT490_SUMMER_TEMPERATURE_LIMIT);

  // Read ADCs continuously
  app.onRepeat(IVT490_ADC_SAMPLING_INTERVAL, []()
               {
                // Read into app_state
                 GT2_reader.read(app_state.ivt490.GT2);
                 GT3_2_reader.read(app_state.ivt490.GT3_2);
                });


  // Run control code
  app.onRepeat(IVT490_CONTROL_INTERVAL, []()
               {
                 LOG_DEBUG("Running control code...");

                 auto control_values = controller.get_control_values(app_state.ivt490);

                 // Control feed temperature
                 GT2_emulator.set_target_value(control_values.GT2);

                 // Make sure EXT_IN relay is in correct position
                 digitalWrite(IVT490_EXT_IN_RELAY_PIN, control_values.enable_vacation_mode);

                 // Control boiler (GT3_2 emulator)
                  GT3_2_emulator.set_target_value(control_values.GT3_2);
 });

  // Board diagnostics
  app.onRepeat(DIAGNOSTICS_PUBLISH_INTERVAL, publish_diagnostics);

  // Serial listener to IVT490
  app.onAvailable(ivtSerial, []()
                  {
                    bool complete = false;
                    while (ivtSerial.available() > 0)
                    {
                      char c = ivtSerial.read();
                      if (c == '\n')
                      {
                        complete = true;
                        break;
                      }
                      if (serial_line_len < sizeof(serial_line) - 1)
                      {
                        serial_line[serial_line_len++] = c;
                      }
                      else
                      {
                        serial_line_overflowed = true;
                      }
                    }
                    if (!complete)
                    {
                      return;
                    }
                    serial_line[serial_line_len] = '\0';
                    serial_line_len = 0;
                    if (serial_line_overflowed)
                    {
                      serial_line_overflowed = false;
                      serial_overflows++;
                      LOG_ERROR("Serial line from IVT490 longer than its buffer, dropped");
                      return;
                    }
                    const char *raw = serial_line;
                    LOG_INFO("Received serial data from IVT490:", raw);

                    LOG_INFO("Publishing raw output to MQTT broker...");
                    char raw_topic[160];
                    snprintf(raw_topic, sizeof(raw_topic), "%s/ivt490/raw", base_topic);
                    mqtt_publish(raw_topic, raw);

                    if (parse_serial(raw, app_state.ivt490.serial) < 0)
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
                                      if (!mqttClient.connected())
                                      {
                                        return;
                                      }

                                      LOG_INFO("Publishing to MQTT broker...");
                                      char topic[160];

                                      // IVT490 state
                                      app_state.ivt490.serialize(state_doc);
                                      snprintf(topic, sizeof(topic), "%s/ivt490/state", base_topic);
                                      publish_state_doc(topic, sizeof(topic));

                                      // Controller state
                                      controller.serialize(state_doc);
                                      snprintf(topic, sizeof(topic), "%s/controller/state", base_topic);
                                      publish_state_doc(topic, sizeof(topic));
                                      });
                    }

                    LOG_INFO("Adjusting thermistor emulator corrections");
                    GT2_emulator.adjust_correction(app_state.ivt490.serial.GT2);
                    GT3_2_emulator.adjust_correction(app_state.ivt490.serial.GT3_2); });

  // Configure OTA
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();
  httpUpdater.setup(&httpServer, "/update", OTA_USERNAME, OTA_PASSWORD);
  httpServer.begin();
  app.onTick([]()
             {
               ArduinoOTA.handle();
               httpServer.handleClient(); });

  LOG_INFO("Setup complete, waiting for serial connection to IVT490 to initialize...");
}

// The MQTT connection and both restarts live here rather than in ReactESP
// callbacks. On 2026-10-31 the board dropped off the network for 17.7 h and
// its scheduled daily restart never fired; these checks need nothing but
// loop() running and millis().
void maintain_connectivity()
{
  uint32_t now = millis();

  if (WiFi.isConnected())
  {
    if (mqttClient.connected())
    {
      mqttClient.loop();
    }
    else if (last_mqtt_attempt == 0 || now - last_mqtt_attempt >= MQTT_RECONNECT_INTERVAL)
    {
      last_mqtt_attempt = now;
      connectToMqtt();
    }
  }

  if (WiFi.isConnected() && mqttClient.connected())
  {
    last_connected = now;
  }
  else if (now - last_connected >= CONNECTIVITY_WATCHDOG_TIMEOUT)
  {
    LOG_ERROR("Wi-Fi or MQTT down beyond the watchdog timeout, restarting");
    ESP.restart();
  }

  // Reset once a day to avoid mysterious fails...
  if (now >= DAILY_RESTART_INTERVAL)
  {
    ESP.restart();
  }
}

void loop()
{
  maintain_connectivity();
  app.tick();
}
