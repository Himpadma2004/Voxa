#pragma once
#include "ScreenCommon.h"
#include "../touch/Touch.h"
#include <string>
#include <vector>

namespace VOXA
{
    struct WiFiNetwork
    {
        std::string ssid;
        int32_t rssi;
        bool isSecure;
    };

    class WiFiSettingsScreen
    {
    public:
        ScreenId show(Touch& touch);

    private:
        void performScan();

        std::vector<WiFiNetwork> m_networks;
        bool m_isScanning { false };
        bool m_hasScanned { false };
        
        bool m_wasTouched { false };
        bool m_isDragging { false };
        bool m_isBackPressed { false };
        bool m_isRefreshPressed { false };
        int  m_pressedItemIndex { -1 };
        float m_scrollY { 0.0f };
        float m_targetScrollY { 0.0f };
        float m_dragStartY { 0.0f };
        float m_dragStartScrollY { 0.0f };
        float m_scrollVelocity { 0.0f };
        uint32_t m_lastTouchSampleMs { 0 };
        float m_lastDragX { 0.0f };
        float m_lastDragY { 0.0f };

        // Wizard state to capture manual inputs
        enum class WizardState
        {
            None,
            InputManualSSID,
            InputManualPassword,
            InputSelectedPassword
        };
        WizardState m_wizardState { WizardState::None };
        std::string m_selectedSSID;
        std::string m_manualSSID;
    };
}
