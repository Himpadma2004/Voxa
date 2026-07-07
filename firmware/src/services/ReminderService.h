#pragma once

#include <cstdint>
#include <vector>

#include "../models/Reminder.h"

namespace VOXA
{
    class StorageService;

    /// High-level business logic for reminders.
    ///
    /// This is the only class that screens should talk to about reminders.
    /// It owns no data itself — it delegates persistence to StorageService.
    class ReminderService
    {
    public:
        /// @param storage  Non-owning pointer. Must outlive this service.
        explicit ReminderService(StorageService* storage);

        /// Returns all reminders, sorted by id ascending.
        [[nodiscard]] std::vector<Reminder> getAll();

        /// Returns only reminders that are not yet completed.
        [[nodiscard]] std::vector<Reminder> getPending();

        /// Returns the number of pending (uncompleted) reminders.
        [[nodiscard]] int getPendingCount();

        /// Add a new reminder. The id field is ignored — a new id is assigned.
        /// Returns the persisted reminder with its assigned id.
        Reminder add(const std::string& title, const std::string& dateTime);

        /// Mark a reminder as completed.
        bool markComplete(uint32_t id);

        /// Permanently remove a reminder.
        bool remove(uint32_t id);

    private:
        StorageService* m_storage { nullptr };
    };
}
