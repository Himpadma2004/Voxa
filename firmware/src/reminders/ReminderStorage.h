#pragma once

#include <vector>
#include "../models/Reminder.h"

namespace VOXA
{
    class ReminderStorage
    {
    public:
        static std::vector<Reminder> loadAll();
        static bool saveAll(const std::vector<Reminder>& reminders);
    };
}
