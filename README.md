# Interactive-CZ-Map

This project is a firmware for the [LaskaKit Interactive CZ Map](https://www.laskakit.cz/laskakit-interaktivni-mapa-cr-ws2812b/), an ESP32 board with 72 WS2812B LEDs laid out over a map of the Czech Republic.

The map is controlled over MQTT from AWS IoT Core, which sends LED effect commands and firmware updates. Optionally, the map also connects to a local Home Assistant MQTT broker, where it can be turned on and off and dimmed.

The project was created in PlatformIO 22.11.2024

[![ESP32](https://img.shields.io/badge/ESP-32-000000.svg?longCache=true&style=flat&colorA=AA101F)](https://www.espressif.com/en/products/socs/esp32)<br>
[![Build with PlatformIO](https://img.shields.io/badge/Build%20with-PlatformIO-orange)](https://platformio.org/)<br>
[![License: MIT](https://img.shields.io/badge/License-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

<br>

## Configuration and build
1. Ensure you have the [PlatformIO](https://platformio.org/) extension installed in VS Code.
2. Create a copy of `include/secrets.h.example` and rename it to `include/secrets.h`. This file is ignored by git.
3. Customize values in `include/secrets.h` to match your AWS IoT Core configuration. Use [this guide](https://aws.amazon.com/ru/blogs/compute/building-an-aws-iot-core-device-using-aws-serverless-and-an-esp32/) to create a new thing in AWS IoT Core and get the required values.
4. Use PlatformIO to build and upload the project to your ESP device.

The platform is pinned to `espressif32@6.11.0` (Arduino core 2.0.17) in `platformio.ini`. The libraries used by the project do not compile on Arduino core 3.x.

### Options in `include/secrets.h`

| Option | Description |
|---|---|
| `AP_SSID`, `AP_PASSWORD` | Name and password of the access point of the WiFi config portal. The default password is `12345678`. |
| `USE_HOME_ASSISTANT` | Enables the Home Assistant integration. It is disabled by default. |
| `HA_MQTT_BROKER_HOST`, `HA_MQTT_BROKER_PORT`, `HA_MQTT_USER`, `HA_MQTT_PASS` | Home Assistant MQTT broker. Needed only with `USE_HOME_ASSISTANT`. |
| `USE_AWS_FOR_FIRMWARE_UPDATE` | The firmware update accepts only `https://` URLs and verifies the server with the AWS root CA. Without it, the firmware is downloaded over plain HTTP with no verification. |
| `THINGNAME` | MQTT client ID for AWS IoT. If it is not defined, the Chip ID is used. |
| `AWS_IOT_ENDPOINT`, `AWS_CERT_CA`, `AWS_CERT_CRT`, `AWS_CERT_PRIVATE` | AWS IoT endpoint, root CA, device certificate and private key. |

The Chip ID is the last 3 bytes of the MAC address as 6 hex characters, for example `C435D4`. It is printed to the serial console on boot (115200 baud).

## WiFi setup
The WiFi credentials are not compiled into the firmware. They are entered in a config portal and stored on the device. Up to 2 networks can be stored.

The config portal opens when no credentials are stored, or when the RESET button is pressed twice within 2 seconds:
1. The map starts blinking orange.
2. Connect to the access point `Interactive-CZ-Map` (default password `12345678`).
3. Open http://192.168.4.1 in a browser and enter the WiFi credentials.

If the WiFi connection is lost, the map keeps trying to reconnect. When WiFi stays down for 1 minute, the device restarts and tries again.

## Status indication
The device shows its state with a ring of LEDs around the map:

| Color | Meaning |
|---|---|
| Orange, slow | The WiFi config portal is open. |
| Blue, slow | Connecting to WiFi. |
| Purple, fast | Connecting to AWS IoT. |
| Green, 2 flashes | Connected. Shown only after one of the indications above. |
| Red, fast | Fatal error: the filesystem could not be initialized. |
| Blue, growing ring | Firmware update progress. If the update fails, the ring stays on until the device is restarted. |

Short outages are not indicated, so that they do not interrupt the running LED effects: a lost WiFi connection is shown after 5 seconds and a lost AWS IoT connection after 10 seconds.

To turn the status indication off, connect GPIO23 to GND with a jumper. The pin is read on boot. The firmware update progress is always shown.

## Home Assistant
The Home Assistant integration is optional. Enable it with `USE_HOME_ASSISTANT` in `include/secrets.h` and fill in the broker settings. The firmware works the same without it, only the map cannot be turned off or dimmed.

The map connects to the Home Assistant MQTT broker in addition to AWS IoT and registers itself through [MQTT discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery). It appears as the device `Interactive CZ Map` with these entities:

| Entity | Type | Description |
|---|---|---|
| Enable | switch | Turns the map on and off. When it is off, all LEDs are turned off, and LED commands and status indications are ignored. |
| Brightness | number (0–255) | Main brightness of the whole map. It scales all LED effects and status indications. 255 means no limit. |
| AWS Reconnect Attempts | sensor | Number of connection attempts to AWS IoT since boot. |
| AWS Messages Received | sensor | Number of messages received from AWS IoT since boot. |
| Uptime | sensor | Time since boot in seconds. |

The map does not store the Enable and Brightness values. Home Assistant publishes them as retained messages, so the broker sends the last values to the map every time it connects. Until a value arrives, the map is enabled and the brightness is not limited.

MQTT topics used on the Home Assistant broker (`<ChipID>` is the Chip ID of the device):

| Topic | Direction | Payload |
|---|---|---|
| `int-cz-map/cmd/enable/<ChipID>` | to the map | `ON` or `OFF` |
| `int-cz-map/cmd/brightness/<ChipID>` | to the map | Number from `0` to `255` |
| `int-cz-map/status/device/<ChipID>` | from the map | JSON with `enabled`, `brightness`, `haReconnectAttempts`, `awsReconnectAttempts`, `awsMsgsReceived` and `uptime`. Published every 10 seconds and after every command. |

## Commands
All commands are sent to the device from AWS IoT Core using MQTT. Commands could be sent as general messages for all devices or personalized for a specific device by Client ID.
The device subscribes to the following topics:
- `int-cz-map/cmd/leds` and `int-cz-map/cmd/leds/<ClientID>`
- `int-cz-map/cmd/update` and `int-cz-map/cmd/update/<ClientID>`

### LEDs command
Topic general: `int-cz-map/cmd/leds`
Topic personalized: `int-cz-map/cmd/leds/AABBCC`

The command contains a list of fade effects in the `leds` array. Each effect lights one LED and fades it out. Values common for all LEDs can be set once on the top level.

| Key | Level | Description | Default |
|---|---|---|---|
| `leds` | top | Array of LED effects. | required |
| `colors` | top | Color palette: array of hex color strings, for example `["FF0000", "00FF00"]`. | none |
| `bright` | top | Brightness for all LEDs, 0–255. | 255 |
| `duration` | top | Fade duration for all LEDs in milliseconds, 0–5000. | 500 |
| `count` | top | Number of fade repeats for all LEDs, 1–100. | 1 |
| `id` | LED | LED number, 1–72. | position in the array |
| `cl` | LED | Color as a hex string, for example `"FF5500"`. | |
| `cx` | LED | Index of the color in the `colors` palette, starting from 0. | |
| `br` | LED | Brightness of this LED, overrides `bright`. | |
| `dr` | LED | Fade duration of this LED, overrides `duration`. | |
| `ct` | LED | Number of fade repeats of this LED, overrides `count`. | |

- The color of an LED is taken from `cl`, then from `cx`, then from the first color of the palette.
- If a top-level value is invalid, the whole command is ignored. If a value of one LED is invalid, only that LED is skipped.
- Several effects for the same LED are played one after another. Up to 10 effects can wait in the queue of each LED, the rest are dropped.

#### Single sequence for single LED
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

#### Multiple sequences for single LED
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

#### Single sequence for multiple LEDs
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

#### Global values and color palette
```json
{
    "colors": ["FF0000", "00FF00"],
    "bright": 50,
    "duration": 300,
    "count": 2,
    "leds": [
        { "id": 10 },
        { "id": 11, "cx": 1 },
        { "id": 12, "cl": "0000FF", "br": 255 }
    ]
}
```

### FW Update command
Topic general: `int-cz-map/cmd/update`
Topic personalized: `int-cz-map/cmd/update/AABBCC`
```json
{
    "firmware_url": "https://example.com/firmware.bin"
}
```

The device downloads the firmware, shows the progress on the map and restarts. The LED effects are paused during the update. The start and the result of the update are published to `int-cz-map/status/update/<ClientID>`.

## Status messages
The device publishes these messages to AWS IoT Core:

| Topic | When | Content |
|---|---|---|
| `int-cz-map/status/device/<ClientID>` | After connecting and every 60 seconds | `fw_version`, `uptime` (seconds), `reconnects`, `reset_reason`, `wifi_ssid`, `ip_address`, `mac_address` |
| `int-cz-map/status/update/<ClientID>` | During a firmware update | `status` (`in_progress`, `success` or `failure`) and `message` |

## Testing
There are no automated tests. You can send messages to the device in AWS IoT MQTT Test console. See `test` folder for example payloads.

The serial console (115200 baud) logs the connection state and every received command.

## Flashing a prebuilt firmware
A prebuilt firmware can be uploaded from a browser, without PlatformIO:
1. Get the binaries `bootloader.bin`, `partitions.bin` and `firmware.bin`. After a build they are in `.pio/build/espwroom32/`.
2. Connect the map to the computer with a USB cable.
3. Open https://espressif.github.io/esptool-js/ in Chrome or Edge and connect to the serial port of the map.
4. Add the files with these flash addresses and press Program:

| Flash address | File |
|---|---|
| `0x1000` | `bootloader.bin` |
| `0x8000` | `partitions.bin` |
| `0x10000` | `firmware.bin` |

5. Press the RESET button on the map after the upload.

## Troubleshooting

### LED flicker
LEDs that are off could briefly flash a random color while other LEDs were lit. The cause was the FastLED RMT driver: with the default buffer it has to refill the LED signal from an interrupt every few tens of microseconds, and any delay corrupts a frame. The fix is in `platformio.ini`:

```ini
build_flags =
	-D FASTLED_RMT_MAX_CHANNELS=1
	-D FASTLED_RMT_MEM_BLOCKS=8
```

These flags give the single LED strip all 8 RMT memory blocks instead of 2. Keep them. The alternatives made it worse: `FASTLED_RMT_BUILTIN_DRIVER=1` (flicker twice in a row) and `FASTLED_ESP32_I2S=1` (more flicker).

## Dependencies
All dependencies could be found in `platformio.ini` file under `lib_deps` section. The versions are pinned.

## Copyright
Copyright (c) 2024 Sen Morgan. Licensed under the MIT license, see LICENSE.md
