#include "RecordingService.h"
#include "DataService.h"
#include "StorageService.h"
#include <SPIFFS.h>

namespace VOXA
{
    RecordingService::RecordingService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<Recording> RecordingService::getAll()
    {
        auto recordings = dataService.getRecordings();
        for (auto it = recordings.begin(); it != recordings.end(); )
        {
            if (it->filePath == "filePath.wav" || it->filePath == "filePath" || it->filePath.empty())
            {
                Serial.printf("[RecordingService] Healing DB: removing invalid record ID %u (%s)\n", it->id, it->filePath.c_str());
                dataService.removeRecordingLocal(it->id);
                m_storage->deleteRecording(it->id);
                it = recordings.erase(it);
            }
            else
            {
                ++it;
            }
        }
        std::sort(recordings.begin(), recordings.end(),
                  [](const Recording& a, const Recording& b) { return a.id > b.id; });
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
        dataService.addRecordingLocal(r);

        // Load and auto-prune to keep only the 3 latest recordings (satisfying 'clean the Recording libraries')
        auto all = dataService.getRecordings();
        while (all.size() > 3)
        {
            const auto& oldest = all.back();
            SPIFFS.remove(oldest.filePath.c_str());
            Serial.printf("[RecordingService] Pruned oldest physical recording file: %s\n", oldest.filePath.c_str());
            dataService.removeRecordingLocal(oldest.id);
            m_storage->deleteRecording(oldest.id);
            all.pop_back();
        }

        return r;
    }

    bool RecordingService::remove(uint32_t id)
    {
        auto all = dataService.getRecordings();
        for (const auto& r : all)
        {
            if (r.id == id)
            {
                SPIFFS.remove(r.filePath.c_str());
                Serial.printf("[RecordingService] Deleted physical WAV file: %s\n", r.filePath.c_str());
                break;
            }
        }
        dataService.removeRecordingLocal(id);
        return m_storage->deleteRecording(id);
    }

    bool RecordingService::update(const Recording& recording)
    {
        dataService.updateRecordingLocal(recording);
        return m_storage->saveRecording(recording);
    }
}

