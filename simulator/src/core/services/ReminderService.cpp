#include "ReminderService.h"
#include "StorageService.h"

#include <algorithm>

namespace VOXA
{
    ReminderService::ReminderService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<Reminder> ReminderService::getAll()
    {
        auto reminders = m_storage->loadAllReminders();
        std::sort(reminders.begin(), reminders.end(),
                  [](const Reminder& a, const Reminder& b) { return a.id < b.id; });
        return reminders;
    }

    std::vector<Reminder> ReminderService::getPending()
    {
        auto all = getAll();
        std::vector<Reminder> pending;
        pending.reserve(all.size());
        for (const auto& r : all)
            if (!r.completed) pending.push_back(r);
        return pending;
    }

    int ReminderService::getPendingCount()
    {
        return static_cast<int>(getPending().size());
    }

    Reminder ReminderService::add(const std::string& title, const std::string& dateTime)
    {
        Reminder r;
        r.id       = 0; // StorageService assigns the id
        r.title    = title;
        r.dateTime = dateTime;
        r.completed = false;
        m_storage->saveReminder(r);

        // Reload to get the assigned id.
        auto all = m_storage->loadAllReminders();
        for (auto it = all.rbegin(); it != all.rend(); ++it)
            if (it->title == title && it->dateTime == dateTime)
                return *it;
        return r;
    }

    bool ReminderService::markComplete(uint32_t id)
    {
        auto all = m_storage->loadAllReminders();
        for (auto& r : all)
        {
            if (r.id == id)
            {
                r.completed = true;
                return m_storage->saveReminder(r);
            }
        }
        return false;
    }

    bool ReminderService::remove(uint32_t id)
    {
        return m_storage->deleteReminder(id);
    }
}
