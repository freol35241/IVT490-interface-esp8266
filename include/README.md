
This directory is intended for project header files.

More specifically, this project requires a header file named `config.h` to be placed in this directory with the following contents:

```cpp
#ifndef CONFIG_H
#define CONFIG_H

#define WIFI_SSID "YOUR WIFI SSID"
#define WIFI_PW "YOUR WIFI PASSWORD"

#define MQTT_HOST IPAddress(XXX, XXX, XXX, XXX)
#define MQTT_PORT XXXX
#define MQTT_USER "YOUR MQTT USERNAME"
#define MQTT_PW "YOUR MQTT PASSWORD"

#define MQTT_BASE_TOPIC "BASE/TOPIC/FOR/PUBLISHING"

#define IVT490_SERIAL_TX 10
#define IVT490_SERIAL_RX 11
#define IVT490_HEATING_CURVE_SLOPE 4.7
#define IVT490_ADC_CS 15                // D8
#define IVT490_THERMISTOR_EMULATOR_CS 2 // D4
#define IVT490_CONTROL_VALUES_VALIDITY 360 * 1000
#define IVT490_R0_RESISTANCE 10000

// To enable debug logging, uncomment the following line
// #define DEBUGLOG_DEFAULT_LOG_LEVEL_DEBUG

#endif
```
