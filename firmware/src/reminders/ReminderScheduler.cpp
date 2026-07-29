#include "ReminderScheduler.h"
#include <Arduino.h>
#include <algorithm>

namespace VOXA
{
    bool ReminderScheduler::update(std::vector<Reminder>& reminders, time_t now, uint32_t repeatIntervalSec)
    {
        bool changed = false;

        // Auto-delete / cleanup check for completed reminders older than 48 hours
        auto initialSize = reminders.size();
        reminders.erase(
            std::remove_if(reminders.begin(), reminders.end(),
                [now](const Reminder& r) {
                    if (r.status == ReminderStatus::COMPLETED && r.completedAt > 0)
                    {
                        if (now >= r.completedAt + 48 * 3600)
                        {
                            Serial.printf("[ReminderScheduler] Reminder '%s' (ID %u) expired 48h limit. Auto deleting.\n", r.title.c_str(), r.id);
                            return true;
                        }
                    }
                    return false;
                }),
            reminders.end()
        );
        if (reminders.size() != initialSize)
        {
            changed = true;
        }

        // Evaluate state transitions
        for (auto& r : reminders)
        {
            if (r.status == ReminderStatus::PENDING)
            {
                if (now >= r.reminderTime && r.reminderTime > 0)
                {
                    r.status = ReminderStatus::ACTIVE;
                    r.lastTriggeredTime = now;
                    Serial.printf("[ReminderScheduler] Reminder '%s' (ID %u) triggered -> ACTIVE at %ld\n", r.title.c_str(), r.id, (long)now);
                    changed = true;
                }
            }
            else if (r.status == ReminderStatus::SNOOZED)
            {
                if (now >= r.snoozeUntil && r.snoozeUntil > 0)
                {
                    r.status = ReminderStatus::ACTIVE;
                    r.lastTriggeredTime = now;
                    Serial.printf("[ReminderScheduler] Snoozed reminder '%s' (ID %u) expired -> ACTIVE at %ld\n", r.title.c_str(), r.id, (long)now);
                    changed = true;
                }
            }
            else if (r.status == ReminderStatus::ACTIVE)
            {
                if (now - r.lastTriggeredTime >= (time_t)repeatIntervalSec)
                {
                    r.lastTriggeredTime = now;
                    Serial.printf("[ReminderScheduler] Active reminder '%s' (ID %u) re-triggered (ignored repeat) at %ld\n", r.title.c_str(), r.id, (long)now);
                    changed = true;
                }
            }
        }

        return changed;
    }
}
