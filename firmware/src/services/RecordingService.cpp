#include "RecordingService.h"
#include "DataService.h"
#include "StorageService.h"
#include <Arduino.h>    // Serial
#include <algorithm>    // std::sort


namespace VOXA
{
    RecordingService::RecordingService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<Recording> RecordingService::getAll()
    {
        // Fetch from in-RAM cloud-synced store; heal invalid entries.
        auto recordings = dataService.getRecordings();
        for (auto it = recordings.begin(); it != recordings.end();)
        {
            if (it->filePath == "filePath.wav" ||
                it->filePath == "filePath"     ||
                it->filePath.empty())
            {
                Serial.printf("[RecordingService] Healing DB: removing invalid record ID %u (%s)\n",
                              it->id, it->filePath.c_str());
                dataService.removeRecordingLocal(it->id);
                it = recordings.erase(it);
            }
            else
            {
                ++it;
            }
        }
        std::sort(recordings.begin(), recordings.end(),
                  [](const Recording& a, const Recording& b) {
                      time_t tA = DataService::parseTimestampToEpoch(a.timestamp);
                      time_t tB = DataService::parseTimestampToEpoch(b.timestamp);
                      if (tA != tB) return tA > tB;
                      return a.id < b.id;
                  });
        return recordings;
    }

    Recording RecordingService::add(const std::string& title,
                                    const std::string& filePath,
                                    uint32_t           durationSeconds,
                                    const std::string& timestamp)
    {
        Recording r;
        r.id              = 0;
        r.title           = title;
        r.filePath        = filePath;  // cloud URL / audio_id returned by backend
        r.durationSeconds = durationSeconds;
        r.timestamp       = timestamp;

        // Persist to StorageService settings store (history), update in-RAM list
        m_storage->saveRecording(r);
        dataService.addRecordingLocal(r);

        // No local file pruning — all recordings live in the cloud.
        Serial.printf("[RecordingService] Added recording: title=%s filePath=%s\n",
                      title.c_str(), filePath.c_str());
        return r;
    }

    bool RecordingService::remove(uint32_t id)
    {
        // Remove from in-RAM list; the cloud record is removed via the backend API
        // (handled by the screen that calls the DELETE endpoint).
        dataService.removeRecordingLocal(id);
        return m_storage->deleteRecording(id);
    }

    bool RecordingService::update(const Recording& recording)
    {
        dataService.updateRecordingLocal(recording);
        return m_storage->saveRecording(recording);
    }
}