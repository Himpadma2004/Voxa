#include "ReminderService.h"
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
        return ReminderManager::instance().getAllReminders();
    }

    std::vector<Reminder> ReminderService::getPending()
    {
        auto all = getAll();
        std::vector<Reminder> pending;
        pending.reserve(all.size());
        for (const auto& r : all)
        {
            if (r.status != ReminderStatus::COMPLETED)
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
        
        // Return the newly created reminder
        auto all = getAll();
        if (!all.empty())
        {
            return all.back();
        }
        
        Reminder r;
        r.title = title;
        r.dateTime = dateTime;
        r.reminderTime = due;
        return r;
    }

    bool ReminderService::markComplete(uint32_t id)
    {
        return ReminderManager::instance().dismissReminder(id);
    }

    bool ReminderService::remove(uint32_t id)
    {
        return ReminderManager::instance().deleteReminder(id);
    }
}
