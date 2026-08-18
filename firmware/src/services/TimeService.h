#pragma once

#include <string>

namespace VOXA
{
    /// Provides current date/time information from the system clock.
    class TimeService
    {
    public:
        TimeService() = default;

        /// Initialize NTP configuration (GMT+5:30 for Asia/Kolkata).
        void begin();

        /// Returns the current local time as "HH:MM AM/PM", e.g. "11:44 AM".
        [[nodiscard]] std::string getCurrentTime() const;

        /// Returns the current local date as "Weekday, Mon DD", e.g. "Friday, Jun 26".
        [[nodiscard]] std::string getCurrentDate() const;

        /// Returns a combined date+time string, e.g. "Friday, Jun 26  11:44 AM".
        [[nodiscard]] std::string getFormattedDateTime() const;

        /// Returns the current local date-time as ISO 8601 string, e.g. "2026-08-18T16:12:13".
        [[nodiscard]] std::string getISO8601Time() const;

        /// Returns the current year as an integer.
        [[nodiscard]] int getCurrentYear() const;

        /// Set system/RTC time (easy synchronization hook).
        void setTime(int hour, int minute, int second, int day, int month, int year);
    };

    extern TimeService timeService;
}
