#include "ReminderManager.h"
#include "ReminderStorage.h"
#include "ReminderScheduler.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../screens/ScreenCommon.h"
#include "../services/WiFiManager.h"
#include "../services/ApiClient.h"
#include "../audio/AudioManager.h"
#include <Arduino.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

extern Touch touch;

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
                notifyBackendStateChange(copy);
                Serial.printf("[ReminderManager] State Change: Reminder dismissed and purged (ID %u, Title: '%s')\n", copy.id, copy.title.c_str());
                m_reminders.erase(it);
                saveReminders();
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
                return true;
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
        // We find the earliest active reminder to display first
        Reminder* activeRem = nullptr;
        for (auto& r : m_reminders)
        {
            if (r.status == ReminderStatus::ACTIVE)
            {
                if (!activeRem || r.reminderTime < activeRem->reminderTime)
                {
                    activeRem = &r;
                }
            }
        }

        if (!activeRem) return;

        Serial.printf("[ReminderManager] State Change: Reminder activated (ID %u, Title: '%s')\n", activeRem->id, activeRem->title.c_str());

        // Allocate local double buffer sprite to render on top of whatever screen we are on
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

        float cardW = w * 0.88f;
        float cardH = h * 0.74f;
        float cardX = (w - cardW) * 0.5f;
        float cardY = (h - cardH) * 0.5f;

        // Play the configured reminder music (reminder.mp3) while the alert popup is active
        AudioManager::instance().playReminderMusicAsync();

        while (activeRem->status == ReminderStatus::ACTIVE)
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
                        // 3 Buttons vertically stacked on the card
                        // Dismiss button: cardY + cardH - 120
                        // Snooze button: cardY + cardH - 80
                        // Reschedule button: cardY + cardH - 40
                        float btnW = cardW * 0.82f;
                        float btnH = 28.0f;
                        float btnX = cardX + (cardW - btnW) * 0.5f;

                        for (int i = 0; i < 3; ++i)
                        {
                            float btnY = cardY + cardH - 110.0f + i * 34.0f;
                            if (tx >= btnX && tx <= btnX + btnW && ty >= btnY && ty <= btnY + btnH)
                            {
                                pressedBtn = i;
                            }
                        }
                    }
                    else if (menu == SubMenu::Snooze)
                    {
                        // Snooze Menu buttons
                        float btnW = cardW * 0.82f;
                        float btnH = 22.0f;
                        float btnX = cardX + (cardW - btnW) * 0.5f;
                        float startY = cardY + 40.0f;

                        // 8 options including Back
                        for (int i = 0; i < 8; ++i)
                        {
                            float btnY = startY + i * 24.0f;
                            if (tx >= btnX && tx <= btnX + btnW && ty >= btnY && ty <= btnY + btnH)
                            {
                                pressedBtn = i;
                            }
                        }
                    }
                    else if (menu == SubMenu::Reschedule)
                    {
                        // Reschedule Menu buttons
                        float btnW = cardW * 0.82f;
                        float btnH = 22.0f;
                        float btnX = cardX + (cardW - btnW) * 0.5f;
                        float startY = cardY + 42.0f;

                        // 7 options including Back
                        for (int i = 0; i < 7; ++i)
                        {
                            float btnY = startY + i * 24.0f;
                            if (tx >= btnX && tx <= btnX + btnW && ty >= btnY && ty <= btnY + btnH)
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
                            if (act == 0) // Dismiss
                            {
                                dismissReminder(activeRem->id);
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
                            // 0: 5 min, 1: 10 min, 2: 15 min, 3: 30 min, 4: 1 hour, 5: Tomorrow, 6: Custom (+2h), 7: Back
                            if (act == 7) // Back
                            {
                                menu = SubMenu::Main;
                            }
                            else
                            {
                                uint32_t mins = 5;
                                if (act == 1) mins = 10;
                                else if (act == 2) mins = 15;
                                else if (act == 3) mins = 30;
                                else if (act == 4) mins = 60;
                                else if (act == 5)
                                {
                                    // Tomorrow morning at 9:00 AM
                                    time_t current = getCurrentTime();
                                    struct tm t;
                                    localtime_r(&current, &t);
                                    t.tm_hour = 9;
                                    t.tm_min = 0;
                                    t.tm_sec = 0;
                                    time_t tomorrow9 = mktime(&t) + 24 * 3600;
                                    mins = (tomorrow9 > current) ? (tomorrow9 - current) / 60 : 1440;
                                }
                                else if (act == 6) mins = 120; // +2 hours
                                
                                // Test mode durations override
                                if (m_testMode)
                                {
                                    if (act == 0) snoozeReminder(activeRem->id, 1); // 1 min snooze
                                    else snoozeReminder(activeRem->id, 2); // 2 mins snooze
                                }
                                else
                                {
                                    snoozeReminder(activeRem->id, mins);
                                }
                            }
                        }
                        else if (menu == SubMenu::Reschedule)
                        {
                            // 0: Today (+2h), 1: Tomorrow (+24h), 2: Next week (+7d), 3: Specific (+10m), 4: Specific (+30m), 5: Specific (+1h), 6: Back
                            if (act == 6) // Back
                            {
                                menu = SubMenu::Main;
                            }
                            else
                            {
                                time_t target = getCurrentTime();
                                if (act == 0) target += 2 * 3600;
                                else if (act == 1) target += 24 * 3600;
                                else if (act == 2) target += 7 * 24 * 3600;
                                else if (act == 3) target += 10 * 60;
                                else if (act == 4) target += 30 * 60;
                                else if (act == 5) target += 3600;

                                rescheduleReminder(activeRem->id, target);
                            }
                        }
                    }
                }
            }

            // Draw darkened background behind modal
            popupSprite.fillRect(0, 0, w, h, popupSprite.color565(12, 10, 20));

            // Draw premium modal container card
            popupSprite.fillRoundRect((int)cardX, (int)cardY, (int)cardW, (int)cardH, 16, VoxaTheme::getSurface());
            popupSprite.drawRoundRect((int)cardX, (int)cardY, (int)cardW, (int)cardH, 16, VoxaTheme::getPrimaryLight());

            if (menu == SubMenu::Main)
            {
                drawAlertPopup(popupSprite, *activeRem, pressedBtn, cardX, cardY, cardW, cardH);
            }
            else if (menu == SubMenu::Snooze)
            {
                drawSnoozeMenu(popupSprite, *activeRem, pressedBtn, cardX, cardY, cardW, cardH);
            }
            else if (menu == SubMenu::Reschedule)
            {
                drawRescheduleMenu(popupSprite, *activeRem, pressedBtn, cardX, cardY, cardW, cardH);
            }

            popupSprite.pushSprite(0, 0);
            delay(16); // ~60fps
        }

        // Stop reminder music when dismissed, snoozed, or rescheduled
        AudioManager::instance().stopReminderMusic();

        popupSprite.deleteSprite();
    }

    void ReminderManager::drawAlertPopup(LovyanGFX& canvas, const Reminder& r, int pressedBtn, float cardX, float cardY, float cardW, float cardH)
    {
        float cx = cardX + cardW * 0.5f;
        
        // 1. Draw Alert Bell Icon
        canvas.fillCircle((int)cx, (int)(cardY + 36.0f), 16, 0xFD20); // Amber alert circle
        ScreenCommon::drawIcon(canvas, Icon::Bell, cx - 8.0f, cardY + 36.0f - 8.0f, 16.0f, VoxaTheme::getBackground());

        // 2. Draw Title
        canvas.setFont(&fonts::FreeSansBold12pt7b);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(TFT_WHITE);
        std::string drawTitle = r.title;
        if (drawTitle.length() > 16) drawTitle = drawTitle.substr(0, 14) + "...";
        canvas.drawString(drawTitle.c_str(), cx, cardY + 68.0f);

        // 3. Draw Subtitle (Description / comments)
        canvas.setFont(&fonts::FreeSans9pt7b);
        canvas.setTextColor(VoxaTheme::getTextSecondary());
        std::string desc = r.description.empty() ? r.comments : r.description;
        if (desc.empty()) desc = "No details";
        if (desc.length() > 22) desc = desc.substr(0, 19) + "...";
        canvas.drawString(desc.c_str(), cx, cardY + 90.0f);

        // 4. Draw Time Difference
        time_t now = getCurrentTime();
        char diffBuf[64];
        if (r.reminderTime > now)
        {
            long futSec = (long)(r.reminderTime - now);
            if (futSec < 60)
            {
                snprintf(diffBuf, sizeof(diffBuf), "Due now");
            }
            else if (futSec < 3600)
            {
                snprintf(diffBuf, sizeof(diffBuf), "Due in %ldm", futSec / 60);
            }
            else
            {
                long hrs = futSec / 3600;
                long mins = (futSec % 3600) / 60;
                snprintf(diffBuf, sizeof(diffBuf), "Due in %ldh %ldm", hrs, mins);
            }
        }
        else
        {
            long pastSec = (long)(now - r.reminderTime);
            if (pastSec < 60)
            {
                snprintf(diffBuf, sizeof(diffBuf), "Due now");
            }
            else if (pastSec < 3600)
            {
                snprintf(diffBuf, sizeof(diffBuf), "Overdue by %ldm", pastSec / 60);
            }
            else
            {
                long hrs = pastSec / 3600;
                long mins = (pastSec % 3600) / 60;
                snprintf(diffBuf, sizeof(diffBuf), "Overdue by %ldh %ldm", hrs, mins);
            }
        }
        canvas.setTextColor(VoxaTheme::getWarning());
        canvas.drawString(diffBuf, cx, cardY + 110.0f);

        // 5. Draw 3 stacked buttons: Dismiss, Snooze, Reschedule
        float btnW = cardW * 0.82f;
        float btnH = 28.0f;
        float btnX = cardX + (cardW - btnW) * 0.5f;

        const char* labels[3] = {"Dismiss", "Snooze", "Reschedule"};
        uint16_t colors[3] = {0x3E8F, 0x2A9A, 0x8410}; // Green, Blue, Dark Gray

        for (int i = 0; i < 3; ++i)
        {
            float btnY = cardY + cardH - 110.0f + i * 34.0f;
            bool isPressed = (pressedBtn == i);
            uint16_t btnBg = isPressed ? VoxaTheme::getPrimary() : colors[i];
            uint16_t btnTextCol = isPressed ? VoxaTheme::getBackground() : TFT_WHITE;

            canvas.fillRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 6, btnBg);
            canvas.drawRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 6, VoxaTheme::getDivider());
            canvas.setTextColor(btnTextCol);
            canvas.setFont(&fonts::FreeSans9pt7b);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.drawString(labels[i], btnX + btnW * 0.5f, btnY + btnH * 0.5f);
        }
    }

    void ReminderManager::drawSnoozeMenu(LovyanGFX& canvas, const Reminder& /*r*/, int pressedBtn, float cardX, float cardY, float cardW, float cardH)
    {
        float cx = cardX + cardW * 0.5f;
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextDatum(textdatum_t::top_center);
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString("Snooze Options", cx, cardY + 10.0f);

        // 8 buttons: 5m, 10m, 15m, 30m, 1h, Tomorrow morning, Custom, Back
        float btnW = cardW * 0.82f;
        float btnH = 22.0f;
        float btnX = cardX + (cardW - btnW) * 0.5f;
        float startY = cardY + 40.0f;

        const char* labels[8] = {
            "5 Minutes",
            "10 Minutes",
            "15 Minutes",
            "30 Minutes",
            "1 Hour",
            "Tomorrow Morning (9am)",
            "Custom (+2 Hours)",
            "<- Back"
        };

        const char* testLabels[8] = {
            "30 Seconds",
            "1 Minute",
            "2 Minutes",
            "5 Minutes",
            "10 Minutes",
            "1 Hour",
            "Custom (+2 Hours)",
            "<- Back"
        };

        for (int i = 0; i < 8; ++i)
        {
            float btnY = startY + i * 24.0f;
            bool isPressed = (pressedBtn == i);
            uint16_t btnBg = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t textCol = isPressed ? VoxaTheme::getBackground() : (i == 7 ? VoxaTheme::getPrimaryLight() : VoxaTheme::getTextPrimary());

            canvas.fillRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 4, btnBg);
            canvas.drawRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 4, VoxaTheme::getDivider());
            
            canvas.setFont(&fonts::FreeSans9pt7b);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(textCol);
            canvas.drawString(m_testMode ? testLabels[i] : labels[i], btnX + btnW * 0.5f, btnY + btnH * 0.5f);
        }
    }

    void ReminderManager::drawRescheduleMenu(LovyanGFX& canvas, const Reminder& /*r*/, int pressedBtn, float cardX, float cardY, float cardW, float cardH)
    {
        float cx = cardX + cardW * 0.5f;
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextDatum(textdatum_t::top_center);
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString("Reschedule Options", cx, cardY + 12.0f);

        // 7 buttons: Today, Tomorrow, Next Week, Specific (+10m), Specific (+30m), Specific (+1h), Back
        float btnW = cardW * 0.82f;
        float btnH = 22.0f;
        float btnX = cardX + (cardW - btnW) * 0.5f;
        float startY = cardY + 42.0f;

        const char* labels[7] = {
            "Later Today (+2 Hours)",
            "Tomorrow (+24 Hours)",
            "Next Week (+7 Days)",
            "Specific (+10 Minutes)",
            "Specific (+30 Minutes)",
            "Specific (+1 Hour)",
            "<- Back"
        };

        for (int i = 0; i < 7; ++i)
        {
            float btnY = startY + i * 24.0f;
            bool isPressed = (pressedBtn == i);
            uint16_t btnBg = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t textCol = isPressed ? VoxaTheme::getBackground() : (i == 6 ? VoxaTheme::getPrimaryLight() : VoxaTheme::getTextPrimary());

            canvas.fillRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 4, btnBg);
            canvas.drawRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 4, VoxaTheme::getDivider());
            
            canvas.setFont(&fonts::FreeSans9pt7b);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(textCol);
            canvas.drawString(labels[i], btnX + btnW * 0.5f, btnY + btnH * 0.5f);
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
        if (r.backendId.empty() || !wifiManager.isConnected()) return;
        
        HTTPClient http;
        std::string url = apiClient.getBaseUrl() + "/api/reminders/" + r.backendId;
        if (r.status == ReminderStatus::COMPLETED)
        {
            url += "/dismiss";
            http.begin(url.c_str());
            http.POST("");
        }
        else if (r.status == ReminderStatus::SNOOZED)
        {
            url += "/snooze";
            http.begin(url.c_str());
            http.addHeader("Content-Type", "application/json");
            char body[64];
            time_t diffSec = r.snoozeUntil - getCurrentTime();
            int mins = diffSec > 0 ? (diffSec + 30) / 60 : 5;
            snprintf(body, sizeof(body), "{\"minutes\":%d}", mins);
            http.POST(body);
        }
        else if (r.status == ReminderStatus::PENDING)
        {
            url += "/reschedule";
            http.begin(url.c_str());
            http.addHeader("Content-Type", "application/json");
            char body[64];
            snprintf(body, sizeof(body), "{\"reminder_time\":%lld}", (long long)r.reminderTime);
            http.POST(body);
        }
        http.end();
        Serial.printf("[ReminderManager] Notified backend of reminder state change for ID %s\n", r.backendId.c_str());
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
