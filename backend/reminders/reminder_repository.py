import uuid

from datetime import datetime

from database.mongodb import db

reminders_collection = db[
    "reminders"
]


def save_reminder(
    audio_id,
    title,
    reminder_time
):

    reminder = {

        "reminder_id":
            str(uuid.uuid4()),

        "audio_id":
            audio_id,

        "title":
            title,

        "reminder_time":
            reminder_time,

        "status":
            "pending",

        "created_at":
            datetime.utcnow()
    }

    reminders_collection.insert_one(
        reminder
    )

    print(
        "✅ Reminder Saved"
    )