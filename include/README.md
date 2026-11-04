
This directory is intended for project header files.

More specifically, this project requires a header file named `config.h` to be placed in this directory with the following contents (replace as applicable). `config.h` is gitignored; so is `include/secrets.h`, if you prefer to keep the credentials in a file of their own and `#include "secrets.h"` from `config.h`.

```cpp
#ifndef CONFIG_H
#define CONFIG_H

#define GENERAL_CONTROL_VALUES_VALIDITY 360 * 1000 // milliseconds, see "Validity" in the main README
#define GENERAL_STATE_PUBLISH_INTERVAL 10000 // milliseconds

#define WIFI_SSID "YOUR WIFI SSID"
#define WIFI_PW "YOUR WIFI PASSWORD"

#define OTA_HOSTNAME "esp8266-ivt490"
#define OTA_PASSWORD "YOUR-PASSWORD" // required: guards both firmware update paths

#define MQTT_HOST IPAddress(1, 1, 1, 1)
#define MQTT_PORT 1883
#define MQTT_USER "YOUR MQTT USERNAME"
#define MQTT_PW "YOUR MQTT PASSWORD"
#define MQTT_BASE_TOPIC String("BASE/TOPIC/FOR/PUBLISHING") // any String expression, e.g. "HOME/" + WiFi.macAddress()

#define IVT490_SERIAL_RX 5                   // D1
#define IVT490_HEATING_CURVE_SLOPE 3.0       // Should match the current configuration on your IVT490
#define IVT490_ADC_CS 15                     // D8
#define IVT490_ADC_CH0_R0 10000              // Ohm, GT2
#define IVT490_ADC_CH1_R0 10000              // Ohm, GT3:2
#define IVT490_ADC_SAMPLING_INTERVAL 1000    // milliseconds
#define IVT490_DIGIPOT_GT2_CS 2                  // D4
#define IVT490_DIGIPOT_GT2_RESOLUTION 8           // bits
#define IVT490_DIGIPOT_GT2_MAX_RESISTANCE 100000 // Ohms (MCP41100)
#define IVT490_DIGIPOT_GT32_CS 4                  // D2
#define IVT490_DIGIPOT_GT32_RESOLUTION 8           // bits
#define IVT490_DIGIPOT_GT32_MAX_RESISTANCE 10000 // Ohms (MCP41010)
#define IVT490_CONTROL_INTERVAL 1000         // milliseconds
#define IVT490_EXT_IN_RELAY_PIN 0            // D3
#define IVT490_INDOOR_TEMPERATURE_FEEDBACK_CONTROL_WEIGHT 10
#define IVT490_SUMMER_TEMPERATURE_LIMIT 14.0 // Should match the summer limit on your IVT490; -1 disables the vacation-mode workaround


// To enable debug logging, uncomment the following line
// #define DEBUGLOG_DEFAULT_LOG_LEVEL_DEBUG

#endif
```

Both ADC channels are filtered with a fixed 600-sample moving average, which at the 1000 ms sampling interval above is a 10-minute mean (`lib/IVT490/IVT490.h`). Older versions of this sample listed `IVT490_ADC_CH{0,1}_FILTER_WINDOW_COUNT`; nothing reads them any more.

The following have defaults in `src/main.cpp` and can be overridden in `config.h` if needed:

| Macro | Default | What |
|---|---|---|
| `OTA_USERNAME` | `"admin"` | User name for the HTTP update page |
| `MQTT_BUFFER_SIZE` | `2048` | Largest MQTT packet, which must hold the `ivt490/state` blob plus its topic |
| `MQTT_KEEPALIVE_S` | `15` | MQTT keepalive, seconds |
| `MQTT_RECONNECT_INTERVAL` | `5000` | Milliseconds between MQTT connection attempts |
| `CONNECTIVITY_WATCHDOG_TIMEOUT` | 5 minutes | Restart if Wi-Fi or MQTT has been down this long |
| `DAILY_RESTART_INTERVAL` | 24 hours | Unconditional restart |
| `DIAGNOSTICS_PUBLISH_INTERVAL` | `60000` | Milliseconds between `diagnostics/*` publishes |
