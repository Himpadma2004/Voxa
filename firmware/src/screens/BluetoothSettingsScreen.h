#pragma once
#include "ScreenCommon.h"
#include "../touch/Touch.h"
#include <string>
#include <vector>

namespace VOXA
{
    struct BluetoothDevice
    {
        std::string name;
        std::string address;
        int32_t rssi;
        bool isConnected;
    };

    class BluetoothSettingsScreen
    {
    public:
        ScreenId show(Touch& touch);

    private:
        void performScan();

        std::vector<BluetoothDevice> m_devices;
        bool m_isScanning { false };
        bool m_hasScanned { false };
        bool m_btEnabled { true };
        
        bool m_wasTouched { false };
        bool m_isDragging { false };
        bool m_isBackPressed { false };
        bool m_isRefreshPressed { false };
        int  m_pressedIndex { -1 };

        float m_scrollY { 0.0f };
        float m_targetScrollY { 0.0f };
        float m_scrollVelocity { 0.0f };
        float m_dragStartY { 0.0f };
        float m_lastDragY { 0.0f };
        uint32_t m_lastTouchMs { 0 };
    };
}
