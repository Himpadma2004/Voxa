#include "RecordingService.h"
#include "StorageService.h"
#include <SPIFFS.h>
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
        bool changed = false;
        for (auto it = recordings.begin(); it != recordings.end(); )
        {
            if (it->filePath == "filePath.wav" || it->filePath == "filePath" || it->filePath.empty())
            {
                Serial.printf("[RecordingService] Healing DB: removing invalid record ID %u (%s)\n", it->id, it->filePath.c_str());
                m_storage->deleteRecording(it->id);
                it = recordings.erase(it);
                changed = true;
            }
            else
            {
                ++it;
            }
        }
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

        // Load and auto-prune to keep only the 3 latest recordings (satisfying 'clean the Recording libraries')
        auto all = m_storage->loadAllRecordings();
        std::sort(all.begin(), all.end(),
                  [](const Recording& a, const Recording& b) { return a.id < b.id; });

        while (all.size() > 3)
        {
            SPIFFS.remove(all[0].filePath.c_str());
            Serial.printf("[RecordingService] Pruned oldest physical recording file: %s\n", all[0].filePath.c_str());
            m_storage->deleteRecording(all[0].id);
            all.erase(all.begin());
        }

        for (auto it = all.rbegin(); it != all.rend(); ++it)
            if (it->title == title && it->filePath == filePath)
                return *it;
        return r;
    }

    bool RecordingService::remove(uint32_t id)
    {
        auto all = m_storage->loadAllRecordings();
        for (const auto& r : all)
        {
            if (r.id == id)
            {
                SPIFFS.remove(r.filePath.c_str());
                Serial.printf("[RecordingService] Deleted physical WAV file: %s\n", r.filePath.c_str());
                break;
            }
        }
        return m_storage->deleteRecording(id);
    }

    bool RecordingService::update(const Recording& recording)
    {
        return m_storage->saveRecording(recording);
    }
}

