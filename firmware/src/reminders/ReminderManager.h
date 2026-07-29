#pragma once

#include <vector>
#include <string>
#include <ctime>
#include "../models/Reminder.h"
#include "../touch/Touch.h"
#include <LovyanGFX.hpp>

namespace VOXA
{
    class ReminderManager
    {
    public:
        static ReminderManager& instance();

        void begin();
        void tick();
        
        // Configuration & Test Mode
        void setTestMode(bool enabled);
        [[nodiscard]] bool isTestMode() const;
        void setRepeatInterval(uint32_t seconds);

        // Core APIs
        std::vector<Reminder> getAllReminders();
        bool addReminder(const std::string& title, const std::string& description, time_t reminderTime);
        bool snoozeReminder(uint32_t id, uint32_t minutes);
        bool rescheduleReminder(uint32_t id, time_t newTime);
        bool dismissReminder(uint32_t id);
        bool deleteReminder(uint32_t id);

        // Check if any reminder is currently ACTIVE and needs to pop up
        [[nodiscard]] bool hasActiveReminder() const;
        void checkAndShowPopup(LovyanGFX& canvas);

        // Run automated test scenarios
        void runTestScenarios();
        
        // Time simulation
        [[nodiscard]] time_t getCurrentTime() const;
        void setTimeOffset(time_t offset);

        // Backend Integration
        void fetchActiveReminders();
        void notifyBackendStateChange(const Reminder& r);
        void handleNotification();

    private:
        ReminderManager() = default;
        ~ReminderManager() = default;
        ReminderManager(const ReminderManager&) = delete;
        ReminderManager& operator=(const ReminderManager&) = delete;

        std::vector<Reminder> m_reminders;
        bool m_testMode { false };
        uint32_t m_repeatIntervalSec { 120 }; // Default 2 minutes repeat if ignored
        uint32_t m_lastTickMs { 0 };
        time_t m_timeOffset { 0 };
        
        // WebServer & connection state
        void* m_httpServer { nullptr }; // Cast to WebServer* internally to keep header clean
        bool m_wasConnected { false };

        void loadReminders();
        void saveReminders();
        void showActiveReminderPopup(LovyanGFX& canvas);

        // Modal views within the popup
        void drawAlertPopup(LovyanGFX& canvas, const Reminder& r, int pressedBtn, float cardX, float cardY, float cardW, float cardH);
        void drawSnoozeMenu(LovyanGFX& canvas, const Reminder& r, int pressedBtn, float cardX, float cardY, float cardW, float cardH);
        void drawRescheduleMenu(LovyanGFX& canvas, const Reminder& r, int pressedBtn, float cardX, float cardY, float cardW, float cardH);
    };
}
