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
    existing = reminders_collection.find_one({"audio_id": audio_id, "title": title.strip() if title else ""})
    if existing:
        if reminder_time:
            reminders_collection.update_one(
                {"_id": existing["_id"]},
                {"$set": {"reminder_time": reminder_time}}
            )
        print("✅ Reminder Updated")
        return

    reminder = {
        "reminder_id": str(uuid.uuid4()),
        "audio_id": audio_id,
        "title": title.strip() if title else "",
        "reminder_time": reminder_time,
        "status": "pending",
        "created_at": datetime.utcnow()
    }

    reminders_collection.insert_one(reminder)
    print("✅ Reminder Saved")


def load_all_reminders():
    """
    Load all reminders from the MongoDB collection.
    """
    return list(reminders_collection.find())