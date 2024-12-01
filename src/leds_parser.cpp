#include <ArduinoJson.h>
#include "constants.h"
#include "leds_parser.h"

// Define JSON keys
#define LEDS_KEY              "leds"     // Key for the LED configurations array
#define COLOR_PALETTE_KEY     "colors"   // Key for the color palette array
#define BRIGHTNESS_KEY        "bright"   // Key for the global brightness value
#define DURATION_KEY          "duration" // Key for the global fade effect duration
#define COUNT_KEY             "count"    // Key for the global fade effect count
#define LED_ID_KEY            "id"       // Key for the LED ID
#define LED_COLOR_KEY         "cl"       // Key for the LED color (hex string)
#define LED_COLOR_PALETTE_KEY "cx"       // Key for the LED color index in the palette
#define LED_BRIGHTNESS_KEY    "br"       // Key for the LED brightness
#define LED_DURATION_KEY      "dr"       // Key for the fade effect duration
#define LED_COUNT_KEY         "ct"       // Key for the fade effect count

// Default values for LED configurations
#define DEFAULT_BRIGHTNESS 255 // Default brightness value for LEDs
#define DEFAULT_DURATION   500 // Default duration value for fade effects
#define DEFAULT_COUNT      1   // Default count value for fade effects

/**
 * @brief Validates and retrieves the brightness value from a JSON variant.
 *
 * This function checks if the provided `jsonValue` contains a valid brightness value.
 * If `jsonValue` is an integer within the range [0, 255], it assigns this value to
 * the location pointed to by `brightness` and returns `true`. If the value is out of
 * range or not an integer, it logs an appropriate error message and returns `false`.
 *
 * @param jsonValue The JSON variant containing the brightness value to validate.
 * @param brightness A pointer to a `uint16_t` where the validated brightness value will be stored.
 * @param ledId Optional. The ID of the LED being validated. Defaults to `0` for global brightness.
 *
 * @return `true` if the brightness value is valid and successfully assigned.
 * @return `false` if the brightness value is invalid or not an integer.
 */
bool validateBrightness(JsonVariant jsonValue, uint16_t *brightness, int16_t ledId = 0)
{
    // Check if the JSON variant is of integer type
    if (jsonValue.is<int>())
    {
        // Retrieve the brightness value as a signed 16-bit integer
        int16_t value = jsonValue.as<int>();

        // Validate that the brightness value is within the acceptable range
        if (value < 0 || value > 255)
        {
            if (ledId > 0)
                // Log an error message specific to the LED ID with the invalid brightness value
                Serial.printf("Invalid brightness value for LED %d: %d. Must be between 0 and 255\n", ledId, value);
            else
                // Log an error message for global brightness with the invalid value
                Serial.printf("Invalid global brightness value: %d. Must be between 0 and 255\n", value);
            return false; // Indicate that validation failed
        }

        // Assign the validated brightness value to the provided pointer
        *brightness = static_cast<uint16_t>(value);
        return true; // Indicate successful validation
    }
    else
    {
        if (ledId > 0)
            // Log an error message if the brightness value for a specific LED is not an integer
            Serial.printf("Error: LED %d brightness value is not an integer.\n", ledId);
        else
            // Log an error message if the global brightness value is not an integer
            Serial.println("Error: Global brightness value is not an integer.");
        return false; // Indicate that validation failed
    }
}

/**
 * @brief Validates and retrieves the duration value from a JSON variant.
 *
 * This function checks if the provided `jsonValue` contains a valid duration value.
 * If `jsonValue` is an integer within the range [0, MAX_FADE_DURATION], it assigns
 * this value to the location pointed to by `duration` and returns `true`. If the value
 * is out of range or not an integer, it logs an appropriate error message and returns `false`.
 *
 * @param jsonValue The JSON variant containing the duration value to validate.
 * @param duration A pointer to an `uint16_t` where the validated duration value will be stored.
 * @param ledId Optional. The ID of the LED being validated. Defaults to `0` for global duration.
 *
 * @return `true` if the duration value is valid and successfully assigned.
 * @return `false` if the duration value is invalid or not an integer.
 */
bool validateDuration(JsonVariant jsonValue, uint16_t *duration, int16_t ledId = 0)
{
    // Check if the JSON variant is of integer type
    if (jsonValue.is<int>())
    {
        // Retrieve the duration value as a signed 16-bit integer
        int16_t value = jsonValue.as<int>();

        // Validate that the duration value is within the acceptable range
        if (value < 0 || value > MAX_FADE_DURATION)
        {
            if (ledId > 0)
                // Log an error message specific to the LED ID with the invalid duration value
                Serial.printf("Invalid duration value for LED %d: %d. Must be between 0 and %d\n", ledId, value, MAX_FADE_DURATION);
            else
                // Log an error message for global duration with the invalid value
                Serial.printf("Invalid global duration value: %d. Must be between 0 and %d\n", value, MAX_FADE_DURATION);
            return false; // Indicate that validation failed
        }

        // Assign the validated duration value to the provided pointer
        *duration = value;
        return true; // Indicate successful validation
    }
    else
    {
        if (ledId > 0)
            // Log an error message if the duration value for a specific LED is not an integer
            Serial.printf("Error: LED %d duration value is not an integer.\n", ledId);
        else
            // Log an error message if the global duration value is not an integer
            Serial.println("Error: Global duration value is not an integer.");
        return false; // Indicate that validation failed
    }
}

/**
 * @brief Validates and retrieves the fade effect count from a JSON variant.
 *
 * This function checks if the provided `jsonValue` contains a valid count value.
 * If `jsonValue` is an integer within the range [1, MAX_FADE_REPEATS], it assigns
 * this value to the location pointed to by `count` and returns `true`. If the value
 * is out of range or not an integer, it logs an appropriate error message and returns `false`.
 *
 * @param jsonValue The JSON variant containing the count value to validate.
 * @param count A pointer to a `uint16_t` where the validated count value will be stored.
 * @param ledId Optional. The ID of the LED being validated. Defaults to `0`.
 *
 * @return `true` if the count value is valid and successfully assigned.
 * @return `false` if the count value is invalid or not an integer.
 */
bool validateCount(JsonVariant jsonValue, uint16_t *count, int16_t ledId = 0)
{
    // Check if the JSON variant is of integer type
    if (jsonValue.is<int>())
    {
        // Retrieve the count value as an unsigned 16-bit integer
        int16_t value = jsonValue.as<int>();

        // Validate that the count value is within the acceptable range
        if (value < 1 || value > MAX_FADE_REPEATS)
        {
            if (ledId > 0)
                // Log an error message specific to the LED ID with the invalid count value
                Serial.printf("Invalid count value for LED %d: %d. Must be between 1 and %d\n", ledId, value, MAX_FADE_REPEATS);
            else
                // Log an error message for global count with the invalid value
                Serial.printf("Invalid global count value: %d. Must be between 1 and %d\n", value, MAX_FADE_REPEATS);
            return false; // Indicate that validation failed
        }

        // Assign the validated count value to the provided pointer
        *count = static_cast<uint16_t>(value);
        return true; // Indicate successful validation
    }
    else
    {
        if (ledId > 0)
            // Log an error message if the count value for a specific LED is not an integer
            Serial.printf("Error: LED %d count value is not an integer.\n", ledId);
        else
            // Log an error message if the global count value is not an integer
            Serial.println("Error: Global count value is not an integer.");
        return false; // Indicate that validation failed
    }
}

/**
 * @brief Validates the color value (hex string).
 *
 * This function checks if the provided color value is a valid hexadecimal string.
 *
 * @param colorHex The color value to validate.
 * @param ledId Optional. The ID of the LED being validated. Defaults to `0`.
 * @return true if the color value is valid (hexadecimal string), false otherwise.
 */
bool isValidHexColor(String &colorHex, int16_t ledId = 0)
{
    // Check if the color hex string has exactly 6 characters
    if (colorHex.length() == 6)
    {
        // Iterate through each character to ensure it's a valid hexadecimal digit
        for (char c : colorHex)
        {
            if (!isxdigit(c))
            {
                if (ledId > 0)
                    // Log an error message specific to the LED ID with the invalid character
                    Serial.printf("Invalid character '%c' in color hex for LED %d.\n", c, ledId);
                else
                    // Log an error message for global color hex with the invalid character
                    Serial.printf("Invalid character '%c' in global color hex.\n", c);
                return false; // Indicate that validation failed
            }
        }
        return true; // Indicate successful validation
    }
    else
    {
        if (ledId > 0)
            // Log an error message if the color hex length for a specific LED is incorrect
            Serial.printf("Invalid color hex length for LED %d: %s. Expected 6 characters.\n", ledId, colorHex.c_str());
        else
            // Log an error message if the global color hex length is incorrect
            Serial.printf("Invalid global color hex length: %s. Expected 6 characters.\n", colorHex.c_str());
        return false; // Indicate that validation failed
    }
}

/**
 * @brief Validates the color palette array.
 *
 * This function checks if the provided JSON value is a valid color palette array.
 * The color palette array must be non-empty and contain valid hexadecimal color strings.
 *
 * @param jsonValue The JSON value to validate as a color palette array.
 * @param colors A reference to the array of color palettes.
 * @return true if the color palette is valid, false otherwise.
 */
bool validateColorPalette(JsonVariant jsonValue, JsonArray &colors)
{
    // Check if the JSON variant is an array
    if (!jsonValue.is<JsonArray>())
    {
        Serial.println("Error: Color palette is not an array.");
        return false;
    }

    // Retrieve the color palette as a JSON array
    colors = jsonValue.as<JsonArray>();

    // Check if the color palette is empty
    if (colors.size() == 0)
    {
        Serial.println("Error: Color palette is empty.");
        return false;
    }

    // Validate each color in the palette
    for (size_t i = 0; i < colors.size(); ++i)
    {
        // Check if the color value is a string
        if (!colors[i].is<String>())
        {
            Serial.printf("Error: Invalid color value at index %d.\n", i);
            return false;
        }

        // Retrieve the color value as a string
        String colorHex = colors[i].as<String>();

        // Validate the color value as a hexadecimal string
        if (!isValidHexColor(colorHex))
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Converts a hexadecimal color string to a CRGB object.
 *
 * @param colorHex The color in hexadecimal string format (e.g., "FFAABB").
 * @return The corresponding CRGB color object.
 */
CRGB convertHexToCRGB(String &colorHex)
{
    long color = strtol(colorHex.c_str(), nullptr, 16);
    int r = (color >> 16) & 0xFF;
    int g = (color >> 8) & 0xFF;
    int b = color & 0xFF;
    return CRGB(r, g, b);
}

/**
 * @brief Determines the color for an LED based on the provided configuration.
 *
 * Priority:
 * 1. LED_COLOR_KEY (color hex string)
 * 2. LED_COLOR_PALETTE_KEY (color index from colors array)
 * 3. Default to first color in palette
 *
 * @param ledConfig The JSON object containing LED configuration.
 * @param colors The array of color palettes.
 * @param color Pointer to a CRGB object where the determined color will be stored.
 * @param ledId The LED number for logging purposes.
 * @return true if the color was successfully determined and validated, false otherwise.
 */
bool getLedColor(JsonObject &ledConfig, JsonArray &colors, CRGB *color, uint16_t ledId)
{
    if (ledConfig[LED_COLOR_KEY].is<String>())
    {
        // LED_COLOR_KEY is provided: color string in hex format
        String colorHex = ledConfig[LED_COLOR_KEY].as<String>();

        // Validate color
        if (!isValidHexColor(colorHex, ledId))
            return false;

        *color = convertHexToCRGB(colorHex);
        return true;
    }
    else if (ledConfig[LED_COLOR_PALETTE_KEY].is<int>())
    {
        // LED_COLOR_PALETTE_KEY is provided: color index from colors array
        int colorIndex = ledConfig[LED_COLOR_PALETTE_KEY].as<int>();

        if (colorIndex < 0 || colorIndex >= colors.size())
        {
            Serial.printf("Error: Invalid color palette index %d for LED %d. Must be between 0 and %d\n",
                          colorIndex, ledId, colors.size() - 1);
            return false;
        }

        // Get color from palette without validation - it was validated when parsing the colors array
        String colorHex = colors[colorIndex].as<String>();
        *color = convertHexToCRGB(colorHex);
        return true;
    }
    else
    {
        // Use default color at index 0
        if (colors.size() == 0)
        {
            Serial.printf("Error: Color palette is empty. Cannot set default color for LED %d.\n", ledId);
            return false;
        }

        // Use default color from palette at index 0  without validation - it was validated when parsing the colors array
        String colorHex = colors[0].as<String>();
        *color = convertHexToCRGB(colorHex);
        return true;
    }
}

/**
 * @brief Parses and applies the configuration for a single LED.
 *
 * @param arrayIndex The zero-based index of the LED in the array.
 * @param ledConfig The JSON object containing a single LED's configuration.
 * @param globalBrightness The global brightness value to be used if not overridden.
 * @param globalDuration The global duration value to be used if not overridden.
 * @param globalCount The global count value to be used if not overridden.
 * @param colors The array of color palettes.
 */
void parseAndSetSingleLed(uint16_t arrayIndex, JsonObject &ledConfig, int16_t globalBrightness,
                          uint16_t globalDuration, uint16_t globalCount, JsonArray &colors)
{
    // Get LED ID if present, else use array index +1
    int16_t ledId = 0; // Use 0 as default LED ID to detect if it was set
    if (ledConfig[LED_ID_KEY].is<int>())
    {
        ledId = ledConfig[LED_ID_KEY].as<int>();
        if (ledId < 1 || ledId > LEDS_COUNT)
        {
            Serial.printf("Error: invalid LED ID: %d\n", ledId);
            return; // Skip invalid LED configurations
        }
    }

    // Use LED ID if present, else use array index. Subtract 1 to convert to zero-based index
    ledId = ledId > 0 ? ledId : arrayIndex + 1;

    // Determine the color to use
    CRGB ledColor = CRGB::Black; // Default color
    if (!getLedColor(ledConfig, colors, &ledColor, ledId))
        return; // Skip invalid LED configurations

    // Get brightness value, override global if specified
    uint16_t brightness = globalBrightness;
    if (!ledConfig[LED_BRIGHTNESS_KEY].isNull() &&
        !validateBrightness(ledConfig[LED_BRIGHTNESS_KEY], &brightness, ledId))
        return; // Skip invalid LED configurations

    // Get duration, override global if specified
    uint16_t duration = globalDuration;
    if (!ledConfig[LED_DURATION_KEY].isNull() &&
        !validateDuration(ledConfig[LED_DURATION_KEY], &duration, ledId))
        return; // Skip invalid LED configurations

    // Get count, override global if specified
    uint16_t count = globalCount;
    if (!ledConfig[LED_COUNT_KEY].isNull() &&
        !validateCount(ledConfig[LED_COUNT_KEY], &count))
        return; // Skip invalid LED configurations

    // Set the LED with the extracted parameters. The LED ID is 1-based, so we subtract 1
    setLed(ledId - 1, brightness, duration, count, ledColor, false);

    // Optional: Log the LED configuration for debugging
    // Serial.printf("LED %d - Brightness: %d, Duration: %d, Count: %d, Color: (%d, %d, %d)\n",
    //               ledId, brightness, duration, count, ledColor.r, ledColor.g, ledColor.b);
}

/**
 * @brief Parses the LEDs array from the JSON document and applies configurations.
 *
 * @param doc The JSON document containing LED configurations.
 * @param globalBrightness The global brightness value to be used if not overridden.
 * @param globalDuration The global duration value to be used if not overridden.
 * @param globalCount The global count value to be used if not overridden.
 * @param colors The array of color palettes.
 */
void parseLedsArray(JsonDocument &doc, int16_t globalBrightness, int16_t globalDuration,
                    uint16_t globalCount, JsonArray &colors)
{
    JsonArray leds = doc[LEDS_KEY].as<JsonArray>();
    if (leds.size() == 0)
    {
        Serial.println("No LED configurations provided");
        return;
    }

    // Iterate through each LED configuration
    for (size_t i = 0; i < leds.size(); ++i)
    {
        JsonObject ledConfig = leds[i].as<JsonObject>();
        parseAndSetSingleLed(i, ledConfig, globalBrightness, globalDuration, globalCount, colors);
    }
}

/**
 * @brief Parses the entire LED configuration from a JSON document and applies settings.
 *
 * This function orchestrates the parsing process by extracting the global settings and individual LED configurations.
 *
 * @param doc The JSON document containing LED configurations.
 */
void setLedsFromJsonDoc(JsonDocument &doc)
{
    // Get the global brightness value or use the default if not specified
    uint16_t globalBrightness = DEFAULT_BRIGHTNESS;
    if (!doc[BRIGHTNESS_KEY].isNull() && !validateBrightness(doc[BRIGHTNESS_KEY], &globalBrightness))
        return; // Exit if brightness is not properly defined

    // Get the global duration value or use the default if not specified
    uint16_t globalDuration = DEFAULT_DURATION;
    if (!doc[DURATION_KEY].isNull() && !validateDuration(doc[DURATION_KEY], &globalDuration))
        return; // Exit if duration is not properly defined

    // Get the global count value or use the default if not specified
    uint16_t globalCount = DEFAULT_COUNT;
    if (!doc[COUNT_KEY].isNull() && !validateCount(doc[COUNT_KEY], &globalCount))
        return; // Exit if count is not properly defined

    // Get the color palette or initialize an empty array
    JsonArray colors;
    if (!doc[COLOR_PALETTE_KEY].isNull() && !validateColorPalette(doc[COLOR_PALETTE_KEY], colors))
        return; // Exit if colors are not properly defined

    // Parse and apply LED configurations
    parseLedsArray(doc, globalBrightness, globalDuration, globalCount, colors);
}
