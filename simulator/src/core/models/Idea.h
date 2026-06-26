#pragma once

#include <cstdint>
#include <string>

namespace VOXA
{
    /// A captured idea note.
    struct Idea
    {
        uint32_t    id        { 0 };
        std::string title;
        std::string content;
        std::string timestamp;  ///< e.g. "2026-05-28"

        [[nodiscard]] bool isValid() const { return id != 0 && !title.empty(); }
    };
}
