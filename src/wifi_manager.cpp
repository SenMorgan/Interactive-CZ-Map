/**
 * @file wifi_manager.cpp
 * @brief WiFi Manager implementation for ESP32.
 *
 * This code was hardly inspired by examples from https://github.com/khoih-prog/ESPAsync_WiFiManager
 */

#include <LittleFS.h>
#include <WiFiMulti.h>
#include "constants.h"
#include "custom_html.h"
#include "drd.h"
#include "leds.h"
#include "wifi_manager.h"

// Configure AsyncWiFiManager
#define USE_AVAILABLE_PAGES        true
#define USE_ESP_WIFIMANAGER_NTP    false
#define USE_STATIC_IP_CONFIG_IN_CP false
#include <ESPAsync_WiFiManager.h> //https://github.com/khoih-prog/ESPAsync_WiFiManager

#define CONFIG_FILENAME F("/wifi_cred.dat")

#define MIN_AP_PASSWORD_SIZE 8

#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64 // WPA2 passwords can be up to 63 characters long + null

#define HTTP_PORT            80
#define NUM_WIFI_CREDENTIALS 2

#define WIFI_MULTI_1ST_CONNECT_WAITING_MS 800L
#define WIFI_MULTI_CONNECT_WAITING_MS     500L

#define WIFI_CHECK_INTERVAL 1000L

// How long to wait for the automatic reconnect of the WiFi driver after the connection is lost,
// before the reconnect through WiFiMulti is forced and indicated on the map (in milliseconds)
#define WIFI_RECONNECT_GRACE_PERIOD 5000L

typedef struct
{
    char wifi_ssid[SSID_MAX_LEN];
    char wifi_pw[PASS_MAX_LEN];
} WiFi_Credentials;

typedef struct
{
    String wifi_ssid;
    String wifi_pw;
} WiFi_Credentials_String;

typedef struct
{
    WiFi_Credentials WiFi_Creds[NUM_WIFI_CREDENTIALS];
    uint16_t checksum;
} WM_Config;

WM_Config WM_config;

WiFiMulti wifiMulti;
FS *filesystem;

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig; // = false;

/**
 * @brief Connects to one of the access points added to WiFiMulti.
 *
 * The function blocks until the connection is established. The connection attempt is
 * indicated on the map. If the connection fails, the ESP32 is restarted.
 *
 * @note The access points are added to WiFiMulti in initWiFiManager().
 */
void connectMultiWiFi()
{
    // Indicate connecting to WiFi
    ledsSetStatus(LedsStatus::WIFI_CONNECTING);

    LOGERROR(F("ConnectMultiWiFi with :"));

    if ((Router_SSID != ""))
    {
        LOGERROR1(F("* Flash-stored Router_SSID = "), Router_SSID);
    }

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
        // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
        if ((String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE))
        {
            LOGERROR1(F("* Additional SSID = "), WM_config.WiFi_Creds[i].wifi_ssid);
        }
    }

    LOGERROR(F("Connecting MultiWifi..."));

    int i = 0;

    uint8_t status = wifiMulti.run();
    delay(WIFI_MULTI_1ST_CONNECT_WAITING_MS);

    while ((i++ < 20) && (status != WL_CONNECTED))
    {
        status = WiFi.status();

        if (status == WL_CONNECTED)
            break;
        else
            delay(WIFI_MULTI_CONNECT_WAITING_MS);
    }

    if (status == WL_CONNECTED)
    {
        // The WiFi indication stays on the map until the AWS module takes over:
        // it switches to the AWS indication or clears it if the MQTT connection is alive
        LOGERROR1(F("WiFi connected after time: "), i);
        LOGERROR3(F("SSID:"), WiFi.SSID(), F(",RSSI="), WiFi.RSSI());
        LOGERROR3(F("Channel:"), WiFi.channel(), F(",IP address:"), WiFi.localIP());
    }
    else
    {
        LOGERROR(F("WiFi not connected"));

        // Restart after unsuccessful connection
        ESP.restart();
    }
}

int calcChecksum(uint8_t *address, uint16_t sizeToCalc)
{
    uint16_t checkSum = 0;

    for (uint16_t index = 0; index < sizeToCalc; index++)
    {
        checkSum += *(((byte *)address) + index);
    }

    return checkSum;
}

bool loadConfigData()
{
    File file = LittleFS.open(CONFIG_FILENAME, "r");
    LOGERROR(F("LoadWiFiCfgFile "));

    memset((void *)&WM_config, 0, sizeof(WM_config));

    if (file)
    {
        file.readBytes((char *)&WM_config, sizeof(WM_config));

        file.close();
        LOGERROR(F("OK"));

        if (WM_config.checksum != calcChecksum((uint8_t *)&WM_config, sizeof(WM_config) - sizeof(WM_config.checksum)))
        {
            LOGERROR(F("WM_config checksum wrong"));

            return false;
        }

        return true;
    }
    else
    {
        LOGERROR(F("failed"));

        return false;
    }
}

void saveConfigData()
{
    File file = LittleFS.open(CONFIG_FILENAME, "w");
    LOGERROR(F("SaveWiFiCfgFile "));

    if (file)
    {
        WM_config.checksum = calcChecksum((uint8_t *)&WM_config, sizeof(WM_config) - sizeof(WM_config.checksum));

        file.write((uint8_t *)&WM_config, sizeof(WM_config));

        file.close();
        LOGERROR(F("OK"));
    }
    else
    {
        LOGERROR(F("failed"));
    }
}

/**
 * @brief Initializes the WiFi Manager and sets up the WiFi connection.
 *
 * @param chipID The unique chip ID of the ESP32 device.
 */
void initWiFiManager(const char *chipID)
{
    Serial.println(F("Initializing WiFi Manager..."));

    // Format LittleFS on fail
    if (!LittleFS.begin(true))
    {
        Serial.println(F("Initializing LittleFS failed! Already tried formatting."));

        if (!LittleFS.begin())
        {
            Serial.println(F("Initializing LittleFS failed!. Please use SPIFFS or EEPROM. Stay forever..."));

            // Indicate error
            ledsSetStatus(LedsStatus::FATAL_ERROR);

            while (true)
                delay(1);
        }
    }

    Serial.println(F("LittleFS initialized"));

    // Initialize Double Reset Detection for starting Config Portal if DRD
    drdTaskInit(DRD_TIMEOUT);

    unsigned long startedAt = millis();

    // Compose hostname from chipID and set it. The String must stay alive while the pointer is used
    String hostnameStr = String(HOSTNAME_PREFIX) + "_" + String(chipID);
    const char *hostname = hostnameStr.c_str();
    WiFi.setHostname(hostname);
    Serial.printf("Hostname: %s\n", hostname);

    AsyncWebServer webServer(HTTP_PORT);
    AsyncDNSServer dnsServer;
    ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer, hostname);

    // Set config portal channel, default = 1. Use 0 => random channel from 1-11
    ESPAsync_wifiManager.setConfigPortalChannel(0);

    // Add custom styles
    ESPAsync_wifiManager.setCustomHeadElement(CUSTOM_HEAD_ELEMENT);

    // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
    // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
    // Have to create a new function to store in EEPROM/SPIFFS for this purpose
    Router_SSID = ESPAsync_wifiManager.WiFi_SSID();
    Router_Pass = ESPAsync_wifiManager.WiFi_Pass();

    // Do not print the WiFi password
    Serial.println("ESP Self-Stored: SSID = " + Router_SSID);

    bool configDataLoaded = false;

    // Don't permit NULL password
    if ((Router_SSID != "") && (Router_Pass != ""))
    {
        LOGERROR1(F("* Add SSID = "), Router_SSID);
        wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());

        ESPAsync_wifiManager.setConfigPortalTimeout(120); // If no access point name has been previously entered disable timeout.
        Serial.println(F("Got ESP Self-Stored Credentials. Timeout 120s for Config Portal"));
    }

    if (loadConfigData())
    {
        configDataLoaded = true;

        ESPAsync_wifiManager.setConfigPortalTimeout(120); // If no access point name has been previously entered disable timeout.
        Serial.println(F("Got stored Credentials. Timeout 120s for Config Portal"));
    }
    else
    {
        // Enter CP only if no stored SSID on flash and file
        Serial.println(F("Open Config Portal without Timeout: No stored Credentials."));
        initialConfig = true;
    }

    if (isDoubleResetDetected())
    {
        // DRD, disable timeout.
        ESPAsync_wifiManager.setConfigPortalTimeout(0);

        Serial.println(F("Open Config Portal without Timeout: Double Reset Detected"));
        initialConfig = true;
    }

    if (initialConfig)
    {
        Serial.println(F("Starting Config Portal. SSID: " AP_SSID));

        // New. Update Credentials, got from loadConfigData(), to display on CP
        ESPAsync_wifiManager.setCredentials(WM_config.WiFi_Creds[0].wifi_ssid, WM_config.WiFi_Creds[0].wifi_pw,
                                            WM_config.WiFi_Creds[1].wifi_ssid, WM_config.WiFi_Creds[1].wifi_pw);

        // Indicate that Config Portal is running
        ledsSetStatus(LedsStatus::CONFIG_PORTAL);

        // Blocking loop waiting to enter Config Portal and update WiFi Credentials
        if (!ESPAsync_wifiManager.startConfigPortal(AP_SSID, AP_PASSWORD))
            Serial.println(F("Not connected to WiFi but continuing anyway."));
        else
        {
            Serial.println(F("WiFi connected...yeey :)"));
        }

        // Clear WiFI credentials from memory
        memset(&WM_config, 0, sizeof(WM_config));

        for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
        {
            String tempSSID = ESPAsync_wifiManager.getSSID(i);
            String tempPW = ESPAsync_wifiManager.getPW(i);

            if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1)
                strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
            else
                strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);

            if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1)
                strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
            else
                strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);

            // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
            if ((String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE))
            {
                LOGERROR1(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid);
                wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
            }
        }

        // Save updated Credentials to memory
        saveConfigData();
    }

    startedAt = millis();

    if (!initialConfig)
    {
        // Load stored data, the addAP ready for MultiWiFi reconnection
        if (!configDataLoaded)
            loadConfigData();

        // Loop through WiFi creds and add them to WiFiMulti
        for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
        {
            // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
            if ((String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE))
            {
                LOGERROR1(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid);
                wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
            }
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println(F("ConnectMultiWiFi in setup"));

            connectMultiWiFi();
        }
    }
    else
    {
        Serial.print(F("After waiting "));
        Serial.print((float)(millis() - startedAt) / 1000);
        Serial.print(F(" secs more in setup(), connection result is "));

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.print(F("connected. Local IP: "));
            Serial.println(WiFi.localIP());
        }
        else
        {
            Serial.println(ESPAsync_wifiManager.getStatus(WiFi.status()));
        }
    }
}

/**
 * @brief Handles WiFi connection status and attempts reconnection if disconnected.
 *
 * This function checks the WiFi connection status at regular intervals defined by WIFI_CHECK_INTERVAL.
 * If the WiFi is disconnected, it first gives the WiFi driver WIFI_RECONNECT_GRACE_PERIOD to reconnect
 * on its own, so that a short dropout does not interrupt the LED effects. After that it reconnects
 * through WiFiMulti, which blocks, indicates the attempt on the map and restarts the ESP32 on failure.
 *
 * @note This function should be called periodically within the main loop to ensure continuous WiFi connectivity.
 */
void handleWiFi()
{
    static uint32_t lastWiFiCheck = 0;
    static uint32_t wifiLostTime = 0;
    static bool wifiLost = false;
    uint32_t timeNow = millis();

    // Check WiFi every WIFI_CHECK_INTERVAL milliseconds.
    if (timeNow - lastWiFiCheck > WIFI_CHECK_INTERVAL)
    {
        lastWiFiCheck = timeNow;

        if (WiFi.status() == WL_CONNECTED)
        {
            if (wifiLost)
            {
                Serial.println(F("WiFi reconnected"));
                wifiLost = false;
            }
        }
        else if (!wifiLost)
        {
            Serial.println(F("\nWiFi lost. Waiting for automatic reconnect"));
            wifiLost = true;
            wifiLostTime = timeNow;
        }
        else if (timeNow - wifiLostTime >= WIFI_RECONNECT_GRACE_PERIOD)
        {
            Serial.println(F("WiFi still lost. Call connectMultiWiFi in loop"));
            connectMultiWiFi();
            wifiLost = false;
        }
    }
}