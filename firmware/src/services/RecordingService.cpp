#include "RecordingService.h"
#include "DataService.h"
#include "StorageService.h"
#include "../storage/SpiffsMutex.h"
#include <SPIFFS.h>

namespace VOXA
{
    RecordingService::RecordingService(StorageService *storage)
        : m_storage(storage)
    {
    }

    std::vector<Recording> RecordingService::getAll()
    {
        auto recordings = dataService.getRecordings();
        for (auto it = recordings.begin(); it != recordings.end();)
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
                  [](const Recording &a, const Recording &b)
                  { return a.id > b.id; });
        return recordings;
    }

    Recording RecordingService::add(const std::string &title, const std::string &filePath, uint32_t durationSeconds, const std::string &timestamp)
    {
        Recording r;
        r.id = 0;
        r.title = title;
        r.filePath = filePath;
        r.durationSeconds = durationSeconds;
        r.timestamp = timestamp;
        m_storage->saveRecording(r);
        dataService.addRecordingLocal(r);

        // Load and auto-prune to keep only the 3 latest LOCAL/PHYSICAL recordings.
        // Cloud-synced recordings (filePath is a remote URL, e.g. an S3 link pulled
        // down by syncRecordings()) don't occupy any local SPIFFS space and must be
        // left alone here — deleting them only wipes them from the local index until
        // the next sync repopulates it, and previously this loop was counting and
        // removing them right alongside genuine local files.
        auto all = dataService.getRecordings();
        std::vector<Recording> localOnly;
        for (const auto &rec : all)
        {
            if (!rec.filePath.empty() && rec.filePath[0] == '/')
            {
                localOnly.push_back(rec);
            }
        }
        while (localOnly.size() > 3)
        {
            const auto &oldest = localOnly.back();
            {
                SpiffsLock lock("RecordingService::pruneOldest");
                SPIFFS.remove(oldest.filePath.c_str());
            }
            Serial.printf("[RecordingService] Pruned oldest physical recording file: %s\n", oldest.filePath.c_str());
            dataService.removeRecordingLocal(oldest.id);
            m_storage->deleteRecording(oldest.id);
            localOnly.pop_back();
        }

        return r;
    }

    bool RecordingService::remove(uint32_t id)
    {
        auto all = dataService.getRecordings();
        for (const auto &r : all)
        {
            if (r.id == id)
            {
                {
                    SpiffsLock lock("RecordingService::remove");
                    SPIFFS.remove(r.filePath.c_str());
                }
                Serial.printf("[RecordingService] Deleted physical WAV file: %s\n", r.filePath.c_str());
                break;
            }
        }
        dataService.removeRecordingLocal(id);
        return m_storage->deleteRecording(id);
    }

    bool RecordingService::update(const Recording &recording)
    {
        dataService.updateRecordingLocal(recording);
        return m_storage->saveRecording(recording);
    }
}