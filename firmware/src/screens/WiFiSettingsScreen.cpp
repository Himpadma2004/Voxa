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
        
        int16_t n = WiFi.scanNetworks(false, false); // Block for scan (takes ~1.5 sec)
        if (n > 0)
        {
            for (int i = 0; i < n; ++i)
            {
                // De-duplicate SSID names
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
        
        // Sort by RSSI strength (strongest first)
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

        // 0. Handle Wizard State Transition from Keyboard Screen
        if (m_wizardState == WizardState::InputManualSSID)
        {
            m_manualSSID = TextInputScreen::getResult();
            if (!m_manualSSID.empty())
            {
                TextInputScreen::prepare("Enter Password", ScreenId::WiFiSettings, true);
                m_wizardState = WizardState::InputManualPassword;
                canvas.deleteSprite();
                return ScreenId::TextInput; // Transit immediately to keyboard
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

        // 1. Initial Scan
        if (!m_hasScanned && !m_isScanning)
        {
            // Draw "Scanning..."
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Wi-Fi", true, false, Icon::Rotate, w, h);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setFont(&fonts::FreeSansBold12pt7b);
            canvas.setTextColor(VoxaTheme::getTextPrimary());
            canvas.drawString("Scanning Wi-Fi...", w / 2, h / 2);
            canvas.pushSprite(0, 0);
            
            performScan();
        }

        ScreenId targetScreen = ScreenId::WiFiSettings;
        uint32_t lastMs = millis();

        float contentHeight = (m_networks.size() + 2) * 50.0f + 20.0f;
        float visibleHeight = h - 70.0f - 18.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        while (targetScreen == ScreenId::WiFiSettings)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            // 1. Process Touch
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
                        int totalItems = m_networks.size() + 2;
                        for (int i = 0; i < totalItems; ++i)
                        {
                            float itemY = 72.0f + i * 50.0f - m_scrollY;
                            if (tx >= leftX && tx <= (leftX + cardW) &&
                                ty >= itemY && ty <= (itemY + 44.0f))
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
                            break; // break loop to trigger rescan
                        }
                        else if (m_pressedItemIndex >= 0)
                        {
                            if (m_pressedItemIndex == 0)
                            {
                                // Add network manually
                                TextInputScreen::prepare("Enter SSID", ScreenId::WiFiSettings, false);
                                m_wizardState = WizardState::InputManualSSID;
                                targetScreen = ScreenId::TextInput;
                            }
                            else if (m_pressedItemIndex == 1)
                            {
                                // Clear credentials
                                wifiManager.clearCredentials();
                                wifiManager.disconnect();
                            }
                            else
                            {
                                // Connect to scanned network
                                int netIdx = m_pressedItemIndex - 2;
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
                    m_isBackPressed = false;
                    m_isRefreshPressed = false;
                    m_pressedItemIndex = -1;
                }
            }

            // Perform Scroll Inertia
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

            // 2. Render Screen Layout
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Wi-Fi", true, true, Icon::Rotate, w, h);

            float leftX = w * 0.04f;
            float cardW = w * 0.92f;

            canvas.setClipRect(0, 70, w, h - 70 - 18);

            // Row 0: Add network manually
            {
                float itemY = 72.0f - m_scrollY;
                uint16_t keyColor = (m_pressedItemIndex == 0) ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                canvas.fillRoundRect(leftX, itemY, cardW, 44, 8, keyColor);
                ScreenCommon::drawIcon(canvas, Icon::Plus, leftX + 12, itemY + 12, 20, (m_pressedItemIndex == 0) ? TFT_WHITE : VoxaTheme::getPrimary());
                
                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextColor((m_pressedItemIndex == 0) ? TFT_WHITE : VoxaTheme::getTextPrimary());
                canvas.drawString("Add Manually", leftX + 44, itemY + 14);
                
                canvas.setTextColor((m_pressedItemIndex == 0) ? TFT_WHITE : VoxaTheme::getTextSecondary());
                canvas.drawString("Enter SSID and Password", leftX + 44, itemY + 30);
            }

            // Row 1: Clear credentials / disconnect
            {
                float itemY = 72.0f + 50.0f - m_scrollY;
                uint16_t keyColor = (m_pressedItemIndex == 1) ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                canvas.fillRoundRect(leftX, itemY, cardW, 44, 8, keyColor);
                ScreenCommon::drawIcon(canvas, Icon::Settings, leftX + 12, itemY + 12, 20, (m_pressedItemIndex == 1) ? TFT_WHITE : 0xF800);
                
                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextColor((m_pressedItemIndex == 1) ? TFT_WHITE : VoxaTheme::getTextPrimary());
                canvas.drawString("Forget Wi-Fi", leftX + 44, itemY + 14);
                
                canvas.setTextColor((m_pressedItemIndex == 1) ? TFT_WHITE : VoxaTheme::getTextSecondary());
                canvas.drawString("Forget credentials & disconnect", leftX + 44, itemY + 30);
            }

            // Scanned rows starting at index 2
            for (size_t i = 0; i < m_networks.size(); ++i)
            {
                int itemIdx = i + 2;
                float itemY = 72.0f + itemIdx * 50.0f - m_scrollY;
                if (itemY < 20.0f || itemY > h - 18.0f) continue; // Skip off-screen rendering

                uint16_t keyColor = (m_pressedItemIndex == itemIdx) ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                canvas.fillRoundRect(leftX, itemY, cardW, 44, 8, keyColor);

                // Highlight currently connected SSID with Primary Theme Color
                bool isCurrent = wifiManager.isConnected() && (wifiManager.getSSID() == m_networks[i].ssid);
                uint16_t iconColor = isCurrent ? VoxaTheme::getPrimary() : VoxaTheme::getTextPrimary();
                if (m_pressedItemIndex == itemIdx) iconColor = TFT_WHITE;

                ScreenCommon::drawIcon(canvas, Icon::Wifi, leftX + 12, itemY + 12, 20, iconColor);
                
                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextColor((m_pressedItemIndex == itemIdx) ? TFT_WHITE : VoxaTheme::getTextPrimary());
                canvas.drawString(m_networks[i].ssid.c_str(), leftX + 44, itemY + 14);
                
                canvas.setTextColor((m_pressedItemIndex == itemIdx) ? TFT_WHITE : VoxaTheme::getTextSecondary());
                std::string secureText = m_networks[i].isSecure ? "Secure" : "Open";
                if (isCurrent) secureText += " (Connected)";
                canvas.drawString(secureText.c_str(), leftX + 44, itemY + 30);
            }

            canvas.clearClipRect();

            // Slide in / double buffer push
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
