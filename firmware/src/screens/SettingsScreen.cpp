#include "SettingsScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/SettingsService.h"
#include "Transition.h"
#include <SPIFFS.h>
#include <cmath>
#include <algorithm>

namespace VOXA
{
    extern SettingsService settingsService;

    struct SettingRow
    {
        Icon icon;
        const char* title;
        std::string subtitle;
        uint16_t color;
    };

    ScreenId SettingsScreen::show(Touch& touch)
    {
        int entryFrame = 0;
        float dragStartX = 0.0f;
        float dragStartY = 0.0f;
        bool swipeBackCandidate = false;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            return ScreenId::Home;
        }

        ScreenId targetScreen = ScreenId::Settings;
        uint32_t lastMs = millis();

        float contentHeight = 9.0f * 50.0f + 10.0f; // 9 settings items
        float visibleHeight = h - 70.0f - 18.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

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
                    if (ty >= 70.0f && ty <= (h - 18.0f))
                    {
                        float leftX = w * 0.04f;
                        float cardW = w * 0.92f;
                        for (int i = 0; i < 9; ++i)
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
                                // Toggle Wi-Fi
                                currentSettings.wifiEnabled = !currentSettings.wifiEnabled;
                                settingsService.updateSettings(currentSettings);
                                Serial.println("[Settings] Toggle Wi-Fi");
                            }
                            else if (m_pressedItemIndex == 1)
                            {
                                // Open Sync & Backup page
                                targetScreen = ScreenId::SyncStatus;
                            }
                            else if (m_pressedItemIndex == 5)
                            {
                                Serial.println("[Settings] Restarting...");
                                delay(200);
                                ESP.restart();
                            }
                            else if (m_pressedItemIndex == 6)
                            {
                                Serial.println("[Settings] Powering Off...");
                                delay(200);
                                esp_deep_sleep_start();
                            }
                            else if (m_pressedItemIndex == 7)
                            {
                                Serial.println("[Settings] Shutting Down...");
                                delay(200);
                                esp_deep_sleep_start();
                            }
                            else if (m_pressedItemIndex == 8)
                            {
                                Serial.println("[Settings] Factory Resetting...");
                                SPIFFS.remove("/reminders.json");
                                SPIFFS.remove("/memory.json");
                                SPIFFS.remove("/ideas.json");
                                SPIFFS.remove("/questions.json");
                                SPIFFS.remove("/settings.json");
                                SPIFFS.remove("/history.json");
                                SPIFFS.remove("/recordings.json");
                                delay(500);
                                ESP.restart();
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

            // 3. Render Settings
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Settings", true, false, Icon::Plus, w, h);

            Settings settings = settingsService.getSettings();
            
            std::string wifiStatus = settings.wifiEnabled ? "Connected" : "Disconnected";
            std::string syncStatus = settings.autoSync ? "Auto Sync: On" : "Auto Sync: Off";
            std::string storageInfo = "12.4 GB / 32 GB";
            std::string deviceInfo = settings.deviceName + " (v" + settings.firmwareVersion + ")";

            SettingRow rows[9] = {
                { Icon::Wifi,       "Wi-Fi",          wifiStatus,  0x266C },
                { Icon::Wifi,       "Sync & Backup",  syncStatus,  0x067F },
                { Icon::Folder,     "Storage",        storageInfo, 0x52AA },
                { Icon::Question,   "Device Info",    deviceInfo,  0xAD55 },
                { Icon::Settings,   "About VOXA",     "AI Companion", 0x79CF },
                { Icon::Settings,   "Restart",        "Reboot Device", 0xFD20 },
                { Icon::Star,       "Power Off",      "Deep Sleep Mode", 0xF800 },
                { Icon::Star,       "Shut Down",      "Turn off device", 0xF800 },
                { Icon::Folder,     "Factory Reset",  "Clear all data", 0xD000 }
            };

            float leftX = w * 0.04f;
            float cardW = w * 0.92f;

            canvas.setClipRect(0, 70, w, h - 70 - 18);

            for (int i = 0; i < 9; ++i)
            {
                float itemY = 72.0f + i * 50.0f - m_scrollY;
                if (itemY + 44.0f < 70.0f || itemY > (h - 18.0f))
                    continue;

                bool isPressed = (m_pressedItemIndex == i);
                uint16_t cardBg = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                uint16_t cardBorder = isPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getDivider();
                uint16_t labelColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
                uint16_t subColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextSecondary();

                canvas.fillRoundRect((int)leftX, (int)itemY, (int)cardW, 44, 8, cardBg);
                canvas.drawRoundRect((int)leftX, (int)itemY, (int)cardW, 44, 8, cardBorder);

                float cy = itemY + 22.0f;
                float iconCx = leftX + 22.0f;
                canvas.fillCircle((int)iconCx, (int)cy, 12, rows[i].color);
                ScreenCommon::drawIcon(canvas, rows[i].icon, iconCx - 6.0f, cy - 6.0f, 12.0f, VoxaTheme::getBackground());

                canvas.setFont(&fonts::DejaVu12);
                canvas.setTextDatum(textdatum_t::middle_left);
                
                canvas.setTextColor(labelColor);
                canvas.drawString(rows[i].title, leftX + 42.0f, cy - 8.0f);

                canvas.setTextColor(subColor);
                canvas.drawString(rows[i].subtitle.c_str(), leftX + 42.0f, cy + 8.0f);

                if (i == 1) // Sync & Backup has a navigation chevron
                {
                    float chevX = leftX + cardW - 16.0f;
                    ScreenCommon::drawIcon(canvas, Icon::ChevronRight, chevX - 5.0f, cy - 5.0f, 10.0f, subColor);
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

        return targetScreen;
    }
}
