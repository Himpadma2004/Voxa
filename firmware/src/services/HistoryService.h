#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "../models/History.h"

namespace VOXA
{
    class StorageService;

    class HistoryService
    {
    public:
        explicit HistoryService(StorageService* storage);

        [[nodiscard]] std::vector<HistoryEntry> getAll();
        void logEvent(const std::string& screen, const std::string& action);

    private:
        StorageService* m_storage { nullptr };
    };
}
