#include "ReminderStorage.h"
#include "../storage/StorageManager.h"
#include <ArduinoJson.h>
#include <ctime>

namespace VOXA
{
    static const char* kRemindersFilePath = "/reminders.json";

    std::vector<Reminder> ReminderStorage::loadAll()
    {
        std::vector<Reminder> list;
        JsonDocument doc;
        
        extern StorageManager storageManager;
        if (!storageManager.loadJson(kRemindersFilePath, doc))
        {
            return list;
        }

        JsonArray arr = doc.as<JsonArray>();
        for (JsonVariant val : arr)
        {
            Reminder r;
            r.id = val["id"] | 0;
            r.backendId = val["backendId"] | "";
            r.title = val["title"] | "";
            r.description = val["description"] | "";
            r.createdAt = val["createdAt"] | 0;
            r.reminderTime = val["reminderTime"] | 0;
            r.status = static_cast<ReminderStatus>(val["status"] | 0);
            r.lastTriggeredTime = val["lastTriggeredTime"] | 0;
            r.snoozeUntil = val["snoozeUntil"] | 0;
            r.completedAt = val["completedAt"] | 0;
            
            // Old fields support
            r.dateTime = val["dateTime"] | "";
            if (r.dateTime.empty() && r.reminderTime > 0)
            {
                struct tm timeinfo;
                time_t rTime = r.reminderTime;
                localtime_r(&rTime, &timeinfo);
                char buf[32];
                strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
                r.dateTime = buf;
            }
            r.comments = val["comments"] | "";
            if (r.comments.empty() && !r.description.empty())
            {
                r.comments = r.description;
            }
            r.completed = (r.status == ReminderStatus::COMPLETED);
            r.pinned = val["pinned"] | false;
            
            list.push_back(r);
        }
        return list;
    }

    bool ReminderStorage::saveAll(const std::vector<Reminder>& reminders)
    {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        
        for (const auto& r : reminders)
        {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = r.id;
            obj["backendId"] = r.backendId;
            obj["title"] = r.title;
            obj["description"] = r.description.empty() ? r.comments : r.description;
            obj["createdAt"] = (long long)r.createdAt;
            obj["reminderTime"] = (long long)r.reminderTime;
            obj["status"] = static_cast<int>(r.status);
            obj["lastTriggeredTime"] = (long long)r.lastTriggeredTime;
            obj["snoozeUntil"] = (long long)r.snoozeUntil;
            obj["completedAt"] = (long long)r.completedAt;
            
            // Old fields support
            obj["dateTime"] = r.dateTime;
            obj["comments"] = r.description.empty() ? r.comments : r.description;
            obj["completed"] = (r.status == ReminderStatus::COMPLETED) ? "true" : "false";
            obj["pinned"] = r.pinned;
        }
        
        extern StorageManager storageManager;
        return storageManager.saveJson(kRemindersFilePath, doc);
    }
}
