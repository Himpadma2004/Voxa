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

    void BluetoothManager::begin()
    {
        if (m_initialized) return;

        Serial.println("[BluetoothManager] Initializing ESP32-S3 BLE Controller (VOXA Bluetooth)...");
        BLEDevice::init("VOXA Bluetooth");

        // Set up BLE Advertising so VOXA is visible to Smartphones, PCs, and Laptops
        BLEServer* pServer = BLEDevice::createServer();
        if (pServer)
        {
            BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
            pAdvertising->addServiceUUID("12345678-1234-1234-1234-123456789abc");
            pAdvertising->setScanResponse(true);
            pAdvertising->setMinPreferred(0x06);  // Functions for iPhone connection speed
            pAdvertising->setMinPreferred(0x12);
            BLEDevice::startAdvertising();
            Serial.println("[BluetoothManager] BLE Advertising Started as 'VOXA Bluetooth'");
        }
        
        m_bleScan = BLEDevice::getScan();
        if (m_bleScan)
        {
            m_bleScan->setAdvertisedDeviceCallbacks(this);
            m_bleScan->setActiveScan(true); // Active scan requests Scan Response packet (contains full device name)
            m_bleScan->setInterval(100);
            m_bleScan->setWindow(99);
        }

        m_initialized = true;
        m_enabled = true;
        Serial.println("[BluetoothManager] ESP32-S3 BLE Controller Ready!");
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
