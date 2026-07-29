#pragma once

#include <cstdint>
#include <string>
#include <ctime>

namespace VOXA
{
    enum class ReminderStatus
    {
        PENDING,
        ACTIVE,
        SNOOZED,
        COMPLETED,
        EXPIRED
    };

    /// A single reminder entry stored by the user.
    struct Reminder
    {
        uint32_t    id        { 0 };
        std::string title;
        std::string dateTime;       ///< ISO-style string, e.g. "2026-07-01 20:00"
        bool        completed { false };
        std::string comments;       ///< Delimited comments
        bool        pinned    { false };

        // Advanced reminder system fields
        std::string backendId;
        std::string description;
        time_t      createdAt { 0 };
        time_t      reminderTime { 0 };
        ReminderStatus status { ReminderStatus::PENDING };
        time_t      lastTriggeredTime { 0 };
        time_t      snoozeUntil { 0 };
        time_t      completedAt { 0 };

        /// Returns true if this reminder has been set and has a valid id.
        [[nodiscard]] bool isValid() const { return id != 0 && !title.empty(); }
    };
}
