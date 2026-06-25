from datetime import datetime
from datetime import timedelta

import uuid
import os

from dotenv import load_dotenv
from pymongo import MongoClient

load_dotenv()

client = MongoClient(
    os.getenv("MONGO_URI")
)

db = client[
    os.getenv("MONGO_DB")
]

reminders = db["reminders"]

reminder_time = datetime.utcnow() + timedelta(
    seconds=20
)

document = {

    "reminder_id":
        str(uuid.uuid4()),

    "audio_id":
        "test",

    "title":
        "TEST ALARM",

    "reminder_time":
        reminder_time,

    "status":
        "pending",

    "created_at":
        datetime.utcnow()
}

reminders.insert_one(
    document
)

print(
    f"✅ Alarm Set For {reminder_time}"
)