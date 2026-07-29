#pragma once

#include <vector>
#include "../models/Reminder.h"

namespace VOXA
{
    class ReminderScheduler
    {
    public:
        // Processes state updates for all reminders. Returns true if any reminder was modified (so storage should be saved).
        static bool update(std::vector<Reminder>& reminders, time_t now, uint32_t repeatIntervalSec);
    };
}
