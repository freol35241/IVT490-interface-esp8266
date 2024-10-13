# IVT490-interface-esp8266
Interfacing software/hardware for the IVT490 heatpump tailored for a esp8266 device such as the Wemos D1 mini.

The code serves as a "dumb" API over MQTT.

## Overview
In essence, a combination of software and hardware is used to:

* Read serial output from the heatpump
* Directly read a subset of the NTC resisitive sensors used by the IVT490 using ADCs
* Emulate resistive sensors using digitally controlled potentiometers (DCPs)
* Control the EXT_IN port on the heatpump to activate/deactivate the vacation mode

As such, it is possible to emulate input to the IVT490 controller board. The default behavior is however to emulate the same temperature as is being sensed by the resistive sensors, which essentially means to keep the heatpump working as per its own configuration.

Currently, the software and hardware supports to emulate the following temperature sensors:

* GT2 (outdoor temperature sensor)

  This allows for controlling the target feed temperature (GT1_target) of the heating system through knowledge of the heating curve used by the heatpump.

* GT3:2 (boiler temperature)

  This allows for explicitly controlling when the compressor will be active, either by conducting Business as usual, blocking operation or boosting operation.


## Hardware

BOM:

- Wemos D1 mini (Other esp8266-based boards should probably be fine but will obviously require adjustments to the wiring)
- MCP3208 (or a similar one with fewer channels, all wiring schematics are shown using this particular one and may have to be adjusted if exchanged)
- MCP41100
- MCP41010
- 10kOhm resistors (2 pcs)
- 3v3 relay

Schematic:
![](circuit.svg)

Note: The MCP41100 Vdd pin is connected to 5V in order to allow the 5V connection from the IVT490 ADC. According to the [datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/11195c.pdf) this should render the 3.3V logic level of the Wemos D1 mini useless but nonetheless, it works...

## Software

All pre-deployment configuration of the software in this repository is done using a specific `config.h` include file, see [here](include/README.md) for more details.

## Heatpump (IVT490)

1. Connect to the debug interface on the control board on the heatpump and enable debug output in the heatpump menu. Detailed instructions are available in [this](http://www.tsoft.se/wp/2015/03/08/overvakning-av-min-ivt-490-varmepump-med-raspberry-pi/) blog post.


2. Intersect the GT2 sensor cabling and connect the [Hardware](#hardware) as a "man-in-the-middle".

3. Interest the GT3:2 sensor caling and the connect the [Hardware](#hardware) as a "man-in-the-middle".

4. Connect the 3v3 relay to the `EXT_IN` port on the heatpump control board.

## API

All communication during runtime happens via MQTT.

The interface publishes data at 10 second intervals according to:

* `{MQTT_BASE_TOPIC}/ivt490`

  The raw text payload received on the serial debug connection from the IVT490 heatpump.

* `{MQTT_BASE_TOPIC}/ivt490/state`

  A JSON blob consisting of the full state of IVT490 heatpump

* `{MQTT_BASE_TOPIC}/ivt490/state/{parameter}`

  All parameters in the state are also published onto individual topics as floats/ints/bools.

* `{MQTT_BASE_TOPIC}/controller/state`

  A JSON blob consisting of the full state of the software controller

* `{MQTT_BASE_TOPIC}/controller/state/{parameter}`

  All parameters in the state are also published onto individual topics as floats/ints/bools.

The controler listens for control commands according to:

* `{MQTT_BASE_TOPIC}/controller/set/feed_temperature_target`

  Set a new target feed temperature of the heating system by publishing to this topic. 

* `{MQTT_BASE_TOPIC}/controller/set/indoor_temperature_target`

  Set a new target indoor temperature of the heating system by publishing to this topic. 

* `{MQTT_BASE_TOPIC}/controller/set/outdoor_temperature_offset`

  Set an arbitrary offset to the oudoor temperature sensor of the heating system by publishing to this topic. 

The controller also listens to feedback according to:

* `{MQTT_BASE_TOPIC}/controller/set/indoor_temperature_actual`

  Indoor temperature feedback for the controller.

* `{MQTT_BASE_TOPIC}/controller/set/operating_mode`

  Operating state for the GT3_2 emulator, can be one of:

  * 1 (Business as usual)
  * 2 (Block mode)
  * 3 (Boost mode)

Please note that all the values received on the `controller` topics have a finite validity and, as such, even non-changing control values need to be repeatedly published to avoid fallback to the default behavior.

## Build and deploy

Clone (or fork and clone) this repository.

Assemble the hardware according to the [Hardware](#hardware) section and configure the software according to the [Software](#software) section. Then build the software using PlatformIO and upload to your board.

