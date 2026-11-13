# CLAUDE.md

This file gives Claude Code (claude.ai/code) guidance for working in this repository.

## Project overview

ESP32 firmware (PlatformIO, Arduino framework) for the LaskaKit Interactive CZ Map: a board with 72 WS2812B LEDs laid out over a map of the Czech Republic. The device is controlled over MQTT from **AWS IoT Core**, which sends LED effect commands and OTA firmware updates. An optional second MQTT client connects to a local **Home Assistant** broker to turn the map on or off and to report sensor data.

## Build / flash / monitor

There is a single environment, `espwroom32` (board `esp32dev`, partitions `min_spiffs.csv`). The platform is pinned to `espressif32@6.11.0` (Arduino core 2.0.17); the pinned libraries do not compile on Arduino core 3.x.

```sh
pio run                      # build
pio run -t upload            # flash (921600 baud)
pio device monitor           # serial monitor, 115200 baud, timestamped
pio run -t upload -t monitor
```

- **Before the first build**, copy `include/secrets.h.example` to `include/secrets.h` (it is gitignored). `constants.h` includes `secrets.h`, so the build fails without it.
- `platformio_override.ini` is gitignored and holds local overrides.
- There are no unit tests. `test/` holds sample MQTT payloads (`example_leds_*.json`) to paste into the AWS IoT MQTT test console, plus mock-ups of the WiFiManager config-portal HTML and CSS.
- `release/` holds prebuilt binaries per version (`bootloader.bin`, `firmware.bin`). Its contents are gitignored except `release/README.md`, which covers flashing through esptool-js (offsets: bootloader `0x1000`, partitions `0x8000`, app `0x10000`).

## Compile-time feature flags (in `include/secrets.h`)

| Macro | Effect |
|---|---|
| `USE_HOME_ASSISTANT` | Starts `haClientTask`, which connects to the HA broker. When undefined, `isMapEnabled()` always returns `true`. |
| `USE_AWS_FOR_FIRMWARE_UPDATE` | OTA requires an `https://` URL and checks it against `AWS_CERT_CA`. Otherwise OTA uses plain HTTP with no validation. |
| `THINGNAME` | MQTT client ID for AWS. Defaults to the 6-hex-char chip ID (the last 3 bytes of the MAC). |

`secrets.h` also holds the AP SSID and password (default `12345678`), the HA broker credentials, the AWS endpoint, and the CA, device certificate and private key as `PROGMEM` strings.

Project-wide constants are in [include/constants.h](include/constants.h): `FIRMWARE_VERSION` (bump it on release), MQTT topic roots, `LEDS_COUNT = 72`, the LED data pin (GPIO25), the notification-control pin (GPIO23), `MAX_FADE_DURATION` and `MAX_FADE_REPEATS`.

## Architecture

Execution is split between the Arduino `loop()` and FreeRTOS tasks pinned to **core 1** (core 0 is left to WiFi).

- **[src/main.cpp](src/main.cpp)**: `setup()` gets the chip ID, then starts LEDs, WiFi, AWS and (optionally) HA. `loop()` runs `handleWiFi()`, `maintainAWSConnection()` and `periodicStatusPublishAWS()`.
- **[src/leds.cpp](src/leds.cpp)**: `ledsTask` runs at 100 Hz and owns FastLED. Each LED has its own `LedState` and a FreeRTOS queue of `LedCommand`s (depth 10). `refreshLeds()` is a per-LED fade state machine (fade in, fade out, decrement cycles). When an LED finishes, it pops the next queued command, so several commands for the same LED run in sequence. `fadeCycles = LOOP_INDEFINITELY (-1)` loops forever. `leds`, `ledStates` and the status variables are shared with the loop task and the HA task and are protected by `ledsMutex`; `ledsTaskInit()` creates the mutex, the queues and the strip before it starts the task. The module also contains the system indications: `ledsSetStatus(LedsStatus)` (a looping ring of LEDs, `CIRCLE_LEDS_ARRAY`, colored by status) and `progressIndicator` (OTA progress). `ledsSetStatus` is idempotent and only shows an effect when GPIO23 is **not** jumpered to GND and `isMapEnabled()` is true. Going back to `LedsStatus::IDLE` flashes green only if an indication was displayed. `ledsApplyMapState()` clears the map when HA turns it off.
- **[src/leds_parser.cpp](src/leds_parser.cpp)**: validates the JSON command and turns it into `pushLedCommand()` calls. On an invalid global field, the whole message is dropped. On an invalid per-LED entry, only that entry is skipped. LED IDs in JSON are **1-based** and become 0-based indices; if `id` is missing, the array position + 1 is used.
- **[src/aws_iot.cpp](src/aws_iot.cpp)**: a `WiFiClientSecure` + PubSubClient connection to AWS (8 KB buffer, exponential reconnect backoff from 1 s up to 30 s, reset only after a connection that lasted 60 s). The module keeps its own copy of the client ID. A disconnect is indicated on the map only after 10 s (`DISCONNECT_INDICATION_DELAY`), so short outages do not interrupt LED effects. It subscribes to both the broadcast topic and the per-device topic (`<topic>/<clientId>`) for LEDs and updates. It publishes a status message every 60 s. `messageHandler` runs inside `client.loop()` in the Arduino loop.
- **[src/firmware_update.cpp](src/firmware_update.cpp)**: a **blocking** HTTP(S) OTA that runs inside the MQTT callback. It reports results through a callback to `int-cz-map/status/update/<id>` and reboots on success. On failure, the progress indication is left on screen on purpose.
- **[src/ha_client.cpp](src/ha_client.cpp)**: `haClientTask` runs a separate PubSubClient on plain TCP. It publishes HA MQTT discovery configs (a switch plus sensors for AWS reconnects, AWS messages received and uptime), listens on `int-cz-map/cmd/enable/<chipId>` for `ON`/`OFF` and sets `mapState`. It also publishes a `number` entity for the main brightness and listens on `int-cz-map/cmd/brightness/<chipId>`; the value goes to `ledsSetMainBrightness()`, which scales the whole strip through `FastLED.setBrightness`. The whole client is inside `#ifdef USE_HOME_ASSISTANT`. Without the flag the file only provides an `isMapEnabled()` stub that returns `true`, so the `HA_MQTT_*` settings are not needed and no second MQTT client is linked. Both configurations must keep building and working: many devices run without HA.
- **[src/wifi_manager.cpp](src/wifi_manager.cpp)**: ESPAsync_WiFiManager with up to 2 stored credentials, saved in LittleFS at `/wifi_cred.dat` with a checksum, and `WiFiMulti` for reconnects. `handleWiFi()` waits 5 s for the driver to reconnect on its own before it calls the blocking `connectMultiWiFi()`, which keeps retrying and restarts the ESP32 when WiFi has been down for 60 s in total (`WIFI_DOWN_RESTART_TIMEOUT`). The config portal opens with no timeout when no credentials are stored or when a double reset is detected. While it is open, the LEDs run an orange circle effect. `custom_html.h` holds the portal's CSS and favicon.
- **[src/drd.cpp](src/drd.cpp)**: double-reset detection through a LittleFS flag file. It only arms on `ESP_RST_POWERON` resets, so a reboot after OTA does not open the portal. The window is `DRD_TIMEOUT` (2 s).
- **[src/esp32_utils.cpp](src/esp32_utils.cpp)**: serial init and chip-ID helpers.

## MQTT contract

Topics are built from the macros in `constants.h`, with base `int-cz-map`:

- Subscribed: `cmd/leds`, `cmd/leds/<id>`, `cmd/update`, `cmd/update/<id>`. On HA only: `cmd/enable/<chipId>` (`ON`/`OFF`) and `cmd/brightness/<chipId>` (0–255). Both HA commands are retained by the broker (`ret: true` in discovery); the device stores nothing in flash and starts enabled at brightness 255.
- Published: `status/device/<id>` and `status/update/<id>`.

LED payload keys use short names to keep messages small:
- Global: `colors` (a palette of hex strings), `bright` (0–255), `duration` (ms, 0–5000), `count` (1–100).
- Per-LED in `leds[]`: `id` (1–72), `cl` (hex color), `cx` (palette index), `br`, `dr`, `ct`.
- Color resolution order: `cl`, then `cx`, then `colors[0]`.
- Defaults: brightness 255, duration 500, count 1.

The README has full examples. When you change keys or limits, update the README and the `test/*.json` samples too.

## Conventions

- Commit messages use Conventional Commits (`feat:`, `fix:`, `refactor:`, `chore:`, `build:`, `docs:`).
- Library versions are pinned exactly in `platformio.ini`. Keep them pinned.
- Functions have Doxygen-style `@brief` comments. Logging goes through `Serial.printf` and `F()` strings. Don't log WiFi passwords; this was removed on purpose.
- Use `ledsSetStatus()`, not direct LED effect calls, for status indications, so that the notify jumper and the HA enable switch are respected.
- A FreeRTOS task function must never `return`; that aborts and reboots the ESP32.
