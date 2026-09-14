#include "ReminderManager.h"
#include "ReminderStorage.h"
#include "ReminderScheduler.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../screens/ScreenCommon.h"
#include "../services/WiFiManager.h"
#include "../services/ApiClient.h"
#include "../audio/AudioManager.h"
#include "../services/DataService.h"
#include <Arduino.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern Touch touch;

namespace
{
    struct BackendNotifyParam
    {
        std::string url;
        std::string body;
        bool isJson;
    };

    void backendNotifyTaskFn(void* param)
    {
        auto* data = static_cast<BackendNotifyParam*>(param);
        if (data)
        {
            HTTPClient http;
            http.setTimeout(1500); // 1.5s max timeout — NEVER hangs or freezes UI!
            http.begin(data->url.c_str());
            if (data->isJson)
            {
                http.addHeader("Content-Type", "application/json");
                http.POST(data->body.c_str());
            }
            else
            {
                http.POST("");
            }
            http.end();
            delete data;
        }
        vTaskDelete(nullptr);
    }
}

namespace VOXA
{
    ReminderManager& ReminderManager::instance()
    {
        static ReminderManager inst;
        return inst;
    }

    void ReminderManager::begin()
    {
        loadReminders();
        m_lastTickMs = millis();
        
        // Start HTTP notification listener on port 80
        WebServer* web = new WebServer(80);
        web->on("/api/reminders/notify", HTTP_POST, [this]() {
            handleNotification();
        });
        web->begin();
        m_httpServer = static_cast<void*>(web);
        
        Serial.printf("[ReminderManager] Initialized. Loaded %u reminders. WebServer active on port 80.\n", m_reminders.size());
    }

    void ReminderManager::tick()
    {
        // Poll WebServer requests
        if (m_httpServer)
        {
            static_cast<WebServer*>(m_httpServer)->handleClient();
        }

        uint32_t nowMs = millis();
        if (nowMs - m_lastTickMs >= 1000) // Run checks every 1 second
        {
            m_lastTickMs = nowMs;
            time_t current = getCurrentTime();
            
            // Check for status updates
            bool changed = ReminderScheduler::update(m_reminders, current, m_repeatIntervalSec);
            if (changed)
            {
                saveReminders();
            }

            // Sync on WiFi connection/reconnection
            bool currentlyConnected = wifiManager.isConnected();
            if (currentlyConnected && !m_wasConnected)
            {
                m_wasConnected = true;
                fetchActiveReminders();
            }
            else if (!currentlyConnected)
            {
                m_wasConnected = false;
            }
        }
    }

    void ReminderManager::setTestMode(bool enabled)
    {
        m_testMode = enabled;
        if (enabled)
        {
            m_repeatIntervalSec = 30; // 30 seconds repeat if ignored in test mode
            Serial.println("[ReminderManager] Test Mode ENABLED (Repeat interval set to 30s).");
        }
        else
        {
            m_repeatIntervalSec = 120; // 2 minutes in production
            Serial.println("[ReminderManager] Test Mode DISABLED (Repeat interval set to 120s).");
        }
    }

    bool ReminderManager::isTestMode() const
    {
        return m_testMode;
    }

    void ReminderManager::setRepeatInterval(uint32_t seconds)
    {
        m_repeatIntervalSec = seconds;
        Serial.printf("[ReminderManager] Repeat Interval set to %u seconds.\n", seconds);
    }

    time_t ReminderManager::getCurrentTime() const
    {
        return time(nullptr) + m_timeOffset;
    }

    void ReminderManager::setTimeOffset(time_t offset)
    {
        m_timeOffset = offset;
        Serial.printf("[ReminderManager] Simulated time offset set to %+ld seconds.\n", (long)offset);
    }

    std::vector<Reminder> ReminderManager::getAllReminders()
    {
        return m_reminders;
    }

    bool ReminderManager::addReminder(const std::string& title, const std::string& description, time_t reminderTime)
    {
        Reminder r;
        r.id = m_reminders.empty() ? 1 : m_reminders.back().id + 1;
        r.title = title;
        r.description = description;
        r.comments = description; // compatibility
        r.createdAt = getCurrentTime();
        r.reminderTime = reminderTime;
        r.status = ReminderStatus::PENDING;
        
        m_reminders.push_back(r);
        saveReminders();
        Serial.printf("[ReminderManager] State Change: Reminder created (ID %u, Title: '%s', Due: %ld)\n", r.id, r.title.c_str(), (long)r.reminderTime);
        return true;
    }

    bool ReminderManager::snoozeReminder(uint32_t id, uint32_t minutes)
    {
        for (auto& r : m_reminders)
        {
            if (r.id == id)
            {
                time_t current = getCurrentTime();
                r.status = ReminderStatus::SNOOZED;
                r.snoozeUntil = current + minutes * 60;
                saveReminders();
                Serial.printf("[ReminderManager] State Change: Reminder snoozed (ID %u, Title: '%s', Snoozed for %u min until %ld)\n", r.id, r.title.c_str(), minutes, (long)r.snoozeUntil);
                notifyBackendStateChange(r);
                return true;
            }
        }
        return false;
    }

    bool ReminderManager::rescheduleReminder(uint32_t id, time_t newTime)
    {
        for (auto& r : m_reminders)
        {
            if (r.id == id)
            {
                r.status = ReminderStatus::PENDING;
                r.reminderTime = newTime;
                r.snoozeUntil = 0;
                saveReminders();
                Serial.printf("[ReminderManager] State Change: Reminder rescheduled (ID %u, Title: '%s', New time: %ld)\n", r.id, r.title.c_str(), (long)r.reminderTime);
                notifyBackendStateChange(r);
                return true;
            }
        }
        return false;
    }

    bool ReminderManager::isDismissed(const std::string& backendId, const std::string& title, uint32_t id) const
    {
        if (!backendId.empty())
        {
            for (const auto& bId : m_dismissedBackendIds)
            {
                if (bId == backendId) return true;
            }
        }
        if (!title.empty())
        {
            for (const auto& t : m_dismissedTitles)
            {
                if (t == title) return true;
            }
        }
        if (id > 0)
        {
            for (uint32_t lId : m_dismissedLocalIds)
            {
                if (lId == id) return true;
            }
        }
        return false;
    }

    bool ReminderManager::dismissReminder(uint32_t id)
    {
        for (auto it = m_reminders.begin(); it != m_reminders.end(); ++it)
        {
            if (it->id == id)
            {
                Reminder copy = *it;
                copy.status = ReminderStatus::COMPLETED;
                copy.completedAt = getCurrentTime();
                copy.completed = true;

                // Track dismissal permanently so it never reappears on screen
                m_dismissedLocalIds.push_back(copy.id);
                if (!copy.backendId.empty())
                {
                    m_dismissedBackendIds.push_back(copy.backendId);
                }
                if (!copy.title.empty())
                {
                    m_dismissedTitles.push_back(copy.title);
                }

                Serial.printf("[ReminderManager] State Change: Reminder dismissed and permanently purged (ID %u, Title: '%s')\n", copy.id, copy.title.c_str());
                m_reminders.erase(it);
                saveReminders();

                // Also purge from DataService in-RAM cache immediately
                dataService.removeReminderLocal(copy.id);

                // Stop audio immediately
                AudioManager::instance().stopReminderMusic();

                // Fire async background task with 0ms UI delay!
                notifyBackendStateChange(copy);
                return true;
            }
        }
        return false;
    }

    bool ReminderManager::deleteReminder(uint32_t id)
    {
        for (auto it = m_reminders.begin(); it != m_reminders.end(); ++it)
        {
            if (it->id == id)
            {
                Serial.printf("[ReminderManager] State Change: Reminder deleted (ID %u, Title: '%s')\n", it->id, it->title.c_str());
                m_reminders.erase(it);
                saveReminders();
                return true;
            }
        }
        return false;
    }

    bool ReminderManager::hasActiveReminder() const
    {
        for (const auto& r : m_reminders)
        {
            if (r.status == ReminderStatus::ACTIVE)
            {
                if (!isDismissed(r.backendId, r.title, r.id))
                {
                    return true;
                }
            }
        }
        return false;
    }

    void ReminderManager::loadReminders()
    {
        m_reminders = ReminderStorage::loadAll();
    }

    void ReminderManager::saveReminders()
    {
        ReminderStorage::saveAll(m_reminders);
    }

    void ReminderManager::checkAndShowPopup(LovyanGFX& canvas)
    {
        if (hasActiveReminder())
        {
            showActiveReminderPopup(canvas);
        }
    }

    void ReminderManager::showActiveReminderPopup(LovyanGFX& /*canvas*/)
    {
        // Find earliest active reminder that is NOT dismissed
        Reminder* activeRem = nullptr;
        for (auto& r : m_reminders)
        {
            if (r.status == ReminderStatus::ACTIVE)
            {
                if (isDismissed(r.backendId, r.title, r.id))
                {
                    r.status = ReminderStatus::COMPLETED;
                    continue;
                }
                if (!activeRem || r.reminderTime < activeRem->reminderTime)
                {
                    activeRem = &r;
                }
            }
        }

        if (!activeRem) return;

        uint32_t activeRemId = activeRem->id;
        Reminder activeRemCopy = *activeRem;

        Serial.printf("[ReminderManager] State Change: Reminder activated (ID %u, Title: '%s')\n", activeRemCopy.id, activeRemCopy.title.c_str());

        // Allocate local double buffer sprite
        uint16_t w = Display::width();
        uint16_t h = Display::height();
        
        LGFX_Sprite popupSprite(&Display::lcd);
        popupSprite.setPsram(true);
        popupSprite.setColorDepth(16);
        popupSprite.createSprite(w, h);

        enum class SubMenu { Main, Snooze, Reschedule };
        SubMenu menu = SubMenu::Main;
        
        int pressedBtn = -1;
        bool wasTouched = false;
        bool popupOpen = true;

        // Dynamic iOS Card Dimensions — adapts to both Portrait (240x320) & Landscape (320x240)
        bool isPortrait = (w < h);
        float cardW = isPortrait ? (w * 0.92f) : (w * 0.90f);
        float cardH = isPortrait ? 276.0f : 210.0f;
        float cardX = (w - cardW) * 0.5f;
        float cardY = (h - cardH) * 0.5f;

        // Play reminder audio
        AudioManager::instance().playReminderMusicAsync();

        while (popupOpen)
        {
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            if (touched)
            {
                if (!wasTouched)
                {
                    wasTouched = true;
                    pressedBtn = -1;
                    
                    if (menu == SubMenu::Main)
                    {
                        if (isPortrait)
                        {
                            // 3 Vertical iOS Action Pills: Dismiss (0), Snooze (1), Reschedule (2)
                            float btnW = cardW * 0.88f;
                            float btnH = 34.0f;
                            float btnX = cardX + (cardW - btnW) * 0.5f;
                            float spacing = 42.0f;
                            float startBtnY = cardY + 138.0f;

                            for (int i = 0; i < 3; ++i)
                            {
                                float btnY = startBtnY + i * spacing;
                                if (tx >= btnX && tx <= btnX + btnW && ty >= btnY - 4.0f && ty <= btnY + btnH + 4.0f)
                                {
                                    pressedBtn = i;
                                }
                            }
                        }
                        else
                        {
                            // 3 Horizontal iOS Action Pills: Dismiss (0), Snooze (1), Reschedule (2)
                            float btnW = (cardW - 40.0f) / 3.0f;
                            float btnH = 34.0f;
                            float btnY = cardY + cardH - 46.0f;

                            for (int i = 0; i < 3; ++i)
                            {
                                float btnX = cardX + 14.0f + i * (btnW + 6.0f);
                                if (tx >= btnX && tx <= btnX + btnW && ty >= btnY - 4.0f && ty <= btnY + btnH + 4.0f)
                                {
                                    pressedBtn = i;
                                }
                            }
                        }
                    }
                    else if (menu == SubMenu::Snooze)
                    {
                        // 5 Options: 5m (0), 15m (1), 1h (2), Tomorrow (3), Cancel (4)
                        if (isPortrait)
                        {
                            float btnW = cardW * 0.88f;
                            float btnH = 30.0f;
                            float btnX = cardX + (cardW - btnW) * 0.5f;
                            float spacing = 38.0f;
                            float startY = cardY + 52.0f;

                            for (int i = 0; i < 5; ++i)
                            {
                                float btnY = startY + i * spacing;
                                if (tx >= btnX && tx <= btnX + btnW && ty >= btnY - 4.0f && ty <= btnY + btnH + 4.0f)
                                {
                                    pressedBtn = i;
                                }
                            }
                        }
                        else
                        {
                            // Landscape: 2x2 grid for times + full width Cancel
                            float halfW = (cardW - 36.0f) * 0.5f;
                            float btnH = 30.0f;
                            float startY = cardY + 44.0f;

                            for (int i = 0; i < 4; ++i)
                            {
                                float colX = (i % 2 == 0) ? (cardX + 14.0f) : (cardX + 22.0f + halfW);
                                float rowY = startY + (i / 2) * 38.0f;
                                if (tx >= colX && tx <= colX + halfW && ty >= rowY - 4.0f && ty <= rowY + btnH + 4.0f)
                                {
                                    pressedBtn = i;
                                }
                            }
                            float cancelY = startY + 2 * 38.0f + 4.0f;
                            float cancelW = cardW - 28.0f;
                            if (tx >= cardX + 14.0f && tx <= cardX + 14.0f + cancelW && ty >= cancelY - 4.0f && ty <= cancelY + btnH + 4.0f)
                            {
                                pressedBtn = 4;
                            }
                        }
                    }
                    else if (menu == SubMenu::Reschedule)
                    {
                        // 4 Options: Today +2h (0), Tomorrow +24h (1), Next Week +7d (2), Cancel (3)
                        float btnW = cardW * 0.88f;
                        float btnH = 30.0f;
                        float btnX = cardX + (cardW - btnW) * 0.5f;
                        float spacing = isPortrait ? 40.0f : 32.0f;
                        float startY = cardY + (isPortrait ? 58.0f : 42.0f);

                        for (int i = 0; i < 4; ++i)
                        {
                            float btnY = startY + i * spacing;
                            if (tx >= btnX && tx <= btnX + btnW && ty >= btnY - 4.0f && ty <= btnY + btnH + 4.0f)
                            {
                                pressedBtn = i;
                            }
                        }
                    }
                }
            }
            else
            {
                if (wasTouched)
                {
                    wasTouched = false;
                    int act = pressedBtn;
                    pressedBtn = -1;

                    if (act != -1)
                    {
                        if (menu == SubMenu::Main)
                        {
                            if (act == 0) // Dismiss — INSTANT CLOSING & PURGING
                            {
                                dismissReminder(activeRemId);
                                popupOpen = false;
                                break;
                            }
                            else if (act == 1) // Snooze
                            {
                                menu = SubMenu::Snooze;
                            }
                            else if (act == 2) // Reschedule
                            {
                                menu = SubMenu::Reschedule;
                            }
                        }
                        else if (menu == SubMenu::Snooze)
                        {
                            if (act == 4) // Cancel
                            {
                                menu = SubMenu::Main;
                            }
                            else
                            {
                                uint32_t mins = 5;
                                if (act == 1) mins = 15;
                                else if (act == 2) mins = 60;
                                else if (act == 3)
                                {
                                    time_t current = getCurrentTime();
                                    struct tm t;
                                    localtime_r(&current, &t);
                                    t.tm_hour = 9;
                                    t.tm_min = 0;
                                    t.tm_sec = 0;
                                    time_t tomorrow9 = mktime(&t) + 24 * 3600;
                                    mins = (tomorrow9 > current) ? (tomorrow9 - current) / 60 : 1440;
                                }
                                snoozeReminder(activeRemId, mins);
                                popupOpen = false;
                                break;
                            }
                        }
                        else if (menu == SubMenu::Reschedule)
                        {
                            if (act == 3) // Cancel
                            {
                                menu = SubMenu::Main;
                            }
                            else
                            {
                                time_t target = getCurrentTime();
                                if (act == 0) target += 2 * 3600;
                                else if (act == 1) target += 24 * 3600;
                                else if (act == 2) target += 7 * 24 * 3600;

                                rescheduleReminder(activeRemId, target);
                                popupOpen = false;
                                break;
                            }
                        }
                    }
                }
            }

            // Dark frosted backdrop
            popupSprite.fillRect(0, 0, w, h, popupSprite.color565(6, 7, 12));

            // Ambient refraction aura
            popupSprite.fillCircle((int)(cardX + cardW * 0.5f), (int)cardY, 60, popupSprite.color565(36, 18, 8));

            // Draw iOS 26 Liquid Glass Card
            ScreenCommon::drawGlassCard(popupSprite, cardX, cardY, cardW, cardH, 20.0f, false, 0);

            if (menu == SubMenu::Main)
            {
                drawAlertPopup(popupSprite, activeRemCopy, pressedBtn, cardX, cardY, cardW, cardH);
            }
            else if (menu == SubMenu::Snooze)
            {
                drawSnoozeMenu(popupSprite, activeRemCopy, pressedBtn, cardX, cardY, cardW, cardH);
            }
            else if (menu == SubMenu::Reschedule)
            {
                drawRescheduleMenu(popupSprite, activeRemCopy, pressedBtn, cardX, cardY, cardW, cardH);
            }

            popupSprite.pushSprite(0, 0);
            delay(16); // ~60fps
        }

        // Always stop reminder audio when modal exits
        AudioManager::instance().stopReminderMusic();

        popupSprite.deleteSprite();
    }

    void ReminderManager::drawAlertPopup(LovyanGFX& canvas, const Reminder& r, int pressedBtn, float cardX, float cardY, float cardW, float cardH)
    {
        float cx = cardX + cardW * 0.5f;
        bool isPortrait = (canvas.width() < canvas.height());

        // 1. iOS Glowing Alarm Bell Squircle (34x34px, radius 10px)
        float bellY = cardY + (isPortrait ? 20.0f : 14.0f);
        canvas.fillRoundRect((int)(cx - 17.0f), (int)bellY, 34, 34, 10, VoxaTheme::getSystemAmber());
        canvas.drawRoundRect((int)(cx - 17.0f), (int)bellY, 34, 34, 10, VoxaTheme::getGlassHighlight());
        canvas.drawFastHLine((int)(cx - 11.0f), (int)bellY + 1, 22, 0xFFFF);
        ScreenCommon::drawIcon(canvas, Icon::Bell, cx - 9.0f, bellY + 8.0f, 18.0f, 0xFFFF);

        // 2. Title in Bold Apple Typography
        canvas.setFont(&fonts::FreeSansBold12pt7b);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        std::string drawTitle = r.title;
        if (drawTitle.length() > 18) drawTitle = drawTitle.substr(0, 16) + "...";
        float titleY = bellY + (isPortrait ? 44.0f : 36.0f);
        canvas.drawString(drawTitle.c_str(), cx, titleY);

        // 3. Subtitle / Description
        canvas.setFont(&fonts::FreeSans9pt7b);
        canvas.setTextColor(VoxaTheme::getTextSecondary());
        std::string desc = r.description.empty() ? r.comments : r.description;
        if (desc.empty()) desc = "Reminder Alert";
        if (desc.length() > 24) desc = desc.substr(0, 21) + "...";
        float descY = titleY + (isPortrait ? 20.0f : 16.0f);
        canvas.drawString(desc.c_str(), cx, descY);

        // 4. iOS Due Status Capsule Badge
        time_t now = getCurrentTime();
        char diffBuf[48];
        bool isOverdue = false;
        if (r.reminderTime > now)
        {
            long futSec = (long)(r.reminderTime - now);
            if (futSec < 60) snprintf(diffBuf, sizeof(diffBuf), "Due now");
            else snprintf(diffBuf, sizeof(diffBuf), "Due in %ldm", futSec / 60);
        }
        else
        {
            long pastSec = (long)(now - r.reminderTime);
            if (pastSec < 60) snprintf(diffBuf, sizeof(diffBuf), "Due now");
            else { snprintf(diffBuf, sizeof(diffBuf), "Overdue (%ldm)", pastSec / 60); isOverdue = true; }
        }

        float badgeY = descY + (isPortrait ? 20.0f : 16.0f);
        int textW = canvas.textWidth(diffBuf);
        int badgeW = textW + 24;
        uint16_t badgeCol = isOverdue ? VoxaTheme::getSystemRed() : VoxaTheme::getSystemAmber();
        canvas.fillRoundRect((int)(cx - badgeW * 0.5f), (int)(badgeY - 8.0f), badgeW, 16, 8, canvas.color565(20, 12, 16));
        canvas.drawRoundRect((int)(cx - badgeW * 0.5f), (int)(badgeY - 8.0f), badgeW, 16, 8, badgeCol);
        canvas.fillCircle((int)(cx - badgeW * 0.5f + 8.0f), (int)badgeY, 3, badgeCol);
        canvas.setFont(&fonts::Font0);
        canvas.setTextColor(0xFFFF);
        canvas.drawString(diffBuf, cx + 5.0f, badgeY);

        // 5. iOS 26 Action Buttons: Dismiss (Crimson), Snooze (Blue), Reschedule (Frosted Glass)
        const char* labels[3] = {"Dismiss", "Snooze", "Reschedule"};
        uint16_t bgColors[3] = {VoxaTheme::getSystemRed(), VoxaTheme::getSystemBlue(), VoxaTheme::getGlassSurface()};
        uint16_t textColors[3] = {0xFFFF, 0xFFFF, VoxaTheme::getTextPrimary()};

        if (isPortrait)
        {
            // 3 Vertical Action Pills
            float btnW = cardW * 0.88f;
            float btnH = 34.0f;
            float btnX = cardX + (cardW - btnW) * 0.5f;
            float spacing = 42.0f;
            float startBtnY = cardY + 138.0f;

            for (int i = 0; i < 3; ++i)
            {
                float btnY = startBtnY + i * spacing;
                bool isPressed = (pressedBtn == i);
                uint16_t fill = isPressed ? 0xFFFF : bgColors[i];
                uint16_t textCol = isPressed ? VoxaTheme::getBackground() : textColors[i];

                canvas.fillRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 17, fill);
                canvas.drawRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 17, isPressed ? 0xFFFF : VoxaTheme::getGlassBorder());
                if (!isPressed)
                {
                    canvas.drawFastHLine((int)btnX + 17, (int)btnY + 1, (int)btnW - 34, VoxaTheme::getGlassHighlight());
                }

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextColor(textCol);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.drawString(labels[i], cx, btnY + btnH * 0.5f);
            }
        }
        else
        {
            // 3 Horizontal Action Pills
            float btnW = (cardW - 40.0f) / 3.0f;
            float btnH = 34.0f;
            float btnY = cardY + cardH - 46.0f;

            for (int i = 0; i < 3; ++i)
            {
                float btnX = cardX + 14.0f + i * (btnW + 6.0f);
                bool isPressed = (pressedBtn == i);
                uint16_t fill = isPressed ? 0xFFFF : bgColors[i];
                uint16_t textCol = isPressed ? VoxaTheme::getBackground() : textColors[i];

                canvas.fillRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 17, fill);
                canvas.drawRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 17, isPressed ? 0xFFFF : VoxaTheme::getGlassBorder());
                if (!isPressed)
                {
                    canvas.drawFastHLine((int)btnX + 12, (int)btnY + 1, (int)btnW - 24, VoxaTheme::getGlassHighlight());
                }

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextColor(textCol);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.drawString(labels[i], btnX + btnW * 0.5f, btnY + btnH * 0.5f);
            }
        }
    }

    void ReminderManager::drawSnoozeMenu(LovyanGFX& canvas, const Reminder& /*r*/, int pressedBtn, float cardX, float cardY, float cardW, float cardH)
    {
        float cx = cardX + cardW * 0.5f;
        bool isPortrait = (canvas.width() < canvas.height());

        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextDatum(textdatum_t::top_center);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        float titleY = cardY + (isPortrait ? 18.0f : 12.0f);
        canvas.drawString("Snooze Options", cx, titleY);
        canvas.drawFastHLine((int)cardX + 24, (int)titleY + 20, (int)cardW - 48, VoxaTheme::getGlassBorder());

        const char* labels[5] = {
            "5 Minutes",
            "15 Minutes",
            "1 Hour",
            "Tomorrow Morning (9 AM)",
            "Cancel"
        };

        if (isPortrait)
        {
            float btnW = cardW * 0.88f;
            float btnH = 30.0f;
            float btnX = cardX + (cardW - btnW) * 0.5f;
            float spacing = 38.0f;
            float startY = cardY + 52.0f;

            for (int i = 0; i < 5; ++i)
            {
                float btnY = startY + i * spacing;
                bool isPressed = (pressedBtn == i);
                bool isCancel = (i == 4);

                uint16_t fill = isPressed 
                    ? (isCancel ? 0xFFFF : VoxaTheme::getPrimary()) 
                    : (isCancel ? (VoxaTheme::isDarkMode() ? canvas.color565(20, 22, 32) : 0xFFFF) : VoxaTheme::getGlassSurface());
                uint16_t textCol = isPressed 
                    ? VoxaTheme::getBackground() 
                    : (isCancel ? VoxaTheme::getTextSecondary() : VoxaTheme::getTextPrimary());

                canvas.fillRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 15, fill);
                canvas.drawRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 15, isCancel ? VoxaTheme::getDivider() : VoxaTheme::getGlassBorder());
                if (!isPressed)
                {
                    canvas.drawFastHLine((int)btnX + 15, (int)btnY + 1, (int)btnW - 30, VoxaTheme::getGlassHighlight());
                }

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(textCol);
                canvas.drawString(labels[i], cx, btnY + btnH * 0.5f);
            }
        }
        else
        {
            float halfW = (cardW - 36.0f) * 0.5f;
            float btnH = 30.0f;
            float startY = cardY + 44.0f;

            for (int i = 0; i < 4; ++i)
            {
                float colX = (i % 2 == 0) ? (cardX + 14.0f) : (cardX + 22.0f + halfW);
                float rowY = startY + (i / 2) * 38.0f;
                bool isPressed = (pressedBtn == i);

                uint16_t fill = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getGlassSurface();
                uint16_t textCol = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();

                canvas.fillRoundRect((int)colX, (int)rowY, (int)halfW, (int)btnH, 15, fill);
                canvas.drawRoundRect((int)colX, (int)rowY, (int)halfW, (int)btnH, 15, VoxaTheme::getGlassBorder());
                if (!isPressed)
                {
                    canvas.drawFastHLine((int)colX + 15, (int)rowY + 1, (int)halfW - 30, VoxaTheme::getGlassHighlight());
                }

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(textCol);
                canvas.drawString(labels[i], colX + halfW * 0.5f, rowY + btnH * 0.5f);
            }

            // Cancel button
            float cancelY = startY + 2 * 38.0f + 4.0f;
            float cancelW = cardW - 28.0f;
            bool isPressed = (pressedBtn == 4);
            uint16_t fill = isPressed ? 0xFFFF : (VoxaTheme::isDarkMode() ? canvas.color565(20, 22, 32) : 0xFFFF);
            uint16_t textCol = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextSecondary();

            canvas.fillRoundRect((int)(cardX + 14.0f), (int)cancelY, (int)cancelW, (int)btnH, 15, fill);
            canvas.drawRoundRect((int)(cardX + 14.0f), (int)cancelY, (int)cancelW, (int)btnH, 15, VoxaTheme::getDivider());
            canvas.setFont(&fonts::FreeSans9pt7b);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(textCol);
            canvas.drawString("Cancel", cx, cancelY + btnH * 0.5f);
        }
    }

    void ReminderManager::drawRescheduleMenu(LovyanGFX& canvas, const Reminder& /*r*/, int pressedBtn, float cardX, float cardY, float cardW, float cardH)
    {
        float cx = cardX + cardW * 0.5f;
        bool isPortrait = (canvas.width() < canvas.height());

        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextDatum(textdatum_t::top_center);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        float titleY = cardY + (isPortrait ? 18.0f : 12.0f);
        canvas.drawString("Reschedule Options", cx, titleY);
        canvas.drawFastHLine((int)cardX + 24, (int)titleY + 20, (int)cardW - 48, VoxaTheme::getGlassBorder());

        const char* labels[4] = {
            "Today (+2 Hours)",
            "Tomorrow (+24 Hours)",
            "Next Week (+7 Days)",
            "Cancel"
        };

        float btnW = cardW * 0.88f;
        float btnH = 30.0f;
        float btnX = cardX + (cardW - btnW) * 0.5f;
        float spacing = isPortrait ? 40.0f : 32.0f;
        float startY = cardY + (isPortrait ? 58.0f : 42.0f);

        for (int i = 0; i < 4; ++i)
        {
            float btnY = startY + i * spacing;
            bool isPressed = (pressedBtn == i);
            bool isCancel = (i == 3);

            uint16_t fill = isPressed 
                ? (isCancel ? 0xFFFF : VoxaTheme::getPrimary()) 
                : (isCancel ? (VoxaTheme::isDarkMode() ? canvas.color565(20, 22, 32) : 0xFFFF) : VoxaTheme::getGlassSurface());
            uint16_t textCol = isPressed 
                ? VoxaTheme::getBackground() 
                : (isCancel ? VoxaTheme::getTextSecondary() : VoxaTheme::getTextPrimary());

            canvas.fillRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 15, fill);
            canvas.drawRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 15, isCancel ? VoxaTheme::getDivider() : VoxaTheme::getGlassBorder());
            if (!isPressed)
            {
                canvas.drawFastHLine((int)btnX + 15, (int)btnY + 1, (int)btnW - 30, VoxaTheme::getGlassHighlight());
            }

            canvas.setFont(&fonts::FreeSans9pt7b);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(textCol);
            canvas.drawString(labels[i], cx, btnY + btnH * 0.5f);
        }
    }

    void ReminderManager::runTestScenarios()
    {
        Serial.println("=========================================");
        Serial.println("--- STARTING REMINDER TEST SUITE ---");
        Serial.println("=========================================");
        
        // Backup old reminders list, clear active test workspace
        auto oldReminders = m_reminders;
        m_reminders.clear();
        m_testMode = true;
        m_timeOffset = 0;

        time_t startTime = getCurrentTime();

        // -------------------------------------------------------------
        // TEST 1: Create reminder for 30 seconds. Verify reminder appears.
        // -------------------------------------------------------------
        Serial.println("[TEST 1] Creating reminder for +30s...");
        addReminder("Test Reminder", "This is scenario test 1", startTime + 30);
        
        // Simulate time passing (31 seconds later)
        time_t timeAfterCreate = startTime + 31;
        m_timeOffset = 31; 
        
        // Trigger scheduler update
        ReminderScheduler::update(m_reminders, timeAfterCreate, m_repeatIntervalSec);
        
        if (hasActiveReminder())
        {
            Serial.println("[TEST 1] PASS: Reminder triggered and became ACTIVE!");
        }
        else
        {
            Serial.println("[TEST 1] FAIL: Reminder did not trigger.");
        }

        // Get the active test reminder ID
        uint32_t activeId = m_reminders.back().id;

        // -------------------------------------------------------------
        // TEST 2: Snooze 1 minute. Verify reminder appears again.
        // -------------------------------------------------------------
        Serial.println("[TEST 2] Snoozing active reminder for 1 minute...");
        snoozeReminder(activeId, 1);
        
        // Verify status becomes SNOOZED
        if (m_reminders.back().status == ReminderStatus::SNOOZED)
        {
            Serial.println("[TEST 2] Step 1: Reminder status correctly SNOOZED.");
        }
        else
        {
            Serial.println("[TEST 2] Step 1 FAIL: Status not SNOOZED.");
        }

        // Simulate 61 seconds passing
        m_timeOffset += 61;
        ReminderScheduler::update(m_reminders, getCurrentTime(), m_repeatIntervalSec);
        
        if (hasActiveReminder())
        {
            Serial.println("[TEST 2] PASS: Snooze expired and reminder became ACTIVE again!");
        }
        else
        {
            Serial.println("[TEST 2] FAIL: Snooze did not re-trigger.");
        }

        // -------------------------------------------------------------
        // TEST 3: Reschedule to +2 minutes. Verify new schedule works.
        // -------------------------------------------------------------
        Serial.println("[TEST 3] Rescheduling reminder to +2 minutes...");
        time_t reschedTime = getCurrentTime() + 120;
        rescheduleReminder(activeId, reschedTime);

        if (m_reminders.back().status == ReminderStatus::PENDING)
        {
            Serial.println("[TEST 3] Step 1: Reminder returned to PENDING.");
        }
        else
        {
            Serial.println("[TEST 3] Step 1 FAIL: Status not PENDING.");
        }

        // Simulate 121 seconds passing
        m_timeOffset += 121;
        ReminderScheduler::update(m_reminders, getCurrentTime(), m_repeatIntervalSec);

        if (hasActiveReminder())
        {
            Serial.println("[TEST 3] PASS: Rescheduled time reached, reminder ACTIVE!");
        }
        else
        {
            Serial.println("[TEST 3] FAIL: Reschedule did not trigger ACTIVE.");
        }

        // -------------------------------------------------------------
        // TEST 4: Dismiss reminder. Verify status becomes COMPLETED.
        // -------------------------------------------------------------
        Serial.println("[TEST 4] Dismissing reminder...");
        dismissReminder(activeId);

        if (m_reminders.back().status == ReminderStatus::COMPLETED && m_reminders.back().completedAt > 0)
        {
            Serial.println("[TEST 4] PASS: Reminder status correctly set to COMPLETED.");
        }
        else
        {
            Serial.println("[TEST 4] FAIL: Status not COMPLETED.");
        }

        // -------------------------------------------------------------
        // TEST 5: Simulate time +49 hours. Verify reminder removed.
        // -------------------------------------------------------------
        Serial.println("[TEST 5] Simulating +49 hours time lapse...");
        m_timeOffset += 49 * 3600; // +49 hours

        ReminderScheduler::update(m_reminders, getCurrentTime(), m_repeatIntervalSec);

        if (m_reminders.empty())
        {
            Serial.println("[TEST 5] PASS: Completed reminder automatically purged after 48h limit.");
        }
        else
        {
            Serial.println("[TEST 5] FAIL: Completed reminder was not purged.");
        }

        // -------------------------------------------------------------
        // TEST 6: Create multiple reminders. Ensure earliest reminder displays first.
        // -------------------------------------------------------------
        Serial.println("[TEST 6] Creating multiple reminders with different due times...");
        time_t cur = getCurrentTime();
        addReminder("Late Reminder", "Due in 10 minutes", cur + 600);
        addReminder("Early Reminder", "Due in 5 minutes", cur + 300);

        // Sort check: earliest should display first
        m_timeOffset += 601; // trigger both
        ReminderScheduler::update(m_reminders, getCurrentTime(), m_repeatIntervalSec);

        Reminder* earliest = nullptr;
        for (auto& r : m_reminders)
        {
            if (r.status == ReminderStatus::ACTIVE)
            {
                if (!earliest || r.reminderTime < earliest->reminderTime)
                {
                    earliest = &r;
                }
            }
        }

        if (earliest && earliest->title == "Early Reminder")
        {
            Serial.println("[TEST 6] PASS: Earliest active reminder is correctly prioritized.");
        }
        else
        {
            Serial.println("[TEST 6] FAIL: Earliest reminder was not prioritized.");
        }

        // Restore user reminders and state
        m_reminders = oldReminders;
        m_testMode = false;
        m_timeOffset = 0;
        saveReminders();

        Serial.println("=========================================");
        Serial.println("--- REMINDER TEST SUITE COMPLETED ---");
        Serial.println("=========================================");
    }

    void ReminderManager::fetchActiveReminders()
    {
        if (!wifiManager.isConnected()) return;
        
        Serial.println("[ReminderManager] Fetching active reminders from backend...");
        
        HTTPClient http;
        std::string url = apiClient.getBaseUrl() + "/api/reminders/active";
        http.begin(url.c_str());
        
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (!error && doc["success"] == true)
            {
                JsonArray items = doc["items"].as<JsonArray>();
                for (JsonVariant val : items)
                {
                    Reminder r;
                    r.backendId = val["id"] | "";
                    r.title = val["title"] | "";
                    r.description = val["description"] | "";
                    r.comments = r.description;
                    r.reminderTime = val["reminderTime"] | 0;
                    r.status = ReminderStatus::ACTIVE;
                    r.createdAt = getCurrentTime();

                    // Drop if already dismissed locally
                    if (isDismissed(r.backendId, r.title, 0))
                    {
                        continue;
                    }
                    
                    bool found = false;
                    for (auto& item : m_reminders)
                    {
                        if (!item.backendId.empty() && item.backendId == r.backendId)
                        {
                            item.title = r.title;
                            item.description = r.description;
                            item.comments = r.comments;
                            item.reminderTime = r.reminderTime;
                            item.status = r.status;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        r.id = m_reminders.empty() ? 1 : m_reminders.back().id + 1;
                        m_reminders.push_back(r);
                    }
                }
                saveReminders();
                Serial.println("[ReminderManager] Successfully synced active reminders from backend.");
            }
        }
        http.end();
    }

    void ReminderManager::notifyBackendStateChange(const Reminder& r)
    {
        if (!wifiManager.isConnected()) return;
        
        std::string idOrTitle = !r.backendId.empty() ? r.backendId : r.title;
        if (idOrTitle.empty()) return;

        auto* param = new BackendNotifyParam();
        param->url = apiClient.getBaseUrl() + "/api/reminders/" + idOrTitle;
        param->isJson = false;

        if (r.status == ReminderStatus::COMPLETED)
        {
            param->url += "/dismiss";
        }
        else if (r.status == ReminderStatus::SNOOZED)
        {
            param->url += "/snooze";
            param->isJson = true;
            char body[64];
            time_t diffSec = r.snoozeUntil - getCurrentTime();
            int mins = diffSec > 0 ? (diffSec + 30) / 60 : 5;
            snprintf(body, sizeof(body), "{\"minutes\":%d}", mins);
            param->body = body;
        }
        else if (r.status == ReminderStatus::PENDING)
        {
            param->url += "/reschedule";
            param->isJson = true;
            char body[64];
            snprintf(body, sizeof(body), "{\"reminder_time\":%lld}", (long long)r.reminderTime);
            param->body = body;
        }

        BaseType_t res = xTaskCreate(backendNotifyTaskFn, "remNotifAsync", 4096, param, 1, nullptr);
        if (res != pdPASS)
        {
            delete param;
        }
        Serial.printf("[ReminderManager] Dispatched async backend update for '%s'\n", idOrTitle.c_str());
    }

    void ReminderManager::handleNotification()
    {
        if (!m_httpServer) return;
        
        WebServer* web = static_cast<WebServer*>(m_httpServer);
        String postBody = web->arg("plain");
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, postBody);
        if (error)
        {
            web->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
            return;
        }

        Reminder r;
        r.backendId = doc["id"] | "";
        r.title = doc["title"] | "";
        r.description = doc["description"] | "";
        r.comments = r.description;
        r.reminderTime = doc["reminderTime"] | 0;
        r.status = ReminderStatus::ACTIVE;
        r.createdAt = getCurrentTime();

        // Drop immediately if already dismissed
        if (isDismissed(r.backendId, r.title, 0))
        {
            Serial.printf("[ReminderManager] Ignored notification for already dismissed reminder: '%s'\n", r.title.c_str());
            web->send(200, "application/json", "{\"success\":true,\"dismissed\":true}");
            return;
        }
        
        bool found = false;
        for (auto& item : m_reminders)
        {
            if (!item.backendId.empty() && item.backendId == r.backendId)
            {
                item.title = r.title;
                item.description = r.description;
                item.comments = r.comments;
                item.reminderTime = r.reminderTime;
                item.status = r.status;
                r.id = item.id;
                found = true;
                break;
            }
        }
        if (!found)
        {
            r.id = m_reminders.empty() ? 1 : m_reminders.back().id + 1;
            m_reminders.push_back(r);
        }
        
        saveReminders();
        
        web->send(200, "application/json", "{\"success\":true}");
        Serial.printf("[ReminderManager] Received active reminder notification: '%s'\n", r.title.c_str());
    }
}
