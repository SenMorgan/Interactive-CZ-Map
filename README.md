# Interactive-CZ-Map

This project is a firmware for the [LaskaKit Interactive CZ Map](https://www.laskakit.cz/laskakit-interaktivni-mapa-cr-ws2812b/). It is designed to be controlled from AWS IoT Core. The map uses WS2812B LEDs to display various interactive features and information.

The project was created in PlatformIO 22.11.2024

[![ESP32](https://img.shields.io/badge/ESP-32-000000.svg?longCache=true&style=flat&colorA=AA101F)](https://www.espressif.com/en/products/socs/esp32)<br>
[![Build with PlatformIO](https://img.shields.io/badge/Build%20with-PlatformIO-orange)](https://platformio.org/)<br>
[![License: MIT](https://img.shields.io/badge/License-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

<br>

## Configuration and build
1. Ensure you have the [PlatformIO](https://platformio.org/) extension installed in VS Code.
3. Create a copy of `include/secrets.h.example` and rename it to `include/secrets.h`.
4. Customize values in `include/secrets.h` to match your AWS IoT Core configuration and update WiFi configuration if needed. Use [this guide](https://aws.amazon.com/ru/blogs/compute/building-an-aws-iot-core-device-using-aws-serverless-and-an-esp32/) to create a new thing in AWS IoT Core and get the required values.
5. Use PlatformIO to build and upload the project to your ESP device.
6. Default password for AP mode is `12345678`.

## LED flicker

LEDs that are off could briefly flash a random color while other LEDs were lit. The cause was the FastLED RMT driver: with the default buffer it has to refill the LED signal from an interrupt every few tens of microseconds, and any delay corrupts a frame. The fix is in `platformio.ini`:

```ini
build_flags =
	-D FASTLED_RMT_MAX_CHANNELS=1
	-D FASTLED_RMT_MEM_BLOCKS=8
```

These flags give the single LED strip all 8 RMT memory blocks instead of 2. Keep them. The alternatives made it worse: `FASTLED_RMT_BUILTIN_DRIVER=1` (flicker twice in a row) and `FASTLED_ESP32_I2S=1` (more flicker).

## Testing
You can send messages to the device in AWS IoT MQTT Test console. See `test` folder for examples.

## Commands
All commands are sent to the device using MQTT. Commands could be sent as general messages or personalized for a specific device by Client ID.
The device subscribes to the following topics:
- `int-cz-map/cmd/leds/#`
- `int-cz-map/cmd/update/#`

### Example of LEDs command
Topic general: `int-cz-map/cmd/leds`
Topic personalized: `int-cz-map/cmd/leds/AABBCC`

### Single sequence for single LED
```json
{
    "leds": [
        {
            "id": 1,
            "cl": "FFFFFF",
            "br": 100,
            "dr": 600,
            "ct": 2
        }
    ]
}
```

### Multiple sequences for single LED
```json
{
    "leds": [
        {
            "id": 36,
            "cl": "FF0000",
            "br": 50,
            "dr": 600,
            "ct": 2
        },
        {
            "id": 36,
            "cl": "00FF00",
            "br": 100,
            "dr": 300,
            "ct": 2
        },
        {
            "id": 36,
            "cl": "0000FF",
            "ct": 5
        }
    ]
}
```

### Single sequence for multiple LEDs
```json
{
    "leds": [
        {
            "id": 1,
            "cl": "00FFFF",
            "br": 100,
            "dr": 600,
            "ct": 2
        },
        {
            "id": 2,
            "cl": "FF0000",
            "br": 50,
            "dr": 300,
            "ct": 2
        },
        {
            "id": 3,
            "cl": "00FF00",
            "br": 100,
            "dr": 600,
            "ct": 2
        },
        {
            "id": 4,
            "cl": "FF00FF",
            "br": 50,
            "dr": 300,
            "ct": 2
        }
    ]
}
```

### Example of FW Update command
Topic general: `int-cz-map/cmd/update`
Topic personalized: `int-cz-map/cmd/update/AABBCC`
```json
{
    "firmware_url": "https://example.com/firmware.bin"
}
```

## Dependencies
All dependencies could be found in `platformio.ini` file under `lib_deps` section.

## Copyright
Copyright (c) 2024 Sen Morgan. Licensed under the MIT license, see LICENSE.md