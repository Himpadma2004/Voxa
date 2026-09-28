#include "QuickPanel.h"
#include "../display/Display.h"
#include "../audio/AudioManager.h"
#include "../services/WiFiManager.h"
#include "../services/TimeService.h"
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

        float panelH = 224.0f;

        // 1. GESTURE DETECTION (Pull down from top status bar to open, pull up from handle to close)
        if (touched)
        {
            if (!m_trackingPull && m_activeSlider == -1)
            {
                if (!m_isOpen && ty <= 45) // Touch down on top status bar
                {
                    m_trackingPull = true;
                    m_pullStartY = ty;
                }
                else if (m_isOpen && ty >= panelH - 25.0f) // Touch down near bottom handle area
                {
                    m_trackingPull = true;
                    m_pullStartY = ty;
                }
            }
            else if (m_trackingPull)
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
                else if (m_pressedBtn == 2) // Mode toggle (Day vs Night)
                {
                    m_nightMode = !m_nightMode;
                    if (m_nightMode)
                    {
                        Display::setBrightness(40);
                    }
                    else
                    {
                        Display::setBrightness(200);
                    }
                    Serial.printf("[QuickPanel] Mode Toggled -> %s\n", m_nightMode ? "NIGHT" : "DAY");
                }
                else if (m_pressedBtn == 3) // Port. Lock toggle
                {
                    uint8_t curRot = Display::getRotation();
                    uint8_t nextRot = (curRot == 1) ? 0 : 1;
                    Display::setRotation(nextRot);
                    touch.setRotation(nextRot);
                    Serial.printf("[QuickPanel] Rotation Lock Toggled -> %u\n", nextRot);
                }
            }

            m_trackingPull = false;
            if (m_activeSlider == 1)
            {
                AudioManager::instance().setVolume(AudioManager::instance().getVolume(), true);
            }
            m_activeSlider = -1;
            m_pressedBtn = -1;
            m_touchStartMs = 0;
            m_longPressTriggered = false;
        }

        // 2. ANIMATION TIMELINE SMOOTHING
        float targetY = m_isOpen ? 1.0f : 0.0f;
        m_animY += (targetY - m_animY) * 0.35f;
        if (std::abs(targetY - m_animY) < 0.005f)
        {
            m_animY = targetY;
        }

        if (m_animY <= 0.001f)
        {
            return ScreenId::Home; // Panel closed
        }

        // 3. INTERACTION TOUCH HANDLING WHEN OPEN
        float currentPanelY = (m_animY - 1.0f) * panelH;

        if (m_isOpen && touched && m_animY > 0.7f && !m_trackingPull)
        {
            float toggleY = currentPanelY + 36.0f;
            float btnW = 51.0f;
            float gapX = 6.0f;

            // Toggle Buttons Touch Region (Y = toggleY to toggleY + 56)
            if (ty >= toggleY && ty <= toggleY + 56.0f && m_activeSlider == -1)
            {
                if (m_pressedBtn == -1)
                {
                    m_touchStartMs = nowMs;
                    m_longPressTriggered = false;

                    if (tx >= 8.0f && tx <= 8.0f + btnW) m_pressedBtn = 0; // Wi-Fi
                    else if (tx >= 8.0f + (btnW + gapX) && tx <= 8.0f + 2 * btnW + gapX) m_pressedBtn = 1; // BT
                    else if (tx >= 8.0f + 2 * (btnW + gapX) && tx <= 8.0f + 3 * btnW + 2 * gapX) m_pressedBtn = 2; // Mode
                    else if (tx >= 8.0f + 3 * (btnW + gapX) && tx <= 8.0f + 4 * btnW + 3 * gapX) m_pressedBtn = 3; // Port Lock
                }
                else if (m_touchStartMs > 0 && !m_longPressTriggered)
                {
                    // Long press (> 350ms)
                    if (nowMs - m_touchStartMs >= 350)
                    {
                        m_longPressTriggered = true;
                        if (m_pressedBtn == 0) // Wi-Fi settings
                        {
                            m_isOpen = false;
                            m_pressedBtn = -1;
                            m_touchStartMs = 0;
                            navTarget = ScreenId::WiFiSettings;
                        }
                        else if (m_pressedBtn == 1) // BT settings
                        {
                            m_isOpen = false;
                            m_pressedBtn = -1;
                            m_touchStartMs = 0;
                            navTarget = ScreenId::BluetoothSettings;
                        }
                    }
                }
            }

            // Vertical Sliders (OLED LUX & REC GAIN)
            float sliderCardY = currentPanelY + 98.0f;
            float sliderCardH = 104.0f;
            float cardW = 108.0f;

            if (m_activeSlider == 0) // Dragging Brightness (Left Card)
            {
                float pct = 1.0f - std::max(0.0f, std::min(1.0f, (ty - sliderCardY) / sliderCardH));
                uint8_t newBright = static_cast<uint8_t>(pct * 255.0f);
                Display::setBrightness(std::max((uint8_t)15, newBright));
            }
            else if (m_activeSlider == 1) // Dragging Volume (Right Card)
            {
                float pct = 1.0f - std::max(0.0f, std::min(1.0f, (ty - sliderCardY) / sliderCardH));
                uint8_t newVol = static_cast<uint8_t>(pct * 100.0f + 0.5f);
                AudioManager::instance().setVolume(newVol, false);
            }
            else if (m_pressedBtn == -1) // Hitbox detection for sliders
            {
                if (ty >= sliderCardY && ty <= sliderCardY + sliderCardH)
                {
                    if (tx >= 8.0f && tx <= 8.0f + cardW) // Left Card: OLED LUX
                    {
                        m_activeSlider = 0;
                        float pct = 1.0f - std::max(0.0f, std::min(1.0f, (ty - sliderCardY) / sliderCardH));
                        uint8_t newBright = static_cast<uint8_t>(pct * 255.0f);
                        Display::setBrightness(std::max((uint8_t)15, newBright));
                    }
                    else if (tx >= 124.0f && tx <= 124.0f + cardW) // Right Card: REC GAIN
                    {
                        m_activeSlider = 1;
                        float pct = 1.0f - std::max(0.0f, std::min(1.0f, (ty - sliderCardY) / sliderCardH));
                        uint8_t newVol = static_cast<uint8_t>(pct * 100.0f + 0.5f);
                        AudioManager::instance().setVolume(newVol, false);
                    }
                }
            }

            // Tap handle bar to close
            if (ty >= currentPanelY + panelH - 18.0f && m_activeSlider == -1)
            {
                m_isOpen = false;
            }
        }

        // 4. PANEL RENDER OVERLAY - Pure Pitch Black OLED Hub Style
        target.fillRoundRect(0, (int)currentPanelY, w, (int)panelH, 14, 0x0000);
        target.drawRoundRect(0, (int)currentPanelY, w, (int)panelH, 14, target.color565(34, 38, 48));

        // Grabber Bar at Top Center
        target.fillRoundRect((int)(w * 0.5f - 14.0f), (int)(currentPanelY + 5.0f), 28, 3, 1, target.color565(80, 90, 105));

        // ── TOP STATUS / HEADER (Y = currentPanelY + 12) ──
        // Title "Control"
        target.setFont(&fonts::FreeSansBold9pt7b);
        target.setTextDatum(textdatum_t::top_left);
        target.setTextColor(0xFFFF);
        target.drawString("Control", 10, (int)(currentPanelY + 12.0f));

        // Cyan Time "10:42"
        target.setFont(&fonts::DejaVu9);
        std::string timeStr = timeService.getCurrentTime();
        if (timeStr.empty()) timeStr = "14:41";
        if (timeStr.length() > 5) timeStr = timeStr.substr(0, 5);
        target.setTextColor(0x3DFE); // Bright Cyan
        target.drawString(timeStr.c_str(), 72, (int)(currentPanelY + 14.0f));

        // Memory "128M"
        target.setTextDatum(textdatum_t::top_right);
        target.setTextColor(0x94A3B8);
        target.drawString("128M", w - 50, (int)(currentPanelY + 14.0f));

        // Battery "94% ⚡" in Amber
        target.setTextColor(0xFDC0);
        target.drawString("94%", w - 20, (int)(currentPanelY + 14.0f));
        // Lightning bolt
        target.fillTriangle(w - 14, (int)(currentPanelY + 14.0f), w - 18, (int)(currentPanelY + 20.0f), w - 13, (int)(currentPanelY + 20.0f), 0xFDC0);
        target.fillTriangle(w - 15, (int)(currentPanelY + 19.0f), w - 10, (int)(currentPanelY + 19.0f), w - 14, (int)(currentPanelY + 25.0f), 0xFDC0);

        // ── 4 QUICK TOGGLE CARDS (Y = currentPanelY + 36, H = 54) ──
        float btnW = 51.0f;
        float btnH = 54.0f;
        float gapX = 6.0f;
        float toggleY = currentPanelY + 36.0f;

        // 1. Wi-Fi Card
        float b0X = 8.0f;
        bool wifiOn = m_wifiEnabled;
        target.fillRoundRect((int)b0X, (int)toggleY, (int)btnW, (int)btnH, 8, target.color565(18, 20, 26));
        target.drawRoundRect((int)b0X, (int)toggleY, (int)btnW, (int)btnH, 8, target.color565(34, 38, 48));

        // Wi-Fi Icon (Cyan arcs)
        uint16_t wifiCol = wifiOn ? 0x3DFE : target.color565(90, 100, 115);
        int wCx = (int)(b0X + btnW * 0.5f);
        int wCy = (int)(toggleY + 18.0f);
        target.drawCircle(wCx, wCy, 7, wifiCol);
        target.drawCircle(wCx, wCy, 4, wifiCol);
        target.fillCircle(wCx, wCy, 2, wifiCol);
        target.fillRect((int)b0X, wCy + 1, (int)btnW, 12, target.color565(18, 20, 26)); // clip bottom half of arcs

        target.setFont(&fonts::DejaVu9);
        target.setTextDatum(textdatum_t::top_center);
        target.setTextColor(0xFFFF);
        target.drawString("Wi-Fi", wCx, (int)(toggleY + 28.0f));
        target.setTextColor(wifiOn ? 0x3DFE : 0x64748B);
        target.drawString(wifiOn ? "VOX-5G" : "Off", wCx, (int)(toggleY + 40.0f));

        // 2. BT Card
        float b1X = 8.0f + btnW + gapX;
        target.fillRoundRect((int)b1X, (int)toggleY, (int)btnW, (int)btnH, 8, target.color565(18, 20, 26));
        target.drawRoundRect((int)b1X, (int)toggleY, (int)btnW, (int)btnH, 8, target.color565(34, 38, 48));

        // BT Icon (Orange with slash if off)
        int btCx = (int)(b1X + btnW * 0.5f);
        int btCy = (int)(toggleY + 14.0f);
        uint16_t btCol = m_bluetoothEnabled ? 0x3DFE : 0xFDC0;
        target.drawLine(btCx, btCy - 6, btCx, btCy + 6, btCol);
        target.drawLine(btCx, btCy - 6, btCx + 4, btCy - 2, btCol);
        target.drawLine(btCx + 4, btCy - 2, btCx - 4, btCy + 2, btCol);
        target.drawLine(btCx - 4, btCy - 2, btCx + 4, btCy + 2, btCol);
        target.drawLine(btCx + 4, btCy + 2, btCx, btCy + 6, btCol);
        if (!m_bluetoothEnabled)
        {
            target.drawLine(btCx - 6, btCy - 6, btCx + 6, btCy + 6, target.color565(160, 170, 185));
        }

        target.setTextColor(0xFFFF);
        target.drawString("BT", btCx, (int)(toggleY + 28.0f));
        target.setTextColor(m_bluetoothEnabled ? 0x3DFE : 0x64748B);
        target.drawString(m_bluetoothEnabled ? "On" : "Off", btCx, (int)(toggleY + 40.0f));

        // 3. Mode Card (Sun / Mode)
        float b2X = 8.0f + 2.0f * (btnW + gapX);
        target.fillRoundRect((int)b2X, (int)toggleY, (int)btnW, (int)btnH, 8, target.color565(18, 20, 26));
        target.drawRoundRect((int)b2X, (int)toggleY, (int)btnW, (int)btnH, 8, target.color565(34, 38, 48));

        int mCx = (int)(b2X + btnW * 0.5f);
        int mCy = (int)(toggleY + 14.0f);
        target.drawCircle(mCx, mCy, 4, 0xFDC0);
        for (int r = 0; r < 8; ++r)
        {
            float ang = r * 0.785f;
            target.drawLine(mCx + (int)(std::cos(ang) * 5.0f), mCy + (int)(std::sin(ang) * 5.0f),
                            mCx + (int)(std::cos(ang) * 7.0f), mCy + (int)(std::sin(ang) * 7.0f), 0xFDC0);
        }

        target.setTextColor(0xFFFF);
        target.drawString("Mode", mCx, (int)(toggleY + 28.0f));
        target.setTextColor(m_nightMode ? target.color565(160, 180, 255) : 0xFDC0);
        target.drawString(m_nightMode ? "Night" : "Day", mCx, (int)(toggleY + 40.0f));

        // 4. Port. Lock Card (Solid Amber Highlighted Button)
        float b3X = 8.0f + 3.0f * (btnW + gapX);
        bool isPortLocked = true;
        uint16_t portBg = isPortLocked ? 0xFDC0 : target.color565(18, 20, 26);
        uint16_t portFg = isPortLocked ? 0x0000 : 0xFFFF;
        target.fillRoundRect((int)b3X, (int)toggleY, (int)btnW, (int)btnH, 8, portBg);

        int pCx = (int)(b3X + btnW * 0.5f);
        int pCy = (int)(toggleY + 14.0f);
        // Phone icon with lock inside
        target.drawRoundRect(pCx - 5, pCy - 6, 10, 13, 2, portFg);
        target.drawRoundRect(pCx - 2, pCy - 2, 5, 4, 1, portFg);
        target.drawCircle(pCx, pCy - 3, 1, portFg);

        target.setTextColor(portFg);
        target.drawString("Port.", pCx, (int)(toggleY + 28.0f));
        target.setTextColor(isPortLocked ? target.color565(60, 50, 0) : 0x64748B);
        target.drawString("Lock", pCx, (int)(toggleY + 40.0f));

        // ── 2 LARGE VERTICAL SLIDER CARDS (Y = currentPanelY + 98, H = 104) ──
        float sliderCardY = currentPanelY + 98.0f;
        float sliderCardH = 104.0f;
        float cardW = 108.0f;

        // ─────────────────── LEFT CARD: OLED LUX ───────────────────
        float c0X = 8.0f;
        uint8_t curBright = Display::getBrightness();
        float brightPct = curBright / 255.0f;
        int brightFillY = (int)(sliderCardY + sliderCardH * (1.0f - brightPct));
        int brightFillH = (int)(sliderCardY + sliderCardH - brightFillY);

        // Card base
        target.fillRoundRect((int)c0X, (int)sliderCardY, (int)cardW, (int)sliderCardH, 10, target.color565(18, 20, 26));

        // Filled lower region (amber/gold fill)
        if (brightFillH > 0)
        {
            target.setClipRect((int)c0X, brightFillY, (int)cardW, brightFillH);
            target.fillRoundRect((int)c0X, (int)sliderCardY, (int)cardW, (int)sliderCardH, 10, target.color565(55, 45, 12));
            target.clearClipRect();

            // Glowing Amber divider bar at brightFillY
            target.drawLine((int)c0X + 3, brightFillY, (int)(c0X + cardW - 3), brightFillY, 0xFDC0);
            target.drawLine((int)c0X + 3, brightFillY + 1, (int)(c0X + cardW - 3), brightFillY + 1, 0xFDC0);
        }

        // Card Border
        target.drawRoundRect((int)c0X, (int)sliderCardY, (int)cardW, (int)sliderCardH, 10, target.color565(34, 38, 48));

        // Top Left: Sun Icon (Amber)
        int sCx = (int)(c0X + 16.0f);
        int sCy = (int)(sliderCardY + 16.0f);
        target.drawCircle(sCx, sCy, 4, 0xFDC0);
        for (int r = 0; r < 8; ++r)
        {
            float ang = r * 0.785f;
            target.drawLine(sCx + (int)(std::cos(ang) * 5.0f), sCy + (int)(std::sin(ang) * 5.0f),
                            sCx + (int)(std::cos(ang) * 7.0f), sCy + (int)(std::sin(ang) * 7.0f), 0xFDC0);
        }

        // Top Right: Percentage Text (e.g. 52%)
        target.setFont(&fonts::DejaVu9);
        target.setTextDatum(textdatum_t::top_right);
        target.setTextColor(0xFDC0);
        char bBuf[16];
        snprintf(bBuf, sizeof(bBuf), "%d%%", (int)(brightPct * 100.0f + 0.5f));
        target.drawString(bBuf, (int)(c0X + cardW - 10.0f), (int)(sliderCardY + 12.0f));

        // Bottom Left Label: "OLED LUX"
        target.setTextDatum(textdatum_t::bottom_left);
        target.setTextColor(0xFDC0);
        target.drawString("OLED LUX", (int)(c0X + 10.0f), (int)(sliderCardY + sliderCardH - 8.0f));

        // ─────────────────── RIGHT CARD: REC GAIN ───────────────────
        float c1X = 124.0f;
        uint8_t curVol = AudioManager::instance().getVolume();
        float volPct = curVol / 100.0f;
        int volFillY = (int)(sliderCardY + sliderCardH * (1.0f - volPct));
        int volFillH = (int)(sliderCardY + sliderCardH - volFillY);

        // Card base
        target.fillRoundRect((int)c1X, (int)sliderCardY, (int)cardW, (int)sliderCardH, 10, target.color565(18, 20, 26));

        // Filled lower region (deep cyan/teal fill)
        if (volFillH > 0)
        {
            target.setClipRect((int)c1X, volFillY, (int)cardW, volFillH);
            target.fillRoundRect((int)c1X, (int)sliderCardY, (int)cardW, (int)sliderCardH, 10, target.color565(12, 45, 52));
            target.clearClipRect();

            // Glowing Cyan divider bar at volFillY
            target.drawLine((int)c1X + 3, volFillY, (int)(c1X + cardW - 3), volFillY, 0x3DFE);
            target.drawLine((int)c1X + 3, volFillY + 1, (int)(c1X + cardW - 3), volFillY + 1, 0x3DFE);
        }

        // Card Border
        target.drawRoundRect((int)c1X, (int)sliderCardY, (int)cardW, (int)sliderCardH, 10, target.color565(34, 38, 48));

        // Top Left: Soundwave / Speaker Icon (Cyan)
        int vCx = (int)(c1X + 16.0f);
        int vCy = (int)(sliderCardY + 16.0f);
        target.drawCircle(vCx - 2, vCy, 3, 0x3DFE);
        target.drawLine(vCx - 2, vCy - 3, vCx + 3, vCy - 6, 0x3DFE);
        target.drawLine(vCx + 3, vCy - 6, vCx + 3, vCy + 6, 0x3DFE);
        target.drawLine(vCx + 3, vCy + 6, vCx - 2, vCy + 3, 0x3DFE);
        target.drawCircle(vCx + 6, vCy, 4, 0x3DFE);

        // Top Right: Percentage Text (e.g. 85%)
        target.setFont(&fonts::DejaVu9);
        target.setTextDatum(textdatum_t::top_right);
        target.setTextColor(0x3DFE);
        char vBuf[16];
        snprintf(vBuf, sizeof(vBuf), "%d%%", curVol);
        target.drawString(vBuf, (int)(c1X + cardW - 10.0f), (int)(sliderCardY + 12.0f));

        // Bottom Left Label: "REC GAIN"
        target.setTextDatum(textdatum_t::bottom_left);
        target.setTextColor(0x3DFE);
        target.drawString("REC GAIN", (int)(c1X + 10.0f), (int)(sliderCardY + sliderCardH - 8.0f));

        return navTarget;
    }
}
