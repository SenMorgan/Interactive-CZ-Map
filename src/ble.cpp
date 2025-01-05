#include <NimBLEDevice.h>
#include "ble.h"
#include "leds.h"

// Task parameters
#define BLE_TASK_STACK_SIZE (8 * 1024U)
#define BLE_TASK_PRIORITY   (tskIDLE_PRIORITY)
#define BLE_TASK_CORE       CONFIG_BT_NIMBLE_PINNED_TO_CORE // Use the core that NimBLE is pinned to

const char HID_SERVICE[] = "1812";     // Human Interface Device Service
const char HID_REPORT_DATA[] = "2A4D"; // Report Data Characteristic

static const NimBLEAdvertisedDevice *advDevice;
static bool doConnect = false;
static uint32_t scanTimeMs = 5000; /** scan time in milliseconds, 0 = scan forever */

class ClientCallbacks : public NimBLEClientCallbacks
{
    void onConnect(NimBLEClient *pClient) override
    {
        Serial.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());
    }

    void onDisconnect(NimBLEClient *pClient, int reason) override
    {
        Serial.printf("%s Disconnected, reason = %d - Starting scan\n", pClient->getPeerAddress().toString().c_str(), reason);
        NimBLEDevice::getScan()->start(scanTimeMs);
    }
} clientCallbacks;

/** Define a class to handle the callbacks when scan events are received */
class ScanCallbacks : public NimBLEScanCallbacks
{
    void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override
    {
        uint8_t advType = advertisedDevice->getAdvType();
        if ((advType == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_HD) ||
            (advType == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_LD) ||
            (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(NimBLEUUID(HID_SERVICE))))
        {
            Serial.printf("Found Our Service\n");
            /** stop scan before connecting */
            NimBLEDevice::getScan()->stop();
            /** Save the device reference in a global for the client to use*/
            advDevice = advertisedDevice;
            /** Ready to connect now */
            doConnect = true;
        }
    }

    /** Callback to process the results of the completed scan or restart it */
    void onScanEnd(const NimBLEScanResults &results, int reason) override
    {
        Serial.printf("Scan Ended, reason: %d, device count: %d; Restarting scan\n", reason, results.getCount());
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
} scanCallbacks;

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    std::string str = (isNotify == true) ? "Notification" : "Indication";
    str += " from ";
    str += pRemoteCharacteristic->getClient()->getPeerAddress().toString();
    str += ": Service = " + pRemoteCharacteristic->getRemoteService()->getUUID().toString();
    str += ", Characteristic = " + pRemoteCharacteristic->getUUID().toString();
    str += ", Value = " + std::string((char *)pData, length);
    Serial.printf("%s\n", str.c_str());

    // If data are 0x0200, then the button is pressed, otherwise it is released
    if (length == 2 && pData[0] == 0x02 && pData[1] == 0x00)
    {
        Serial.println("Button pressed");

        // Blink with LEDs to indicate the button press
        circleLedEffect(CRGB::White, 100, LOOP_INDEFINITELY);
    }
    else if (length == 2 && pData[0] == 0x00 && pData[1] == 0x00)
    {
        Serial.println("Button released");

        // Stop the LED effect
        circleLedEffect(CRGB::Black, 1, 1);
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

/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer()
{
    NimBLEClient *pClient = nullptr;

    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getCreatedClientCount())
    {
        /**
         *  Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if (pClient)
        {
            if (!pClient->connect(advDevice, false))
            {
                Serial.printf("Reconnect failed\n");
                return false;
            }
            Serial.printf("Reconnected client\n");
        }
        else
        {
            /**
             *  We don't already have a client that knows this device,
             *  check for a client that is disconnected that we can use.
             */
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    /** No client to reuse? Create a new one. */
    if (!pClient)
    {
        if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS)
        {
            Serial.printf("Max clients reached - no more connections available\n");
            return false;
        }

        pClient = NimBLEDevice::createClient();

        Serial.printf("New client created\n");

        pClient->setClientCallbacks(&clientCallbacks, false);
        /**
         *  Set initial connection parameters:
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 150 * 10ms = 1500ms timeout
         */
        pClient->setConnectionParams(12, 12, 0, 150);

        /** Set how long we are willing to wait for the connection to complete (milliseconds), default is 30000. */
        pClient->setConnectTimeout(5 * 1000);

        if (!pClient->connect(advDevice))
        {
            /** Created a client but failed to connect, don't need to keep it as it has no data */
            NimBLEDevice::deleteClient(pClient);
            Serial.printf("Failed to connect, deleted client\n");
            return false;
        }
    }

    if (!pClient->isConnected())
    {
        if (!pClient->connect(advDevice))
        {
            Serial.printf("Failed to connect\n");
            return false;
        }
    }

    Serial.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());

    /** Now we can read/write/subscribe the characteristics of the services we are interested in */
    NimBLERemoteService *pSvc = nullptr;
    NimBLERemoteCharacteristic *pChr = nullptr;
    NimBLERemoteDescriptor *pDsc = nullptr;

    pSvc = pClient->getService(NimBLEUUID(HID_SERVICE));
    if (pSvc)
    {
        // Get all characteristics for debugging
        const std::vector<NimBLERemoteCharacteristic *> &charvector = pSvc->getCharacteristics(true);

        Serial.printf("Number of characteristics found: %d\n", charvector.size());
        for (const auto &chr : charvector)
        {
            Serial.printf("Characteristic UUID: %s\n", chr->getUUID().toString().c_str());

            // Subscribe to HID Report characteristic (UUID: 2A4D)
            if (chr->getUUID().equals(NimBLEUUID("2A4D")) && chr->canNotify())
            {
                chr->subscribe(true, notifyCB);
                Serial.println("Subscribed to HID Report characteristic");
            }
        }

        pChr = pSvc->getCharacteristic(NimBLEUUID(HID_REPORT_DATA));
    }
    else
    {
        Serial.println("HID Service not found");
    }

    if (pChr)
    {
        if (pChr->canRead())
        {
            Serial.printf("%s Value: %s\n", pChr->getUUID().toString().c_str(), pChr->readValue().c_str());
        }

        if (pChr->canNotify())
        {
            if (!pChr->subscribe(true, notifyCB))
            {
                pClient->disconnect();
                return false;
            }
        }
        else if (pChr->canIndicate())
        {
            /** Send false as first argument to subscribe to indications instead of notifications */
            if (!pChr->subscribe(false, notifyCB))
            {
                pClient->disconnect();
                return false;
            }
        }
    }
    else
    {
        Serial.printf("HID service not found.\n");
    }

    Serial.printf("Done with this device!\n");
    return true;
}

void setupBle()
{
}
/**
 * @brief The BLE task.
 *
 * @param pvParameters Pointer to task parameters (unused).
 */
void bleTask(void *pvParameters)
{
    /** Initialize NimBLE and set the device name */
    Serial.printf("Starting Interactive-Map BLE Client\n");
    NimBLEDevice::init("Interactive-Map");
    NimBLEDevice::setPower(3); /** +3db */

    NimBLEScan *pScan = NimBLEDevice::getScan();

    /** Set the callbacks to call when scan events occur, no duplicates */
    pScan->setScanCallbacks(&scanCallbacks, false);

    /** Set scan interval (how often) and window (how long) in milliseconds */
    pScan->setInterval(100);
    pScan->setWindow(100);

    /**
     * Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
    pScan->setActiveScan(true);

    /** Start scanning for advertisers */
    pScan->start(scanTimeMs);
    Serial.printf("Scanning for peripherals\n");

    // Main task loop
    for (;;)
    {
        if (doConnect) // TODO: use a semaphore instead of a flag
        {
            doConnect = false;
            if (connectToServer())
            {
                Serial.printf("Success! We should now be getting notifications!\n");
            }
            else
            {
                Serial.printf("Failed to connect, starting scan\n");
                NimBLEDevice::getScan()->start(scanTimeMs);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Initializes the BLE task.
 *
 * This function creates a new task called "bleTask" and assigns it to a specific core.
 *
 * @note This function should be called once during the setup phase of the program.
 */
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