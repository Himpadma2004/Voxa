#ifndef VOXA_BLUETOOTHMANAGER_H
#define VOXA_BLUETOOTHMANAGER_H

#include <Arduino.h>
#include <vector>
#include <string>
#include <map>

// BLE headers MUST be included in global scope, NOT inside namespace VOXA
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

namespace VOXA
{

    struct DiscoveredDevice
    {
        std::string name;
        std::string address;
        int rssi;
        bool isConnected;
    };

    class BluetoothManager : public BLEAdvertisedDeviceCallbacks
    {
    public:
        static BluetoothManager& instance();

        BluetoothManager();
        ~BluetoothManager() = default;

        void begin();
        void startScan(uint32_t durationSecs = 4);
        void stopScan();

        bool isScanning() const { return m_isScanning; }
        bool isEnabled() const { return m_enabled; }

        void setEnabled(bool enable);

        const std::vector<DiscoveredDevice>& getDiscoveredDevices() const { return m_discoveredDevices; }
        const std::string& getConnectedAddress() const { return m_connectedAddress; }

        bool connectToDevice(const std::string& address);
        void disconnectCurrent();

        // BLEAdvertisedDeviceCallbacks override
        void onResult(BLEAdvertisedDevice advertisedDevice) override;

    private:
        bool m_initialized{false};
        bool m_enabled{true};
        bool m_isScanning{false};

        std::string m_connectedAddress;
        BLEScan* m_bleScan{nullptr};
        std::vector<DiscoveredDevice> m_discoveredDevices;
    };


    extern BluetoothManager bluetoothManager;
}

#endif // VOXA_BLUETOOTHMANAGER_H

