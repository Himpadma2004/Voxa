import sys
import os
import time

from datetime import datetime

from playsound import playsound

sys.path.append(
    os.path.abspath(
        os.path.join(
            os.path.dirname(__file__),
            ".."
        )
    )
)

from database.mongodb import db

reminders_collection = db[
    "reminders"
]

ALARM_SOUND = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__),
        "..",
        "assets",
        "reminder.mp3"
    )
)


def trigger_alarm(
    reminder
):

    print("\n")
    print("=" * 50)
    print("🔔 REMINDER ALERT")
    print("=" * 50)

    print(
        f"Title: {reminder.get('title')}"
    )

    print(
        f"Time: {reminder.get('reminder_time')}"
    )

    print("=" * 50)

    try:

        playsound(
            ALARM_SOUND
        )

    except Exception as e:

        print(
            f"❌ Alarm Error: {e}"
        )

    reminders_collection.update_one(

        {
            "_id":
                reminder["_id"]
        },

        {
            "$set": {
                "status":
                    "completed"
            }
        }
    )

    print(
        "✅ Reminder Completed"
    )


def check_reminders():

    print(
        "🔔 Reminder Checker Started"
    )

    print(
        f"Alarm File: {ALARM_SOUND}"
    )

    while True:

        try:

            now = datetime.utcnow()

            reminders = list(

                reminders_collection.find(
                    {
                        "status":
                            "pending"
                    }
                )
            )

            print(
                f"\n🕒 {now.strftime('%H:%M:%S')} | Checking {len(reminders)} reminders..."
            )

            for reminder in reminders:

                reminder_time = reminder.get(
                    "reminder_time"
                )

                if not reminder_time:
                    continue

                if isinstance(
                    reminder_time,
                    str
                ):

                    print(
                        f"⚠️ Skipping old reminder: {reminder_time}"
                    )

                    continue

                if reminder_time <= now:

                    trigger_alarm(
                        reminder
                    )

        except Exception as e:

            print(
                f"❌ Checker Error: {e}"
            )

        time.sleep(5)


if __name__ == "__main__":

    check_reminders()