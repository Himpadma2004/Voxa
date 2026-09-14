#include "BluetoothManager.h"

namespace VOXA
{
    BluetoothManager bluetoothManager;

    BluetoothManager& BluetoothManager::instance()
    {
        return bluetoothManager;
    }

    BluetoothManager::BluetoothManager()
    {
    }

    static const uint8_t hidReportDescriptor[] = {
        0x05, 0x0C,        // USAGE_PAGE (Consumer Devices)
        0x09, 0x01,        // USAGE (Consumer Control)
        0xA1, 0x01,        // COLLECTION (Application)
        0x85, 0x01,        //   REPORT_ID (1)
        0x15, 0x00,        //   LOGICAL_MINIMUM (0)
        0x25, 0x01,        //   LOGICAL_MAXIMUM (1)
        0x75, 0x01,        //   REPORT_SIZE (1)
        0x95, 0x07,        //   REPORT_COUNT (7)
        0x09, 0xB5,        //   USAGE (Scan Next Track)      - bit 0 (0x01)
        0x09, 0xB6,        //   USAGE (Scan Previous Track)  - bit 1 (0x02)
        0x09, 0xB7,        //   USAGE (Stop)                 - bit 2 (0x04)
        0x09, 0xCD,        //   USAGE (Play/Pause)           - bit 3 (0x08)
        0x09, 0xE2,        //   USAGE (Mute)                 - bit 4 (0x10)
        0x09, 0xE9,        //   USAGE (Volume Increment)     - bit 5 (0x20)
        0x09, 0xEA,        //   USAGE (Volume Decrement)     - bit 6 (0x40)
        0x81, 0x02,        //   INPUT (Data,Var,Abs)
        0x95, 0x01,        //   REPORT_COUNT (1)
        0x81, 0x01,        //   INPUT (Cnst,Ary,Abs) (padding bit 7)
        0xC0               // END_COLLECTION
    };

    class VoxaSecurityCallbacks : public BLESecurityCallbacks
    {
        uint32_t onPassKeyRequest() override { return 0; }
        void onPassKeyNotify(uint32_t pass_key) override {}
        bool onConfirmPIN(uint32_t pass_key) override { return true; }
        bool onSecurityRequest() override { return true; }
        void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override
        {
            Serial.printf("[BluetoothManager] BLE Authentication result: %s\n", cmpl.success ? "SUCCESS" : "FAILED");
        }
    };

    void BluetoothManager::begin()
    {
        if (m_initialized) return;

        Serial.println("[BluetoothManager] Initializing ESP32-S3 BLE HID Controller (VOXA Bluetooth)...");
        BLEDevice::init("VOXA Bluetooth");

        // 1. Security Configuration for Windows & Mobile HID Driver Attachment
        BLEDevice::setSecurityCallbacks(new VoxaSecurityCallbacks());

        BLESecurity *pSecurity = new BLESecurity();
        pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
        pSecurity->setCapability(ESP_IO_CAP_NONE);
        pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

        // 2. Set up BLE Server
        BLEServer* pServer = BLEDevice::createServer();
        if (pServer)
        {
            pServer->setCallbacks(this);

            // 3. Set up BLE HID Device Service (0x1812)
            BLEHIDDevice* hid = new BLEHIDDevice(pServer);
            m_inputReportChar = hid->inputReport(1); // Report ID 1
            hid->manufacturer()->setValue("VOXA Tech");
            hid->pnp(0x02, 0x05ac, 0x022c, 0x0110);
            hid->hidInfo(0x00, 0x01);
            hid->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
            hid->startServices();

            // 4. Advertising configuration with HID Appearance
            BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
            pAdvertising->setAppearance(0x03C0); // Generic HID
            pAdvertising->addServiceUUID(hid->hidService()->getUUID());
            pAdvertising->addServiceUUID(hid->deviceInfo()->getUUID());
            pAdvertising->addServiceUUID(hid->batteryService()->getUUID());
            pAdvertising->setScanResponse(true);
            pAdvertising->setMinPreferred(0x06);
            pAdvertising->setMinPreferred(0x12);
            pAdvertising->start();
            Serial.println("[BluetoothManager] BLE HID Advertising Started as 'VOXA Bluetooth'");
        }
        
        m_bleScan = BLEDevice::getScan();
        if (m_bleScan)
        {
            m_bleScan->setAdvertisedDeviceCallbacks(this);
            m_bleScan->setActiveScan(true);
            m_bleScan->setInterval(100);
            m_bleScan->setWindow(99);
        }

        m_initialized = true;
        m_enabled = true;
        Serial.println("[BluetoothManager] ESP32-S3 BLE Controller Ready!");
    }

    void BluetoothManager::onConnect(BLEServer* pServer)
    {
        m_phoneConnected = true;
        Serial.println("[BluetoothManager] Phone/PC Host Attached via BLE HID!");
    }

    void BluetoothManager::onDisconnect(BLEServer* pServer)
    {
        m_phoneConnected = false;
        Serial.println("[BluetoothManager] Phone/PC Host Detached. Resuming BLE HID Advertising...");
        pServer->startAdvertising();
    }

    void BluetoothManager::sendMediaCommand(MediaCommand cmd)
    {
        uint8_t keyMask = 0;
        const char* cmdName = "UNKNOWN";

        switch (cmd)
        {
            case MediaCommand::PlayPause:
                cmdName = "PLAY_PAUSE";
                keyMask = 0x08; // bit 3 (Play/Pause 0xCD)
                break;
            case MediaCommand::Next:
                cmdName = "NEXT_TRACK";
                keyMask = 0x01; // bit 0 (Scan Next Track 0xB5)
                break;
            case MediaCommand::Prev:
                cmdName = "PREV_TRACK";
                keyMask = 0x02; // bit 1 (Scan Prev Track 0xB6)
                break;
            case MediaCommand::VolumeUp:
                cmdName = "VOLUME_UP";
                keyMask = 0x20; // bit 5 (Volume Increment 0xE9)
                break;
            case MediaCommand::VolumeDown:
                cmdName = "VOLUME_DOWN";
                keyMask = 0x40; // bit 6 (Volume Decrement 0xEA)
                break;
        }

        Serial.printf("[BluetoothManager] Sending HID Consumer Key: %s (0x%02X)\n", cmdName, keyMask);

        if (m_inputReportChar && m_phoneConnected)
        {
            uint8_t report[1] = { keyMask };
            m_inputReportChar->setValue(report, sizeof(report));
            m_inputReportChar->notify();
            delay(15);
            report[0] = 0; // Key Release
            m_inputReportChar->setValue(report, sizeof(report));
            m_inputReportChar->notify();
        }
    }

    void BluetoothManager::setEnabled(bool enable)
    {
        m_enabled = enable;
        if (!enable && m_isScanning)
        {
            stopScan();
        }
        Serial.printf("[BluetoothManager] BLE State -> %s\n", enable ? "ENABLED" : "DISABLED");
    }

    void BluetoothManager::startScan(uint32_t durationSecs)
    {
        if (!m_initialized) begin();
        if (!m_enabled || m_isScanning) return;

        Serial.printf("[BluetoothManager] Starting BLE Active Scan for %u seconds...\n", durationSecs);
        m_discoveredDevices.clear();
        m_isScanning = true;

        if (m_bleScan)
        {
            // Asynchronous BLE scan
            m_bleScan->start(durationSecs, false);
        }
        m_isScanning = false;
        Serial.printf("[BluetoothManager] Scan finished! Found %u BLE devices.\n", (unsigned int)m_discoveredDevices.size());
    }

    void BluetoothManager::stopScan()
    {
        if (m_bleScan && m_isScanning)
        {
            m_bleScan->stop();
            m_bleScan->clearResults();
            m_isScanning = false;
            Serial.println("[BluetoothManager] Scan stopped.");
        }
    }

    bool BluetoothManager::connectToDevice(const std::string& address)
    {
        if (!m_initialized) begin();
        
        // Non-blocking connection simulation/toggle for UI responsiveness
        Serial.printf("[BluetoothManager] Toggling connection for BLE device: %s...\n", address.c_str());

        if (m_connectedAddress == address)
        {
            m_connectedAddress.clear();
        }
        else
        {
            m_connectedAddress = address;
        }

        for (auto& dev : m_discoveredDevices)
        {
            dev.isConnected = (!m_connectedAddress.empty() && dev.address == m_connectedAddress);
        }

        return true;
    }

    void BluetoothManager::disconnectCurrent()
    {
        m_connectedAddress.clear();
        for (auto& dev : m_discoveredDevices)
        {
            dev.isConnected = false;
        }
        Serial.println("[BluetoothManager] Disconnected current BLE device.");
    }

    void BluetoothManager::onResult(BLEAdvertisedDevice advertisedDevice)
    {
        std::string name;
        std::string addr = advertisedDevice.getAddress().toString();
        int rssi = advertisedDevice.getRSSI();

        // 1. ALWAYS PRIORITIZE ACTUAL ADVERTISED LOCAL NAME (Complete or Shortened)
        if (advertisedDevice.haveName())
        {
            name = advertisedDevice.getName();
        }

        // 2. Fallback to Manufacturer Payload classification ONLY if no name string was sent
        if (name.empty() && advertisedDevice.haveManufacturerData())
        {
            std::string mfg = advertisedDevice.getManufacturerData();
            if (mfg.length() >= 2)
            {
                uint16_t companyId = ((uint8_t)mfg[1] << 8) | (uint8_t)mfg[0];
                if (companyId == 0x004C) name = "Apple Device";
                else if (companyId == 0x0075) name = "Samsung Device";
                else if (companyId == 0x00E0) name = "Google Device";
                else if (companyId == 0x012D) name = "Sony Audio";
                else if (companyId == 0x02E5) name = "Espressif Device";
                else if (companyId == 0x0006) name = "Microsoft PC";
                else if (companyId == 0x000A) name = "Qualcomm Audio";
            }
        }

        // 3. Fallback to Appearance / MAC suffix
        if (name.empty())
        {
            if (advertisedDevice.haveAppearance())
            {
                uint16_t appearance = advertisedDevice.getAppearance();
                if (appearance >= 64 && appearance <= 127) name = "Smartphone";
                else if (appearance >= 128 && appearance <= 191) name = "Computer / PC";
                else if (appearance >= 192 && appearance <= 255) name = "Smart Watch";
                else if (appearance >= 960 && appearance <= 1023) name = "Wireless Earbuds";
                else name = "Bluetooth Device";
            }
            else
            {
                std::string shortId = addr.length() >= 5 ? addr.substr(addr.length() - 5) : addr;
                for (char& c : shortId) if (c == ':') c = '-';
                name = "BT Device #" + shortId;
            }
        }

        // Avoid duplicates - update RSSI and overwrite generic/fallback names if real name is discovered in Scan Response
        for (auto& dev : m_discoveredDevices)
        {
            if (dev.address == addr)
            {
                dev.rssi = rssi;
                // If we previously assigned a fallback name but now received the real name, update it!
                if (!name.empty() && (dev.name.rfind("BT Device #", 0) == 0 || dev.name == "Apple Device" || dev.name == "Microsoft PC" || dev.name == "Samsung Device"))
                {
                    if (name.rfind("BT Device #", 0) != 0)
                    {
                        dev.name = name;
                    }
                }
                return;
            }
        }

        DiscoveredDevice device;
        device.name = name;
        device.address = addr;
        device.rssi = rssi;
        device.isConnected = (!m_connectedAddress.empty() && m_connectedAddress == addr);

        m_discoveredDevices.push_back(device);
        Serial.printf("[BLE Scan] Found: %s [%s] RSSI: %d dBm\n", name.c_str(), addr.c_str(), rssi);
    }
}
