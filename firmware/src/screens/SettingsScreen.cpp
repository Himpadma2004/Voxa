#include "SettingsScreen.h"
#include "ScreenCommon.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../audio/AudioManager.h"
#include "../services/SettingsService.h"
#include "../services/WiFiManager.h"
#include "../storage/SpiffsMutex.h"
#include "Transition.h"
#include "../services/ApiClient.h"
#include "../services/PowerManager.h"
#include <SPIFFS.h>
#include "../services/TimeService.h"

#include <cmath>
#include <algorithm>

namespace VOXA
{
    extern SettingsService settingsService;


    ScreenId SettingsScreen::show(Touch& touch)
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
            return ScreenId::Home;
        }

        ScreenId targetScreen = ScreenId::Settings;
        uint32_t lastMs = millis();

        float contentHeight = 50.0f + 10.0f * 52.0f + 20.0f;  // 10 rows * 52px + padding
        float visibleHeight = h - 46.0f;                        // below header divider at y=44
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        Settings settings = settingsService.getSettings();

        while (targetScreen == ScreenId::Settings)
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

                    // Card checks
                    if (ty >= 68.0f && ty <= (h - 6.0f))
                    {
                        float leftX = w * 0.04f;
                        float cardW = w * 0.92f;
                        for (int i = 0; i < 10; ++i)
                        {
                            float itemY = 72.0f + i * 52.0f - m_scrollY;
                            if (tx >= leftX && tx <= (leftX + cardW) &&
                                ty >= itemY && ty <= (itemY + 48.0f))
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
                        targetScreen = ScreenId::Home;
                        swipeBackCandidate = false;
                    }

                    float dy = ty - m_dragStartY;
                    if (!m_isDragging && std::abs(dy) > 10.0f)
                    {
                        m_isDragging = true;
                        m_isBackPressed = false;
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
                    float rx = m_lastDragX;
                    float ry = m_lastDragY;

                    if (m_isDragging)
                    {
                        m_isDragging = false;
                    }
                    else
                    {
                        if (m_isBackPressed)
                        {
                            targetScreen = ScreenId::Home;
                        }

                        if (m_pressedItemIndex >= 0)
                        {
                            Settings currentSettings = settingsService.getSettings();
                            if (m_pressedItemIndex == 0)
                            {
                                // Toggle Appearance (Dark Mode <-> Light Mode)
                                VoxaTheme::ThemeMode nextTheme = (VoxaTheme::getThemeMode() == VoxaTheme::ThemeMode::Dark)
                                    ? VoxaTheme::ThemeMode::Light : VoxaTheme::ThemeMode::Dark;
                                VoxaTheme::setThemeMode(nextTheme);
                                AudioManager::instance().playTapSoundAsync();
                                Serial.printf("[Settings] Appearance switched to: %s\n", 
                                              nextTheme == VoxaTheme::ThemeMode::Dark ? "Dark Mode" : "Light Mode");
                            }
                            else if (m_pressedItemIndex == 1)
                            {
                                // Open Wi-Fi Settings screen
                                targetScreen = ScreenId::WiFiSettings;
                            }
                            else if (m_pressedItemIndex == 2)
                            {
                                // Open Sync & Backup page
                                targetScreen = ScreenId::SyncStatus;
                            }
                            else if (m_pressedItemIndex == 3)
                            {
                                // Reboot to Setup Mode (same as Wi-Fi toggle when no credentials)
                                Serial.println("[Settings] Entering Portal Setup Mode via reboot to configure API URL...");
                                wifiManager.setForcePortal(true);
                                delay(500);
                                ESP.restart();
                            }
                            else if (m_pressedItemIndex == 7)
                            {
                                // Clean smartphone-style Restart
                                PowerManager::instance().restartDevice();
                            }
                            else if (m_pressedItemIndex == 8)
                            {
                                // Clean smartphone-style Power Off (Deep Sleep with GPIO1 button wake)
                                PowerManager::instance().shutdownDevice();
                            }
                            else if (m_pressedItemIndex == 9)
                            {
                                // Complete Factory Reset
                                PowerManager::instance().factoryReset();
                            }
                        }
                    }
                    m_isBackPressed = false;
                    m_pressedItemIndex = -1;
                }
            }

            // Dimensions re-query for rotation reflows
            w = Display::width();
            h = Display::height();
            visibleHeight = h - 70.0f - 18.0f;
            maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            // 2. Perform Scroll Inertia
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

            // 3. Render - Minimal black/white design (matches TasksScreen / ReminderScreen)
            canvas.fillScreen(0x0000); // Pitch black

            // --- Top Status Bar ---
            canvas.setFont(&fonts::Font0);
            canvas.setTextDatum(textdatum_t::top_left);
            canvas.setTextColor(0xFFFF);
            std::string timeStr = timeService.getCurrentTime();
            if (timeStr.empty()) timeStr = "10:42";
            if (timeStr.length() > 5) timeStr = timeStr.substr(0, 5);
            canvas.drawString(timeStr.c_str(), 12, 10);

            canvas.setTextDatum(textdatum_t::top_right);
            canvas.setTextColor(0xFDC0);
            canvas.drawString("VOXA OS", w - 20, 10);
            // Flash bolt icon
            canvas.fillTriangle(w - 14, 10, w - 18, 16, w - 13, 16, 0xFDC0);
            canvas.fillTriangle(w - 15, 15, w - 10, 15, w - 14, 21, 0xFDC0);

            // --- Sub-Header ---
            canvas.setTextDatum(textdatum_t::middle_left);
            canvas.setFont(&fonts::Font0);
            canvas.setTextColor(0x94A3B8);
            canvas.drawString("< Hub", 12, 34);

            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextColor(0xFFFF);
            canvas.drawString("SETTINGS", 56, 33);

            // Gear icon on right
            uint16_t gicC = 0xFDC0;
            canvas.drawCircle(w - 18, 33, 5, gicC);
            canvas.drawCircle(w - 18, 33, 2, gicC);
            canvas.drawLine(w - 18, 26, w - 18, 28, gicC);
            canvas.drawLine(w - 18, 38, w - 18, 40, gicC);
            canvas.drawLine(w - 25, 33, w - 23, 33, gicC);
            canvas.drawLine(w - 13, 33, w - 11, 33, gicC);

            // Thin divider under header
            canvas.drawFastHLine(0, 44, w, canvas.color565(30, 32, 40));

            // Build live data strings
            std::string wifiStatus = "Disconnected";
            if (settings.wifiEnabled)
            {
                if (wifiManager.isConnected())      wifiStatus = wifiManager.getSSID();
                else if (wifiManager.hasSavedCredentials()) wifiStatus = "Connecting...";
                else                                wifiStatus = "No Saved Wi-Fi";
            }
            std::string syncStatus  = settings.autoSync ? "Auto Sync: On" : "Auto Sync: Off";
            std::string storageInfo;
            {
                size_t spiffsTotal = SPIFFS.totalBytes();
                size_t spiffsUsed  = SPIFFS.usedBytes();
                size_t spiffsFree  = spiffsTotal > spiffsUsed ? spiffsTotal - spiffsUsed : 0;
                char buf[64];
                snprintf(buf, sizeof(buf), "%.1f MB free", spiffsFree / (1024.0f * 1024.0f));
                storageInfo = buf;
            }
            std::string deviceInfo  = settings.deviceName + "  v" + settings.firmwareVersion;
            std::string backendUrl  = VOXA::apiClient.getBaseUrl();
            std::string themeLabel  = VoxaTheme::isDarkMode() ? "Dark Mode" : "Light Mode";

            // Row definitions: { label, subtitle, accent color, icon }
            struct SRow { const char* label; std::string sub; uint16_t dot; Icon icon; };
            SRow rows[10] = {
                { "Appearance",    themeLabel,    0x4208,  VoxaTheme::isDarkMode() ? Icon::Moon : Icon::Sun },
                { "Wi-Fi",         wifiStatus,    0x0439,  Icon::Wifi      },
                { "Sync & Backup", syncStatus,    0x024F,  Icon::Cloud     },
                { "Backend URL",   backendUrl,    0x3186,  Icon::Upload    },
                { "Storage",       storageInfo,   0x294A,  Icon::Storage   },
                { "Device Info",   deviceInfo,    0x3A69,  Icon::Info      },
                { "About VOXA",    "AI Companion",0x2965,  Icon::Spark     },
                { "Restart",       "Reboot device",0x5AA0, Icon::Rotate   },
                { "Power Off",     "Deep sleep",  0x6000,  Icon::Power     },
                { "Factory Reset", "Clear all data",0x6000,Icon::Reset    }
            };

            float leftX = w * 0.04f;
            float cardW = w * 0.92f;

            canvas.setClipRect(0, 46, w, h - 46);

            for (int i = 0; i < 10; ++i)
            {
                float itemY = 50.0f + i * 52.0f - m_scrollY;
                if (itemY + 48.0f < 46.0f || itemY > (float)h + 10.0f)
                    continue;

                bool isPressed = (m_pressedItemIndex == i);

                // Card
                uint16_t cardBg     = isPressed ? canvas.color565(30, 32, 42) : canvas.color565(14, 16, 20);
                uint16_t cardBorder = isPressed ? 0xFDC0 : canvas.color565(32, 36, 48);
                canvas.fillRoundRect((int)leftX, (int)itemY, (int)cardW, 46, 8, cardBg);
                canvas.drawRoundRect((int)leftX, (int)itemY, (int)cardW, 46, 8, cardBorder);

                // Icon tile (small rounded square) + icon
                int iconTileX = (int)leftX + 6;
                int iconTileY = (int)itemY + 11;
                // Danger rows get red tile, others get muted tinted tile
                uint16_t tileFill = (i >= 8) ? canvas.color565(40, 8, 8)
                                             : canvas.color565(22, 26, 36);
                uint16_t tileBorder = (i >= 8) ? rows[i].dot : canvas.color565(44, 50, 66);
                canvas.fillRoundRect(iconTileX, iconTileY, 24, 24, 4, tileFill);
                canvas.drawRoundRect(iconTileX, iconTileY, 24, 24, 4, tileBorder);
                // Icon drawn centred inside tile
                uint16_t iconColor = isPressed ? 0xFDC0 : rows[i].dot;
                ScreenCommon::drawIcon(canvas, rows[i].icon,
                                       iconTileX + 4.0f, iconTileY + 4.0f, 16.0f, iconColor);

                // Label (white)
                canvas.setFont(&fonts::Font0);
                canvas.setTextDatum(textdatum_t::middle_left);
                canvas.setTextColor(isPressed ? 0xFDC0 : 0xFFFF);
                canvas.drawString(rows[i].label, (int)leftX + 38, (int)itemY + 14);

                // Subtitle (grey)
                canvas.setTextColor(isPressed ? 0xCBD5E1 : 0x7B8EA8);
                std::string sub = rows[i].sub;
                if (sub.length() > 30) sub = sub.substr(0, 28) + "..";
                canvas.drawString(sub.c_str(), (int)leftX + 38, (int)itemY + 31);

                // Chevron on right for navigable rows (Wi-Fi=1, Sync=2)
                if (i == 1 || i == 2)
                {
                    int cx = (int)(leftX + cardW) - 14;
                    int cy = (int)itemY + 23;
                    canvas.drawLine(cx, cy - 5, cx + 4, cy, isPressed ? 0xFDC0 : 0x555555);
                    canvas.drawLine(cx + 4, cy, cx, cy + 5, isPressed ? 0xFDC0 : 0x555555);
                }

                // Tag badge for dangerous actions
                if (i == 8 || i == 9)
                {
                    uint16_t tagC = (i == 9) ? 0xD000 : 0xF800;
                    canvas.fillRoundRect((int)(leftX + cardW) - 38, (int)itemY + 14, 28, 14, 3, canvas.color565(30, 8, 8));
                    canvas.drawRoundRect((int)(leftX + cardW) - 38, (int)itemY + 14, 28, 14, 3, tagC);
                    canvas.setTextDatum(textdatum_t::middle_center);
                    canvas.setTextColor(tagC);
                    canvas.drawString(i == 9 ? "RISK" : "OFF", (int)(leftX + cardW) - 24, (int)itemY + 21);
                }
            }

            canvas.clearClipRect();
            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Settings), entryFrame, 10);
                entryFrame++;
            }
            else
            {
                canvas.pushSprite(0, 0);
            }

            uint32_t frameMs = millis() - nowMs;
            if (frameMs < 16)
            {
                delay(16 - frameMs);
            }
        }

        canvas.deleteSprite();
        return targetScreen;
    }
}
