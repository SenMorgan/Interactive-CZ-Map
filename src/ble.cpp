#include <NimBLEDevice.h>
#include "config_parser.h"
#include "leds.h"
#include "ble.h"

// Task parameters
#define BLE_TASK_STACK_SIZE (8 * 1024U)
#define BLE_TASK_PRIORITY   (tskIDLE_PRIORITY)
#define BLE_TASK_CORE       CONFIG_BT_NIMBLE_PINNED_TO_CORE // Use the core that NimBLE is pinned to

const uint16_t APPEARANCE_HID_KEYBOARD = 0x03C1;
const char HID_SERVICE[] = "1812";     // Human Interface Device Service
const char HID_REPORT_DATA[] = "2A4D"; // Report Data Characteristic

static const NimBLEAdvertisedDevice *advDevice;
static uint32_t scanTimeMs = 5000; // Scan time in ms, 0 → scan forever

static bool buttonClicked = false;
SemaphoreHandle_t disconnectSemaphore;

// Client Callbacks
class ClientCallbacks : public NimBLEClientCallbacks
{
    void onConnect(NimBLEClient *pClient) override
    {
        Serial.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());
    }

    void onDisconnect(NimBLEClient *pClient, int reason) override
    {
        Serial.printf("%s Disconnected, reason = %d\n", pClient->getPeerAddress().toString().c_str(), reason);

        // Stop the LED effect if device is disconnected during button press
        if (buttonClicked)
        {
            buttonClicked = false;
            blinkWithSingleLed(devConfig.baseLedId, CRGB::Black, 1, 1);
        }

        // Notify the bleTask to resume scanning
        xSemaphoreGive(disconnectSemaphore);
    }
} clientCallbacks;

// Scan Callbacks
class ScanCallbacks : public NimBLEScanCallbacks
{
    void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override
    {
        // Check if device address matches the HID device from the configuration
        if (advertisedDevice->getAddress().equals(devConfig.bleHidAddress))
        {
            // Check if the device is a HID keyboard
            if (advertisedDevice->getAppearance() == APPEARANCE_HID_KEYBOARD &&
                advertisedDevice->isAdvertisingService(NimBLEUUID(HID_SERVICE)))
            {
                Serial.printf("Found HID Device: %s\n", advertisedDevice->toString().c_str());
                NimBLEDevice::getScan()->stop();
                advDevice = advertisedDevice;
                // Connection will be handled in bleTask
            }
            else
            {
                Serial.println("Found device, but it is not a HID device");
            }
        }
    }

    void onScanEnd(const NimBLEScanResults &results, int reason) override
    {
        Serial.printf("Scan Ended, reason: %d, device count: %d; Restarting scan\n", reason, results.getCount());
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
} scanCallbacks;

// Notification Callback
void notifyCB(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    std::string str = (isNotify) ? "Notification" : "Indication";
    str += " from ";
    str += pRemoteCharacteristic->getClient()->getPeerAddress().toString();
    str += ": Service = " + pRemoteCharacteristic->getRemoteService()->getUUID().toString();
    str += ", Characteristic = " + pRemoteCharacteristic->getUUID().toString();
    str += ", Value = " + std::string((char *)pData, length);
    Serial.printf("%s\n", str.c_str());

    // Handle button press/release
    if (length == 2 && pData[0] == 0x02 && pData[1] == 0x00)
    {
        Serial.println("Button pressed");
        buttonClicked = true;

        // Blink with base LED to indicate the button press
        blinkWithSingleLed(devConfig.baseLedId, CRGB::White, 100, LOOP_INDEFINITELY);
    }
    else if (length == 2 && pData[0] == 0x00 && pData[1] == 0x00)
    {
        Serial.println("Button released");
        buttonClicked = false;

        // Stop the LED effect
        blinkWithSingleLed(devConfig.baseLedId, CRGB::Black, 1, 1);
    }
    else
    {
        Serial.print("Unknown data (" + String(length) + " bytes): ");
        for (size_t i = 0; i < length; i++)
        {
            Serial.printf("%02X ", pData[i]);
        }
        Serial.println();
    }
}

// Connect to HID Device
bool connectToHID()
{
    NimBLEClient *pClient = nullptr;

    if (NimBLEDevice::getCreatedClientCount())
    {
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if (pClient)
        {
            if (!pClient->connect(advDevice, false))
            {
                Serial.println("Reconnect failed!");
                return false;
            }
            Serial.println("Reconnected client");
        }
        else
        {
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    if (!pClient)
    {
        if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS)
        {
            Serial.println("Max clients reached - no more connections available!");
            return false;
        }

        pClient = NimBLEDevice::createClient();
        Serial.println("New client created");

        pClient->setClientCallbacks(&clientCallbacks, false);
        pClient->setConnectionParams(12, 12, 0, 150);
        pClient->setConnectTimeout(5000);

        if (!pClient->connect(advDevice))
        {
            NimBLEDevice::deleteClient(pClient);
            Serial.println("Failed to connect, deleted client");
            return false;
        }
    }

    if (!pClient->isConnected())
    {
        if (!pClient->connect(advDevice))
        {
            Serial.println("Failed to connect");
            return false;
        }
    }

    Serial.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());

    NimBLERemoteService *pSvc = pClient->getService(NimBLEUUID(HID_SERVICE));
    if (pSvc)
    {
        // Get all characteristics (somehow pSvc->getCharacteristic(NimBLEUUID(HID_REPORT_DATA)) does not work)
        const std::vector<NimBLERemoteCharacteristic *> &charvector = pSvc->getCharacteristics(true);

        Serial.printf("Number of characteristics found: %d\n", charvector.size());
        for (const auto &chr : charvector)
        {
            Serial.printf("Characteristic UUID: %s\n", chr->getUUID().toString().c_str());

            // Subscribe to HID Report Data characteristic
            if (chr->getUUID().equals(NimBLEUUID(HID_REPORT_DATA)) && chr->canNotify())
            {
                if (!chr->subscribe(true, notifyCB))
                {
                    Serial.println("Failed to subscribe to HID Report characteristic");
                    pClient->disconnect();
                    return false;
                }
                else
                {
                    Serial.println("Subscribed to HID Report characteristic");
                }
            }
        }
    }
    else
    {
        Serial.println("HID Service not found");
    }

    Serial.println("Done with this device!");
    return true;
}

/**
 * @brief The BLE task.
 *
 * @param pvParameters Pointer to task parameters (unused).
 */
void bleTask(void *pvParameters)
{
    Serial.println("Starting Interactive-CZ-Map BLE Task");
    NimBLEDevice::init("Interactive-CZ-Map");
    NimBLEDevice::setPower(3); /** +3dBm */

    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(&scanCallbacks, false);
    pScan->setInterval(100);
    pScan->setWindow(100);

    // Initialize semaphore
    disconnectSemaphore = xSemaphoreCreateBinary();

    for (;;)
    {
        // Start scanning
        Serial.println("Scanning for HID devices...");
        pScan->start(scanTimeMs, false);

        // Wait to find a device to connect
        while (!advDevice)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        if (connectToHID())
        {
            Serial.println("HID device connected!");

            // Wait for disconnection
            if (xSemaphoreTake(disconnectSemaphore, portMAX_DELAY))
            {
                Serial.println("Disconnected. Restarting scan");
                advDevice = nullptr;
            }
        }
        else
        {
            Serial.println("Failed to connect. Restarting scan");
            advDevice = nullptr;
        }
    }
}

// Initialize BLE Task
void bleTaskInit()
{
    if (xTaskCreatePinnedToCore(bleTask,
                                "bleTask",
                                BLE_TASK_STACK_SIZE,
                                NULL,
                                BLE_TASK_PRIORITY,
                                NULL,
                                BLE_TASK_CORE) != pdPASS)
    {
        Serial.println("Failed to create bleTask");
    }
}