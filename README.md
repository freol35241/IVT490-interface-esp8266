# (WIP) IVT490-interface-esp8266
Interfacing software/hardware for the IVT490 heatpump tailored for a esp8266 device such as the Wemos D1 mini.

The code serves as a "dumb" API over MQTT.

## Overview
In essence, a combination of software and hardware is used to:

* Read serial output from the heatpump
* Directly read a subset of the NTC resisitive sensors used by the IVT490 using ADCs
* Emulate resistive sensors using digitally controlled potentiometers (DCPs)

As such, it is possible to "fake" input to the IVT490 controller board such that it is possible to:

* Directly control the target feed temperature to the heating system

The default behavior is however to emulate the same temperature as is being sensed by the resistive sensors, which essentially means to keep the heatpump working as per its own configuration.


## Hardware

BOM:

- Wemos D1 mini (Other esp8266-based boards should probably be fine but will obviously require adjustments to the wiring)
- MCP3208 (or a similar one with fewer channels, all wiring schematics are shown using this particular one and may have to be adjusted if exchanged)
- MCP41100
- 10kOhm resistor

Schematic:
![](circuit.svg)


## Software

All pre-deployment configuration of the software in this repository is done using a specific `config.h` include file, see [here](include/README.md) for more details.

## Heatpump (IVT490)

1. Connect to the debug interface on the control board on the heatpump and enable debug output in the heatpump menu. Detailed instructions are available in [this](http://www.tsoft.se/wp/2015/03/08/overvakning-av-min-ivt-490-varmepump-med-raspberry-pi/) blog post.


2. Intersect the GT2 sensor cabling and connect the [Hardware](#hardware) as a "man-in-the-middle".

## API

All communication during runtime happens via MQTT.

The interface publishes data according to:

* `{MQTT_BASE_TOPIC}/state`

  A JSON blob consisting of the full state of the agent and IVT490 heatpump

* `{MQTT_BASE_TOPIC}/state/{parameter}`

  All parameters in the state are also published onto individual topics as floats/ints/bools.

States are published at 60 second intervals.

The interface listens for control commands according to:

* `{MQTT_BASE_TOPIC}/control/GT1_target`

  Set a new target feed temperature of the heating system by publishing to this topic. 
  
Please note that all the values received on the control topics have a finite validity and, as such, even non-changing control values need to be repeatedly published to avoid fallback to the default behavior.

## Build and deploy

Clone (or fork and clone) this repository.

Assemble the hardware according to the [Hardware](#hardware) section and configure the software according to the [Software](#software) section. Then build the software using PlatformIO and upload to your board.

