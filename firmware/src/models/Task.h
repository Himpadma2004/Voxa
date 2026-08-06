#pragma once
#include <cstdint>
#include <string>

namespace VOXA
{
    struct TaskItem
    {
        uint32_t id { 0 };
        std::string title;
        std::string content;
        std::string timestamp;
        std::string comments;
        std::string sourceId;   // audio_id UUID from MongoDB (primary toggle key)
        std::string mongoId;    // raw _id ObjectId hex string (reliable fallback toggle key)
        bool isDone { false };
        bool isPinned { false };
    };
}
