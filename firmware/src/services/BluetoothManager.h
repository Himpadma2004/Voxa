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
#include <BLE2902.h>
#include <BLESecurity.h>
#include <BLEHIDDevice.h>

namespace VOXA
{

    enum class MediaCommand
    {
        PlayPause,
        Next,
        Prev,
        VolumeUp,
        VolumeDown
    };

    struct DiscoveredDevice
    {
        std::string name;
        std::string address;
        int rssi;
        bool isConnected;
    };

    class BluetoothManager : public BLEAdvertisedDeviceCallbacks, public BLEServerCallbacks
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
        bool isConnectedToPhone() const { return m_phoneConnected || !m_connectedAddress.empty(); }
        const std::string& getConnectedPhoneName() const { return m_connectedPhoneName; }

        void setEnabled(bool enable);

        const std::vector<DiscoveredDevice>& getDiscoveredDevices() const { return m_discoveredDevices; }
        const std::string& getConnectedAddress() const { return m_connectedAddress; }

        bool connectToDevice(const std::string& address);
        void disconnectCurrent();

        void sendMediaCommand(MediaCommand cmd);

        // BLEAdvertisedDeviceCallbacks override
        void onResult(BLEAdvertisedDevice advertisedDevice) override;

        // BLEServerCallbacks override
        void onConnect(BLEServer* pServer) override;
        void onDisconnect(BLEServer* pServer) override;

    private:
        bool m_initialized{false};
        bool m_enabled{true};
        bool m_isScanning{false};
        bool m_phoneConnected{false};

        std::string m_connectedAddress;
        std::string m_connectedPhoneName{"Smartphone"};
        BLEScan* m_bleScan{nullptr};
        BLECharacteristic* m_inputReportChar{nullptr};
        std::vector<DiscoveredDevice> m_discoveredDevices;
    };


    extern BluetoothManager bluetoothManager;
}

#endif // VOXA_BLUETOOTHMANAGER_H

