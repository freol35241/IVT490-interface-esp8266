
This directory is intended for project header files.

More specifically, this project requires a header file named `config.h` to be placed in this directory with the following contents (replace as applicable):

```cpp
#ifndef CONFIG_H
#define CONFIG_H

#define GENERAL_CONTROL_VALUES_VALIDITY 360 * 1000
#define GENERAL_STATE_PUBLISH_INTERVAL 10000 // milliseconds

#define WIFI_SSID "YOUR WIFI SSID"
#define WIFI_PW "YOUR WIFI PASSWORD"

#define MQTT_HOST IPAddress(XXX, XXX, XXX, XXX)
#define MQTT_PORT XXXX
#define MQTT_USER "YOUR MQTT USERNAME"
#define MQTT_PW "YOUR MQTT PASSWORD"
#define MQTT_BASE_TOPIC "BASE/TOPIC/FOR/PUBLISHING"

#define IVT490_SERIAL_RX 11
#define IVT490_HEATING_CURVE_SLOPE 4.7
#define IVT490_ADC_CS 15                     // D8
#define IVT490_ADC_R0 10000                  // Ohm
#define IVT490_ADC_SAMPLING_INTERVAL 1000    // milliseconds
#define IVT490_ADC_FILTER_WINDOW_COUNT 10    //
#define IVT490_DIGIPOT_CS 2                  // D4
#define IVT490_DIGPOT_RESOLUTION 8           // bits
#define IVT490_DIGIPOT_MAX_RESISTANCE 100000 // Ohms

// To enable debug logging, uncomment the following line
// #define DEBUGLOG_DEFAULT_LOG_LEVEL_DEBUG

#endif
```
