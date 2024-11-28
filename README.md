# Interactive-CZ-Map

This project is a software for the [LaskaKit Interactive CZ Map](https://www.laskakit.cz/laskakit-interaktivni-mapa-cr-ws2812b/). It is designed to be controlled from AWS IoT Core. The map uses WS2812B LEDs to display various interactive features and information.

The project was created in PlatformIO 22.11.2024

[![ESP32](https://img.shields.io/badge/ESP-32-000000.svg?longCache=true&style=flat&colorA=AA101F)](https://www.espressif.com/en/products/socs/esp32)<br>
[![Build with PlatformIO](https://img.shields.io/badge/Build%20with-PlatformIO-orange)](https://platformio.org/)<br>
[![License: MIT](https://img.shields.io/badge/License-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

<br>

## Configuration and build
1. Ensure you have the [PlatformIO](https://platformio.org/) extension installed in VS Code.
2. Create a copy of `platformio_override.ini.example` and rename it to `platformio_override.ini`.
3. Customize values in `platformio_override.ini`.
4. Create a copy of `src/secrets.h.example` and rename it to `src/secrets.h`.
5. Customize values in `src/secrets.h` to match your AWS IoT Core configuration. Use [this guide](https://aws.amazon.com/ru/blogs/compute/building-an-aws-iot-core-device-using-aws-serverless-and-an-esp32/) to create a new thing in AWS IoT Core and get the required values.
6. Use PlatformIO to build and upload the project to your ESP device.

## Testing
You can send messages to the device in AWS IoT MQTT Test console.

### Set LED color
Use topic `interactive-cz-map/Interactive-CZ-Map-01/commands/leds` and payload:
```json
{
    "index": 0,
    "brightness": 255,
    "blinks": 3,
    "delay": 1000,
    "color": [255, 100, 0]
}
```

### Firmware Update via MQTT
Use topic `interactive-cz-map/Interactive-CZ-Map-01/commands/update` and payload:
```json
{
    "firmware_url": "https://example.com/firmware.bin"
}
```

## Dependencies
All dependencies could be found in `platformio.ini` file under `lib_deps` section.

## Copyright
Copyright (c) 2024 Sen Morgan. Licensed under the MIT license, see LICENSE.md