#include "RecordingService.h"
#include "StorageService.h"

#include <algorithm>

namespace VOXA
{
    RecordingService::RecordingService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<Recording> RecordingService::getAll()
    {
        auto recordings = m_storage->loadAllRecordings();
        std::sort(recordings.begin(), recordings.end(),
                  [](const Recording& a, const Recording& b) { return a.id < b.id; });
        return recordings;
    }

    Recording RecordingService::add(const std::string& title, const std::string& filePath, uint32_t durationSeconds, const std::string& timestamp)
    {
        Recording r;
        r.id = 0;
        r.title = title;
        r.filePath = filePath;
        r.durationSeconds = durationSeconds;
        r.timestamp = timestamp;
        m_storage->saveRecording(r);

        auto all = m_storage->loadAllRecordings();
        for (auto it = all.rbegin(); it != all.rend(); ++it)
            if (it->title == title && it->filePath == filePath)
                return *it;
        return r;
    }

    bool RecordingService::remove(uint32_t id)
    {
        return m_storage->deleteRecording(id);
    }
}
