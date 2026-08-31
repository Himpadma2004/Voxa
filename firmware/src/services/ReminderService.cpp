#include "ReminderService.h"
#include "DataService.h"
#include "../reminders/ReminderManager.h"
#include <ctime>

namespace VOXA
{
    static time_t parseDateTimeStringToTime(const std::string& str)
    {
        time_t now = ReminderManager::instance().getCurrentTime();
        struct tm t;
        localtime_r(&now, &t);
        
        int hour = 18, minute = 0;
        if (sscanf(str.c_str(), "%d:%d", &hour, &minute) == 2)
        {
            t.tm_hour = hour;
            t.tm_min = minute;
            t.tm_sec = 0;
            time_t scheduled = mktime(&t);
            if (scheduled < now)
            {
                scheduled += 24 * 3600;
            }
            return scheduled;
        }
        return now + 3600;
    }

    ReminderService::ReminderService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<Reminder> ReminderService::getAll()
    {
        auto synced = dataService.getReminders();
        auto local = ReminderManager::instance().getAllReminders();

        for (const auto& loc : local)
        {
            bool found = false;
            for (const auto& syn : synced)
            {
                if (syn.id == loc.id || (!loc.title.empty() && syn.title == loc.title))
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                synced.push_back(loc);
            }
        }

        std::sort(synced.begin(), synced.end(), [](const Reminder& a, const Reminder& b) {
            if (a.pinned != b.pinned) return a.pinned > b.pinned;
            time_t tA = DataService::parseTimestampToEpoch(a.dateTime);
            time_t tB = DataService::parseTimestampToEpoch(b.dateTime);
            if (tA != tB) return tA > tB;
            return a.id < b.id;
        });

        return synced;
    }

    std::vector<Reminder> ReminderService::getPending()
    {
        auto all = getAll();
        std::vector<Reminder> pending;
        pending.reserve(all.size());
        for (const auto& r : all)
        {
            if (!r.completed && r.status != ReminderStatus::COMPLETED)
            {
                pending.push_back(r);
            }
        }
        return pending;
    }

    int ReminderService::getPendingCount()
    {
        return static_cast<int>(getPending().size());
    }

    Reminder ReminderService::add(const std::string& title, const std::string& dateTime)
    {
        time_t due = parseDateTimeStringToTime(dateTime);
        ReminderManager::instance().addReminder(title, "", due);
        
        Reminder r;
        r.title = title;
        r.dateTime = dateTime;
        r.reminderTime = due;
        dataService.addReminderLocal(r);
        return r;
    }

    bool ReminderService::markComplete(uint32_t id)
    {
        ReminderManager::instance().dismissReminder(id);
        return dataService.removeReminderLocal(id);
    }

    bool ReminderService::remove(uint32_t id)
    {
        ReminderManager::instance().deleteReminder(id);
        return dataService.removeReminderLocal(id);
    }
}
