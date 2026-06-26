#pragma once

#include <cstdint>
#include <string>

namespace VOXA
{
    /// A single reminder entry stored by the user.
    struct Reminder
    {
        uint32_t    id        { 0 };
        std::string title;
        std::string dateTime;       ///< ISO-style string, e.g. "2026-07-01 20:00"
        bool        completed { false };

        /// Returns true if this reminder has been set and has a valid id.
        [[nodiscard]] bool isValid() const { return id != 0 && !title.empty(); }
    };
}
