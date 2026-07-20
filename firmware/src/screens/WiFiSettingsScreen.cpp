#include "WiFiSettingsScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/WiFiManager.h"
#include "Transition.h"
#include "TextInputScreen.h"
#include <WiFi.h>
#include <cmath>
#include <algorithm>

namespace VOXA
{
    void WiFiSettingsScreen::performScan()
    {
        m_networks.clear();
        m_isScanning = true;
        
        Serial.println("[WiFiSettings] Scanning for networks...");
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        
        int16_t n = WiFi.scanNetworks(false, false);
        if (n > 0)
        {
            for (int i = 0; i < n; ++i)
            {
                bool dup = false;
                std::string ssid = WiFi.SSID(i).c_str();
                for (const auto& net : m_networks)
                {
                    if (net.ssid == ssid)
                    {
                        dup = true;
                        break;
                    }
                }
                if (!dup && !ssid.empty())
                {
                    m_networks.push_back({
                        ssid,
                        WiFi.RSSI(i),
                        WiFi.encryptionType(i) != WIFI_AUTH_OPEN
                    });
                }
            }
        }
        
        std::sort(m_networks.begin(), m_networks.end(), [](const WiFiNetwork& a, const WiFiNetwork& b) {
            return a.rssi > b.rssi;
        });

        m_isScanning = false;
        m_hasScanned = true;
        Serial.printf("[WiFiSettings] Scan finished. Found %u unique SSIDs\n", m_networks.size());
    }

    ScreenId WiFiSettingsScreen::show(Touch& touch)
    {
        int entryFrame = 0;
        float dragStartX = 0.0f;
        float dragStartY = 0.0f;
        bool swipeBackCandidate = false;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            return ScreenId::Settings;
        }

        // Handle Wizard State Transition from Keyboard Screen
        if (m_wizardState == WizardState::InputManualSSID)
        {
            m_manualSSID = TextInputScreen::getResult();
            if (!m_manualSSID.empty())
            {
                TextInputScreen::prepare("Enter Password", ScreenId::WiFiSettings, true);
                m_wizardState = WizardState::InputManualPassword;
                canvas.deleteSprite();
                return ScreenId::TextInput;
            }
            else
            {
                m_wizardState = WizardState::None;
            }
        }
        else if (m_wizardState == WizardState::InputManualPassword)
        {
            std::string password = TextInputScreen::getResult();
            wifiManager.saveCredentials(m_manualSSID, password);
            wifiManager.connect();
            m_wizardState = WizardState::None;
        }
        else if (m_wizardState == WizardState::InputSelectedPassword)
        {
            std::string password = TextInputScreen::getResult();
            wifiManager.saveCredentials(m_selectedSSID, password);
            wifiManager.connect();
            m_wizardState = WizardState::None;
        }

        // Initial Scan
        if (!m_hasScanned && !m_isScanning)
        {
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Wi-Fi Networks", true, false, Icon::Rotate, w, h);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setFont(&fonts::FreeSansBold12pt7b);
            canvas.setTextColor(VoxaTheme::getTextPrimary());
            canvas.drawString("Scanning Wi-Fi...", w / 2, h / 2);
            canvas.pushSprite(0, 0);
            
            performScan();
        }

        ScreenId targetScreen = ScreenId::WiFiSettings;
        uint32_t lastMs = millis();

        int totalItems = (int)m_networks.size() + 2;
        float contentHeight = totalItems * 52.0f + 10.0f;
        float visibleHeight = h - 70.0f - 18.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        while (targetScreen == ScreenId::WiFiSettings)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            totalItems = (int)m_networks.size() + 2;
            contentHeight = totalItems * 52.0f + 10.0f;
            maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            // Process Touch
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            if (touched && entryFrame >= 10)
            {
                m_lastDragX = tx;
                m_lastDragY = ty;

                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    dragStartX = tx;
                    dragStartY = ty;
                    swipeBackCandidate = (tx < 50);
                    m_dragStartY = ty;
                    m_dragStartScrollY = m_targetScrollY;
                    m_lastTouchSampleMs = nowMs;
                    m_isDragging = false;
                    m_scrollVelocity = 0.0f;

                    // Back button bounds
                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isBackPressed = true;
                    }

                    // Refresh Button bounds (top-right action)
                    if (std::sqrt((tx - (w - 20.0f))*(tx - (w - 20.0f)) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isRefreshPressed = true;
                    }

                    // Card checks
                    if (ty >= 70.0f && ty <= (h - 18.0f))
                    {
                        float leftX = w * 0.04f;
                        float cardW = w * 0.92f;
                        for (int i = 0; i < totalItems; ++i)
                        {
                            float itemY = 72.0f + i * 52.0f - m_scrollY;
                            if (tx >= leftX && tx <= (leftX + cardW) &&
                                ty >= itemY && ty <= (itemY + 46.0f))
                            {
                                m_pressedItemIndex = i;
                            }
                        }
                    }
                }
                else
                {
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        targetScreen = ScreenId::Settings;
                        swipeBackCandidate = false;
                    }

                    float dy = ty - m_dragStartY;
                    if (!m_isDragging && std::abs(dy) > 10.0f)
                    {
                        m_isDragging = true;
                        m_isBackPressed = false;
                        m_isRefreshPressed = false;
                        m_pressedItemIndex = -1;
                    }

                    if (m_isDragging)
                    {
                        m_targetScrollY = m_dragStartScrollY - dy;
                        m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));

                        uint32_t dt = nowMs - m_lastTouchSampleMs;
                        if (dt > 0)
                        {
                            m_scrollVelocity = -dy / (dt / 1000.0f);
                        }
                        m_lastTouchSampleMs = nowMs;
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;

                    if (m_isDragging)
                    {
                        m_isDragging = false;
                    }
                    else
                    {
                        if (m_isBackPressed)
                        {
                            targetScreen = ScreenId::Settings;
                        }
                        else if (m_isRefreshPressed)
                        {
                            m_hasScanned = false;
                            targetScreen = ScreenId::WiFiSettings;
                            break;
                        }
                        else if (m_pressedItemIndex >= 0)
                        {
                            if (m_pressedItemIndex == 0)
                            {
                                TextInputScreen::prepare("Enter SSID", ScreenId::WiFiSettings, false);
                                m_wizardState = WizardState::InputManualSSID;
                                targetScreen = ScreenId::TextInput;
                            }
                            else if (m_pressedItemIndex == 1)
                            {
                                wifiManager.clearCredentials();
                                wifiManager.disconnect();
                            }
                            else
                            {
                                int netIdx = m_pressedItemIndex - 2;
                                if (netIdx >= 0 && netIdx < (int)m_networks.size())
                                {
                                    m_selectedSSID = m_networks[netIdx].ssid;
                                    if (m_networks[netIdx].isSecure)
                                    {
                                        TextInputScreen::prepare("Enter Password", ScreenId::WiFiSettings, true);
                                        m_wizardState = WizardState::InputSelectedPassword;
                                        targetScreen = ScreenId::TextInput;
                                    }
                                    else
                                    {
                                        wifiManager.saveCredentials(m_selectedSSID, "");
                                        wifiManager.connect();
                                    }
                                }
                            }
                        }
                    }
                    m_isBackPressed = false;
                    m_isRefreshPressed = false;
                    m_pressedItemIndex = -1;
                }
            }

            // Scroll Inertia
            if (!m_wasTouched && std::abs(m_scrollVelocity) > 0.0f)
            {
                m_targetScrollY += m_scrollVelocity * deltaSecs;
                m_scrollVelocity *= std::pow(0.85f, deltaSecs * 60.0f);
                if (std::abs(m_scrollVelocity) < 5.0f)
                {
                    m_scrollVelocity = 0.0f;
                }
            }

            m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));
            m_scrollY += (m_targetScrollY - m_scrollY) * 15.0f * deltaSecs;
            if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
            {
                m_scrollY = m_targetScrollY;
            }

            // Render Layout
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Wi-Fi Networks", true, true, Icon::Rotate, w, h);

            // Back button
            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, backFill, backColor, w, h);

            // Refresh button
            uint16_t refFill = m_isRefreshPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t refColor = m_isRefreshPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, w - 20.0f, 45.0f, Icon::Rotate, refFill, refColor, w, h);

            float leftX = w * 0.04f;
            float cardW = w * 0.92f;

            canvas.setClipRect(0, 70, w, h - 70 - 18);

            for (int i = 0; i < totalItems; ++i)
            {
                float itemY = 72.0f + i * 52.0f - m_scrollY;
                if (itemY + 46.0f < 70.0f || itemY > (h - 18.0f))
                    continue;

                bool isPressed = (m_pressedItemIndex == i);
                uint16_t cardBg = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                uint16_t cardBorder = isPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getDivider();
                uint16_t titleColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
                uint16_t subColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextSecondary();

                canvas.fillRoundRect((int)leftX, (int)itemY, (int)cardW, 46, 8, cardBg);
                canvas.drawRoundRect((int)leftX, (int)itemY, (int)cardW, 46, 8, cardBorder);

                float cy = itemY + 23.0f;
                float iconCx = leftX + 22.0f;

                if (i == 0)
                {
                    // Add Network Manually
                    canvas.fillCircle((int)iconCx, (int)cy, 12, 0x067F);
                    ScreenCommon::drawIcon(canvas, Icon::Plus, iconCx - 6.0f, cy - 6.0f, 12.0f, VoxaTheme::getBackground());

                    canvas.setFont(&fonts::FreeSans9pt7b);
                    canvas.setTextDatum(textdatum_t::middle_left);
                    canvas.setTextColor(titleColor);
                    canvas.drawString("Add Network Manually", leftX + 42.0f, cy - 8.0f);

                    canvas.setTextColor(subColor);
                    canvas.drawString("Enter SSID and password", leftX + 42.0f, cy + 8.0f);
                }
                else if (i == 1)
                {
                    // Forget Wi-Fi
                    canvas.fillCircle((int)iconCx, (int)cy, 12, canvas.color565(220, 60, 60));
                    ScreenCommon::drawIcon(canvas, Icon::Settings, iconCx - 6.0f, cy - 6.0f, 12.0f, VoxaTheme::getBackground());

                    canvas.setFont(&fonts::FreeSans9pt7b);
                    canvas.setTextDatum(textdatum_t::middle_left);
                    canvas.setTextColor(titleColor);
                    canvas.drawString("Forget Wi-Fi Network", leftX + 42.0f, cy - 8.0f);

                    canvas.setTextColor(subColor);
                    canvas.drawString("Clear credentials & disconnect", leftX + 42.0f, cy + 8.0f);
                }
                else
                {
                    int netIdx = i - 2;
                    bool isCurrent = wifiManager.isConnected() && (wifiManager.getSSID() == m_networks[netIdx].ssid);
                    uint16_t wifiIconCol = isCurrent ? 0x07E0 : 0x79CF; // Green if connected, blue if available

                    canvas.fillCircle((int)iconCx, (int)cy, 12, wifiIconCol);
                    ScreenCommon::drawIcon(canvas, Icon::Wifi, iconCx - 6.0f, cy - 6.0f, 12.0f, VoxaTheme::getBackground());

                    canvas.setFont(&fonts::FreeSans9pt7b);
                    canvas.setTextDatum(textdatum_t::middle_left);
                    canvas.setTextColor(titleColor);

                    std::string drawSSID = m_networks[netIdx].ssid;
                    if (drawSSID.length() > 16)
                    {
                        drawSSID = drawSSID.substr(0, 14) + "...";
                    }
                    canvas.drawString(drawSSID.c_str(), leftX + 42.0f, cy - 8.0f);

                    canvas.setTextColor(subColor);
                    std::string subText = m_networks[netIdx].isSecure ? "Secure Network" : "Open Network";
                    if (isCurrent) subText += "  |  Connected";
                    canvas.drawString(subText.c_str(), leftX + 42.0f, cy + 8.0f);

                    // Connected badge or chevron
                    if (isCurrent)
                    {
                        float chevX = leftX + cardW - 20.0f;
                        canvas.fillCircle((int)chevX, (int)cy, 5, 0x07E0); // Green dot indicator
                    }
                    else
                    {
                        float chevX = leftX + cardW - 16.0f;
                        ScreenCommon::drawIcon(canvas, Icon::ChevronRight, chevX - 5.0f, cy - 5.0f, 10.0f, subColor);
                    }
                }
            }

            canvas.clearClipRect();

            if (entryFrame < 10)
            {
                playSlideInFrame(canvas, getTransitionType(g_lastScreenId, ScreenId::WiFiSettings), entryFrame, 10);
                entryFrame++;
            }
            else
            {
                canvas.pushSprite(0, 0);
            }
            delay(16);
        }

        canvas.deleteSprite();
        return targetScreen;
    }
}
