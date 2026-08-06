#include "QuickPanel.h"
#include "../display/Display.h"
#include "../audio/AudioManager.h"
#include "../services/WiFiManager.h"
#include "../screens/ScreenCommon.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>

namespace VOXA
{
    QuickPanel& QuickPanel::instance()
    {
        static QuickPanel instance;
        return instance;
    }

    QuickPanel::QuickPanel()
    {
    }

    ScreenId QuickPanel::process(Touch& touch, LovyanGFX& target, uint16_t w, uint16_t h)
    {
        uint16_t tx = 0, ty = 0;
        bool touched = touch.getPoint(tx, ty);
        ScreenId navTarget = ScreenId::Home;
        uint32_t nowMs = millis();

        // 1. GESTURE DETECTION (Pull down from status bar to open, pull up from handle to close)
        if (touched)
        {
            if (!m_trackingPull)
            {
                if (!m_isOpen && ty <= 45) // Touch down on top status bar
                {
                    m_trackingPull = true;
                    m_pullStartY = ty;
                }
                else if (m_isOpen && ty >= h * 0.65f) // Touch down near bottom handle area
                {
                    m_trackingPull = true;
                    m_pullStartY = ty;
                }
            }
            else
            {
                float deltaY = ty - m_pullStartY;
                if (!m_isOpen && deltaY > 15.0f)
                {
                    m_isOpen = true; // Open panel
                }
                else if (m_isOpen && deltaY < -30.0f && m_activeSlider == -1)
                {
                    m_isOpen = false; // Close panel
                    m_trackingPull = false;
                }
            }
        }
        else
        {
            // Touch Released -> Process short tap if long press wasn't triggered
            if (m_pressedBtn >= 0 && !m_longPressTriggered && m_isOpen)
            {
                if (m_pressedBtn == 0) // Wi-Fi toggle
                {
                    m_wifiEnabled = !m_wifiEnabled;
                    if (m_wifiEnabled) wifiManager.connect();
                    else wifiManager.disconnect();
                    Serial.printf("[QuickPanel] Wi-Fi Toggled -> %s\n", m_wifiEnabled ? "ON" : "OFF");
                }
                else if (m_pressedBtn == 1) // BT toggle
                {
                    m_bluetoothEnabled = !m_bluetoothEnabled;
                    Serial.printf("[QuickPanel] Bluetooth Toggled -> %s\n", m_bluetoothEnabled ? "ON" : "OFF");
                }
                else if (m_pressedBtn == 2) // Night Mode toggle
                {
                    m_nightMode = !m_nightMode;
                    Serial.printf("[QuickPanel] Night Mode Toggled -> %s\n", m_nightMode ? "ON" : "OFF");
                }
            }

            m_trackingPull = false;
            m_activeSlider = -1;
            m_pressedBtn = -1;
            m_touchStartMs = 0;
            m_longPressTriggered = false;
        }

        // 2. ANIMATION TIMELINE SMOOTHING
        float targetY = m_isOpen ? 1.0f : 0.0f;
        m_animY += (targetY - m_animY) * 0.30f;
        if (std::abs(targetY - m_animY) < 0.005f)
        {
            m_animY = targetY;
        }

        if (m_animY <= 0.001f)
        {
            return ScreenId::Home; // Panel closed
        }

        // 3. INTERACTION TOUCH HANDLING WHEN OPEN
        float panelH = h * 0.78f;
        float currentPanelY = (m_animY - 1.0f) * panelH;

        if (m_isOpen && touched && m_animY > 0.7f && !m_trackingPull)
        {
            float toggleY = currentPanelY + 46.0f;
            float btnW = (w - 40.0f) / 3.0f;

            // Toggle Buttons Touch Region (Y = toggleY to toggleY + 50)
            if (ty >= toggleY - 5.0f && ty <= toggleY + 55.0f)
            {
                if (m_pressedBtn == -1)
                {
                    m_touchStartMs = nowMs;
                    m_longPressTriggered = false;

                    if (tx >= 10.0f && tx <= 12.0f + btnW) m_pressedBtn = 0; // Wi-Fi
                    else if (tx >= 15.0f + btnW && tx <= 17.0f + 2 * btnW) m_pressedBtn = 1; // BT
                    else if (tx >= 20.0f + 2 * btnW && tx <= 25.0f + 3 * btnW) m_pressedBtn = 2; // Night
                }
                else if (m_touchStartMs > 0 && !m_longPressTriggered)
                {
                    // Check for Long Press (> 350ms)
                    if (nowMs - m_touchStartMs >= 350)
                    {
                        m_longPressTriggered = true;
                        if (m_pressedBtn == 0) // Long Press Wi-Fi
                        {
                            Serial.println("[QuickPanel] Long-press Wi-Fi -> Opening Wi-Fi Settings Screen!");
                            m_isOpen = false;
                            m_pressedBtn = -1;
                            m_touchStartMs = 0;
                            navTarget = ScreenId::WiFiSettings;
                        }
                        else if (m_pressedBtn == 1) // Long Press BT
                        {
                            Serial.println("[QuickPanel] Long-press Bluetooth -> Opening Bluetooth Settings Screen!");
                            m_isOpen = false;
                            m_pressedBtn = -1;
                            m_touchStartMs = 0;
                            navTarget = ScreenId::BluetoothSettings;
                        }
                    }
                }
            }

            // Sliders Touch Region
            float brightY = currentPanelY + 110.0f;
            float volY    = currentPanelY + 140.0f;
            float sliderX = 46.0f;
            float sliderW = w - 66.0f;

            if (ty >= brightY - 10.0f && ty <= brightY + 24.0f)
            {
                m_activeSlider = 0;
                float pct = std::max(0.0f, std::min(1.0f, (tx - sliderX) / sliderW));
                uint8_t newBright = static_cast<uint8_t>(pct * 255.0f);
                Display::setBrightness(std::max((uint8_t)15, newBright));
            }
            else if (ty >= volY - 10.0f && ty <= volY + 24.0f)
            {
                m_activeSlider = 1;
                float pct = std::max(0.0f, std::min(1.0f, (tx - sliderX) / sliderW));
                uint8_t newVol = static_cast<uint8_t>(pct * 100.0f);
                AudioManager::instance().setVolume(newVol);
            }
            
            // Handle Bar tap to close
            if (ty >= currentPanelY + panelH - 25.0f && ty <= currentPanelY + panelH + 15.0f)
            {
                m_isOpen = false;
            }
        }



        // 4. PANEL RENDER OVERLAY (Glassmorphism Dark Sheet)
        // Draw card background sheet
        target.fillRoundRect(6, (int)currentPanelY, w - 12, (int)panelH, 16, VoxaTheme::getSurface());

        target.drawRoundRect(6, (int)currentPanelY, w - 12, (int)panelH, 16, VoxaTheme::getPrimary());

        // Header Title & Status
        target.setFont(&fonts::FreeSansBold9pt7b);
        target.setTextColor(VoxaTheme::getTextPrimary());
        target.setTextDatum(textdatum_t::top_left);
        target.drawString("Control Center", 20, (int)(currentPanelY + 16.0f));

        // Close Pull Handle Bar at bottom of card
        float handleX = w * 0.5f - 20.0f;
        float handleY = currentPanelY + panelH - 12.0f;
        target.fillRoundRect((int)handleX, (int)handleY, 40, 4, 2, VoxaTheme::getDivider());

        // ── 3 QUICK TOGGLE BUTTONS (Wi-Fi, Bluetooth, Night Mode) ────────────
        float btnW = (w - 40.0f) / 3.0f;
        float toggleY = currentPanelY + 46.0f;

        // 1. Wi-Fi Toggle
        bool wifiOn = m_wifiEnabled;
        uint16_t wifiBg = wifiOn ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
        uint16_t wifiFg = wifiOn ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
        target.fillRoundRect(12, (int)toggleY, (int)btnW, 50, 10, wifiBg);
        target.drawRoundRect(12, (int)toggleY, (int)btnW, 50, 10, VoxaTheme::getDivider());
        ScreenCommon::drawIcon(target, wifiOn ? Icon::Wifi : Icon::WiFiOff, 12 + btnW * 0.5f - 10.0f, toggleY + 6.0f, 20.0f, wifiFg);
        target.setFont(&fonts::Font0);
        target.setTextDatum(textdatum_t::top_center);
        target.setTextColor(wifiFg);
        target.drawString(wifiOn ? "Wi-Fi On" : "Wi-Fi Off", 12 + btnW * 0.5f, toggleY + 32.0f);


        // 2. Bluetooth Toggle
        uint16_t btBg = m_bluetoothEnabled ? VoxaTheme::getPrimary() : VoxaTheme::getBackground();
        uint16_t btFg = m_bluetoothEnabled ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
        target.fillRoundRect(17 + (int)btnW, (int)toggleY, (int)btnW, 50, 10, btBg);
        target.drawRoundRect(17 + (int)btnW, (int)toggleY, (int)btnW, 50, 10, VoxaTheme::getDivider());
        ScreenCommon::drawIcon(target, Icon::Bluetooth, 17 + btnW * 1.5f - 10.0f, toggleY + 6.0f, 20.0f, btFg);
        target.setTextColor(btFg);
        target.drawString(m_bluetoothEnabled ? "BT On" : "BT Off", 17 + btnW * 1.5f, toggleY + 32.0f);

        // 3. Night Mode Toggle
        uint16_t nightBg = m_nightMode ? VoxaTheme::getPrimary() : VoxaTheme::getBackground();
        uint16_t nightFg = m_nightMode ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
        target.fillRoundRect(22 + (int)btnW * 2, (int)toggleY, (int)btnW, 50, 10, nightBg);
        target.drawRoundRect(22 + (int)btnW * 2, (int)toggleY, (int)btnW, 50, 10, VoxaTheme::getDivider());
        ScreenCommon::drawIcon(target, m_nightMode ? Icon::Moon : Icon::Sun, 22 + btnW * 2.5f - 10.0f, toggleY + 6.0f, 20.0f, nightFg);
        target.setTextColor(nightFg);
        target.drawString(m_nightMode ? "Night" : "Day", 22 + btnW * 2.5f, toggleY + 32.0f);

        // ── BRIGHTNESS SLIDER ────────────────────────────────────────────────
        float brightY = currentPanelY + 110.0f;
        ScreenCommon::drawIcon(target, Icon::Sun, 16, brightY, 20.0f, VoxaTheme::getPrimary());
        
        float sliderX = 46.0f;
        float sliderW = w - 66.0f;
        float sliderH = 14.0f;
        target.fillRoundRect((int)sliderX, (int)brightY + 3, (int)sliderW, (int)sliderH, 7, VoxaTheme::getBackground());
        
        uint8_t curBright = Display::getBrightness();
        float brightPct = curBright / 255.0f;
        target.fillRoundRect((int)sliderX, (int)brightY + 3, (int)(sliderW * brightPct), (int)sliderH, 7, VoxaTheme::getPrimary());
        target.drawRoundRect((int)sliderX, (int)brightY + 3, (int)sliderW, (int)sliderH, 7, VoxaTheme::getDivider());

        // ── VOLUME SLIDER ────────────────────────────────────────────────────
        float volY = currentPanelY + 140.0f;
        ScreenCommon::drawIcon(target, Icon::Volume, 16, volY, 20.0f, VoxaTheme::getPrimary());
        
        target.fillRoundRect((int)sliderX, (int)volY + 3, (int)sliderW, (int)sliderH, 7, VoxaTheme::getBackground());
        uint8_t curVol = AudioManager::instance().getVolume();
        float volPct = curVol / 100.0f;
        target.fillRoundRect((int)sliderX, (int)volY + 3, (int)(sliderW * volPct), (int)sliderH, 7, VoxaTheme::getPrimary());
        target.drawRoundRect((int)sliderX, (int)volY + 3, (int)sliderW, (int)sliderH, 7, VoxaTheme::getDivider());

        return navTarget;
    }
}
