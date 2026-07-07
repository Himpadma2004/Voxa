#include "HistoryService.h"
#include "StorageService.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace VOXA
{
    HistoryService::HistoryService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<HistoryEntry> HistoryService::getAll()
    {
        auto entries = m_storage->loadAllHistory();
        std::sort(entries.begin(), entries.end(),
                  [](const HistoryEntry& a, const HistoryEntry& b) { return a.id < b.id; });
        return entries;
    }

    void HistoryService::logEvent(const std::string& screen, const std::string& action)
    {
        HistoryEntry entry;
        entry.id = 0;
        entry.screen = screen;
        entry.action = action;

        // Fetch current ISO timestamp
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm local {};
#if defined(_MSC_VER)
        localtime_s(&local, &t);
#else
        localtime_r(&t, &local);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &local);
        entry.timestamp = buf;

        m_storage->saveHistory(entry);
    }
}
