#pragma once

#include <cstdint>
#include <string>

namespace VOXA
{
    /// Represents a captured voice recording or typed memory.
    struct Memory
    {
        uint32_t    id              { 0 };
        std::string title;
        std::string content;            ///< Transcript or short description
        std::string category;           ///< "voice" | "note" | "idea"
        std::string tags;               ///< Comma-separated tags
        std::string createdAt;          ///< ISO timestamp
        std::string updatedAt;          ///< ISO timestamp
        uint32_t    importance      { 1 };  ///< Range 1-5
        bool        favorite        { false };
        std::string source;             ///< "Voice" | "Reminder" | "Idea" | "Manual" | "AI"
        
        // Retained for backward compatibility
        std::string timestamp;          ///< e.g. "2026-06-26 11:44"
        uint32_t    durationSeconds { 0 };  ///< 0 for non-voice memories
        std::string comments;           ///< Delimited comments

        [[nodiscard]] bool isValid() const { return id != 0 && !title.empty(); }
    };
}
