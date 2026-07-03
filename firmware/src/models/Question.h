#pragma once

#include <cstdint>
#include <string>

namespace VOXA
{
    /// A question the user wants to explore or has asked the AI.
    struct Question
    {
        uint32_t    id       { 0 };
        std::string text;
        std::string answer;
        std::string timestamp;  ///< e.g. "2026-05-28"
        bool        answered { false };
        std::string comments;   ///< Delimited comments

        [[nodiscard]] bool isValid() const { return id != 0 && !text.empty(); }
    };
}
