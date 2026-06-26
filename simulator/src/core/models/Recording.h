#pragma once

#include <cstdint>
#include <string>

namespace VOXA
{
    struct Recording
    {
        uint32_t    id              { 0 };
        std::string title;
        std::string filePath;
        uint32_t    durationSeconds { 0 };
        std::string timestamp;

        [[nodiscard]] bool isValid() const { return id != 0 && !title.empty(); }
    };
}
