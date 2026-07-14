#include "ReminderService.h"
#include "DataService.h"
#include "StorageService.h"

namespace VOXA
{
    ReminderService::ReminderService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<Reminder> ReminderService::getAll()
    {
        return dataService.getReminders();
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
        dataService.addReminderLocal(r);

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
                dataService.updateReminderLocal(r);
                return m_storage->saveReminder(r);
            }
        }
        return false;
    }

    bool ReminderService::remove(uint32_t id)
    {
        dataService.removeReminderLocal(id);
        return m_storage->deleteReminder(id);
    }
}
