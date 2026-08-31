#ifndef VOXA_QUICKPANEL_H
#define VOXA_QUICKPANEL_H

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "../touch/Touch.h"
#include "../screens/ScreenCommon.h"


namespace VOXA
{
    class QuickPanel
    {
    public:
        static QuickPanel& instance();

        ScreenId process(Touch& touch, LovyanGFX& target, uint16_t w, uint16_t h);

        bool isOpen() const { return m_isOpen || m_animY > 0.0f; }
        void open() { m_isOpen = true; }
        void close() { m_isOpen = false; }

    private:
        QuickPanel();

        bool  m_isOpen{false};
        float m_animY{0.0f}; // 0.0 (closed) to 1.0 (fully open)

        // Gesture tracking & Long-Press Timer
        bool     m_trackingPull{false};
        float    m_pullStartY{0.0f};
        uint32_t m_touchStartMs{0};
        int      m_pressedBtn{-1}; // 0 = Wi-Fi, 1 = BT
        bool     m_longPressTriggered{false};


        // Panel state
        bool m_wifiEnabled{true};
        bool m_bluetoothEnabled{false};
        bool m_nightMode{false};
        
        // Touch interaction tracking for sliders and toggles
        int     m_activeSlider{-1}; // -1 = none, 0 = brightness, 1 = volume
        bool    m_btnLatched{false};
        uint8_t m_prevNonZeroVol{80};
    };

}

#endif // VOXA_QUICKPANEL_H
