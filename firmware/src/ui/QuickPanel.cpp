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
                else if (m_pressedBtn == 2) // Night Mode toggle (Warm/Dimmed vs Normal Brightness)
                {
                    m_nightMode = !m_nightMode;
                    if (m_nightMode)
                    {
                        Display::setBrightness(30); // Soft dimmed warm night brightness
                    }
                    else
                    {
                        Display::setBrightness(200); // Standard vibrant daylight brightness
                    }
                    Serial.printf("[QuickPanel] Night Mode Toggled -> %s\n", m_nightMode ? "NIGHT (DIM)" : "DAY (BRIGHT)");
                }
                else if (m_pressedBtn == 3) // Rotate Screen toggle (Horizontal default vs Vertical)
                {
                    uint8_t curRot = Display::getRotation();
                    // Toggle between 1 (Horizontal Landscape default) and 0 (Vertical Portrait)
                    uint8_t nextRot = (curRot == 1) ? 0 : 1;
                    Display::setRotation(nextRot);
                    touch.setRotation(nextRot); // Synchronize CST328 touch controller rotation on hardware!
                    Serial.printf("[QuickPanel] Screen & Touch Rotation Toggled -> %u (%s)\n", nextRot, nextRot == 1 ? "Landscape" : "Portrait");
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
        float panelH = std::min((float)h * 0.88f, 210.0f);
        float currentPanelY = (m_animY - 1.0f) * panelH;

        if (m_isOpen && touched && m_animY > 0.7f && !m_trackingPull)
        {
            float toggleY = currentPanelY + 42.0f;
            float btnW = (w - 30.0f) / 4.0f; // 4 Grid Toggle Buttons

            // Toggle Buttons Touch Region (Y = toggleY to toggleY + 60)
            if (ty >= toggleY - 5.0f && ty <= toggleY + 65.0f)
            {
                if (m_pressedBtn == -1)
                {
                    m_touchStartMs = nowMs;
                    m_longPressTriggered = false;

                    if (tx >= 6.0f && tx <= 6.0f + btnW) m_pressedBtn = 0; // Wi-Fi
                    else if (tx >= 11.0f + btnW && tx <= 11.0f + 2 * btnW) m_pressedBtn = 1; // BT
                    else if (tx >= 16.0f + 2 * btnW && tx <= 16.0f + 3 * btnW) m_pressedBtn = 2; // Night Mode
                    else if (tx >= 21.0f + 3 * btnW && tx <= 21.0f + 4 * btnW) m_pressedBtn = 3; // Rotate
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
            float brightY = currentPanelY + 116.0f;
            float volY    = currentPanelY + 152.0f;
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
        target.drawString("Control Center", 16, (int)(currentPanelY + 14.0f));

        // Close Pull Handle Bar at bottom of card
        float handleX = w * 0.5f - 20.0f;
        float handleY = currentPanelY + panelH - 10.0f;
        target.fillRoundRect((int)handleX, (int)handleY, 40, 4, 2, VoxaTheme::getDivider());

        // ── 4 QUICK TOGGLE BUTTONS (Wi-Fi, Bluetooth, Night Mode, Rotate) ────
        float marginX = 14.0f;
        float gapX = 6.0f;
        float btnW = (w - (2.0f * marginX) - (3.0f * gapX)) / 4.0f;
        float btnH = 58.0f;
        float toggleY = currentPanelY + 44.0f;

        // 1. Wi-Fi Toggle
        bool wifiOn = m_wifiEnabled;
        uint16_t wifiBg = wifiOn ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
        uint16_t wifiFg = wifiOn ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
        float b0X = marginX;
        target.fillRoundRect((int)b0X, (int)toggleY, (int)btnW, (int)btnH, 10, wifiBg);
        target.drawRoundRect((int)b0X, (int)toggleY, (int)btnW, (int)btnH, 10, VoxaTheme::getDivider());
        ScreenCommon::drawIcon(target, wifiOn ? Icon::Wifi : Icon::WiFiOff, b0X + btnW * 0.5f - 10.0f, toggleY + 8.0f, 20.0f, wifiFg);
        target.setFont(&fonts::Font0);
        target.setTextDatum(textdatum_t::top_center);
        target.setTextColor(wifiFg);
        target.drawString(wifiOn ? "Wi-Fi" : "Off", b0X + btnW * 0.5f, toggleY + 40.0f);

        // 2. Bluetooth Toggle
        uint16_t btBg = m_bluetoothEnabled ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
        uint16_t btFg = m_bluetoothEnabled ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
        float b1X = marginX + btnW + gapX;
        target.fillRoundRect((int)b1X, (int)toggleY, (int)btnW, (int)btnH, 10, btBg);
        target.drawRoundRect((int)b1X, (int)toggleY, (int)btnW, (int)btnH, 10, VoxaTheme::getDivider());
        ScreenCommon::drawIcon(target, Icon::Bluetooth, b1X + btnW * 0.5f - 10.0f, toggleY + 8.0f, 20.0f, btFg);
        target.setTextColor(btFg);
        target.drawString(m_bluetoothEnabled ? "BT On" : "BT Off", b1X + btnW * 0.5f, toggleY + 40.0f);

        // 3. Night Mode Toggle
        uint16_t nightBg = m_nightMode ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
        uint16_t nightFg = m_nightMode ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
        float b2X = marginX + (btnW + gapX) * 2.0f;
        target.fillRoundRect((int)b2X, (int)toggleY, (int)btnW, (int)btnH, 10, nightBg);
        target.drawRoundRect((int)b2X, (int)toggleY, (int)btnW, (int)btnH, 10, VoxaTheme::getDivider());
        ScreenCommon::drawIcon(target, m_nightMode ? Icon::Moon : Icon::Sun, b2X + btnW * 0.5f - 10.0f, toggleY + 8.0f, 20.0f, nightFg);
        target.setTextColor(nightFg);
        target.drawString(m_nightMode ? "Night" : "Day", b2X + btnW * 0.5f, toggleY + 40.0f);

        // 4. Rotate Screen Toggle
        bool isPortrait = (Display::getRotation() == 0 || Display::getRotation() == 2);
        uint16_t rotBg = isPortrait ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
        uint16_t rotFg = isPortrait ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
        float b3X = marginX + (btnW + gapX) * 3.0f;
        target.fillRoundRect((int)b3X, (int)toggleY, (int)btnW, (int)btnH, 10, rotBg);
        target.drawRoundRect((int)b3X, (int)toggleY, (int)btnW, (int)btnH, 10, VoxaTheme::getDivider());
        ScreenCommon::drawIcon(target, Icon::Rotate, b3X + btnW * 0.5f - 10.0f, toggleY + 8.0f, 20.0f, rotFg);
        target.setTextColor(rotFg);
        target.drawString(isPortrait ? "Port." : "Land.", b3X + btnW * 0.5f, toggleY + 40.0f);


        // ── BRIGHTNESS SLIDER ────────────────────────────────────────────────
        float brightY = currentPanelY + 116.0f;
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
        float volY = currentPanelY + 152.0f;
        ScreenCommon::drawIcon(target, Icon::Volume, 16, volY, 20.0f, VoxaTheme::getPrimary());
        
        target.fillRoundRect((int)sliderX, (int)volY + 3, (int)sliderW, (int)sliderH, 7, VoxaTheme::getBackground());
        uint8_t curVol = AudioManager::instance().getVolume();
        float volPct = curVol / 100.0f;
        target.fillRoundRect((int)sliderX, (int)volY + 3, (int)(sliderW * volPct), (int)sliderH, 7, VoxaTheme::getPrimary());
        target.drawRoundRect((int)sliderX, (int)volY + 3, (int)sliderW, (int)sliderH, 7, VoxaTheme::getDivider());

        return navTarget;
    }
}

