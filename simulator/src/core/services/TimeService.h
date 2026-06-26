#pragma once

#include <string>

namespace VOXA
{
    /// Provides current date/time information from the system clock.
    class TimeService
    {
    public:
        TimeService() = default;

        /// Returns the current local time as "HH:MM AM/PM", e.g. "11:44 AM".
        [[nodiscard]] std::string getCurrentTime() const;

        /// Returns the current local date as "Weekday, Mon DD", e.g. "Friday, Jun 26".
        [[nodiscard]] std::string getCurrentDate() const;

        /// Returns a combined date+time string, e.g. "Friday, Jun 26  11:44 AM".
        [[nodiscard]] std::string getFormattedDateTime() const;

        /// Returns the current year as an integer.
        [[nodiscard]] int getCurrentYear() const;
    };
}
