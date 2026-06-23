import sys
import os
import time

from datetime import datetime

sys.path.append(
    os.path.abspath(
        os.path.join(
            os.path.dirname(__file__),
            ".."
        )
    )
)

from database.mongodb import db

reminders_collection = db["reminders"]


def check_reminders():

    print(
        "🔔 Reminder Checker Started"
    )

    reminders = reminders_collection.find(
        {
            "status": "pending"
        }
    )

    count = 0

    for reminder in reminders:

        title = reminder.get(
            "title",
            "Unknown"
        )

        reminder_time = reminder.get(
            "reminder_time"
        )

        print(
            f"Title: {title}"
        )

        print(
            f"Time: {reminder_time}"
        )

        print("-" * 40)

        count += 1

    print(
        f"Checked {count} reminders"
    )


if __name__ == "__main__":

    check_reminders()