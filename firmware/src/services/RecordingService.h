#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "../models/Recording.h"

namespace VOXA
{
    class StorageService;

    class RecordingService
    {
    public:
        explicit RecordingService(StorageService* storage);

        [[nodiscard]] std::vector<Recording> getAll();
        Recording add(const std::string& title, const std::string& filePath, uint32_t durationSeconds, const std::string& timestamp);
        bool remove(uint32_t id);

    private:
        StorageService* m_storage { nullptr };
    };
}
