
This directory is intended for project header files.

More specifically, this project requires a header file named `config.h` to be placed in this directory with the following contents (replace as applicable):

```cpp
#ifndef CONFIG_H
#define CONFIG_H

#define GENERAL_CONTROL_VALUES_VALIDITY 360 * 1000
#define GENERAL_STATE_PUBLISH_INTERVAL 10000 // milliseconds

#define WIFI_SSID "YOUR WIFI SSID"
#define WIFI_PW "YOUR WIFI PASSWORD"

#define OTA_HOSTNAME "esp8266-ivt490"
// #define OTA_PASSWORD "YOUR-PASSWORD"

#define MQTT_HOST IPAddress(1, 1, 1, 1)
#define MQTT_PORT 1883
#define MQTT_USER "YOUR MQTT USERNAME"
#define MQTT_PW "YOUR MQTT PASSWORD"
#define MQTT_BASE_TOPIC String("BASE/TOPIC/FOR/PUBLISHING")

#define IVT490_SERIAL_RX 5                   // D1
#define IVT490_HEATING_CURVE_SLOPE 3.0       // Should match the current configuration on your IVT490
#define IVT490_ADC_CS 15                     // D8
#define IVT490_ADC_CH0_R0 10000              // Ohm
#define IVT490_ADC_CH1_R0 10000              // Ohm
#define IVT490_ADC_SAMPLING_INTERVAL 1000    // milliseconds
#define IVT490_ADC_CH0_FILTER_WINDOW_COUNT 600   // 10 minute average
#define IVT490_ADC_CH1_FILTER_WINDOW_COUNT 600   // 10 minute average
#define IVT490_DIGIPOT_GT2_CS 2                  // D4
#define IVT490_DIGIPOT_GT2_RESOLUTION 8           // bits
#define IVT490_DIGIPOT_GT2_MAX_RESISTANCE 100000 // Ohms
#define IVT490_DIGIPOT_GT32_CS 2                  // D4
#define IVT490_DIGIPOT_GT32_RESOLUTION 8           // bits
#define IVT490_DIGIPOT_GT32_MAX_RESISTANCE 10000 // Ohms
#define IVT490_CONTROL_INTERVAL 1000         // milliseconds
#define IVT490_EXT_IN_RELAY_PIN 6            // D??
#define IVT490_INDOOR_TEMPERATURE_FEEDBACK_CONTROL_WEIGHT 10
#define IVT490_SUMMER_TEMPERATURE_LIMIT 14.0


// To enable debug logging, uncomment the following line
// #define DEBUGLOG_DEFAULT_LOG_LEVEL_DEBUG

#endif
```
