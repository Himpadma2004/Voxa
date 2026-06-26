#pragma once

#include <cstdint>
#include <string>

namespace VOXA
{
    struct HistoryEntry
    {
        uint32_t    id        { 0 };
        std::string screen;
        std::string action;
        std::string timestamp;

        [[nodiscard]] bool isValid() const { return id != 0 && !screen.empty(); }
    };
}
