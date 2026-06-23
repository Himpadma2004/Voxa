from pymongo import MongoClient
from dotenv import load_dotenv
from datetime import datetime
import os

load_dotenv()

MONGO_URI = os.getenv("MONGO_URI")
MONGO_DB = os.getenv("MONGO_DB")
MONGO_COLLECTION = os.getenv("MONGO_COLLECTION")

try:

    client = MongoClient(
        MONGO_URI,
        serverSelectionTimeoutMS=10000,
        connectTimeoutMS=10000,
        socketTimeoutMS=10000
    )

    client.admin.command("ping")

    print("✅ MongoDB Connected")

except Exception as e:

    print(f"❌ MongoDB Connection Error: {e}")
    raise


db = client[MONGO_DB]

collection = db[MONGO_COLLECTION]


def save_audio_metadata(document):

    result = collection.insert_one(
        document
    )

    return str(
        result.inserted_id
    )


def update_transcript(
    audio_id,
    transcript
):

    collection.update_one(

        {
            "audio_id": audio_id
        },

        {
            "$set": {

                "transcript":
                    transcript,

                "status":
                    "transcribed",

                "transcribed_at":
                    datetime.utcnow()
            }
        }
    )

    print(
        "✅ MongoDB updated"
    )


def update_llm_result(
    audio_id,
    llm_data,
    model_name
):

    collection.update_one(

        {
            "audio_id": audio_id
        },

        {
            "$set": {

                "status":
                    "processed",

                "processed_at":
                    datetime.utcnow(),

                "llm_model":
                    model_name,

                "category":
                    llm_data.get(
                        "category",
                        "Other"
                    ),

                "summary":
                    llm_data.get(
                        "summary",
                        ""
                    ),

                "tasks":
                    llm_data.get(
                        "tasks",
                        []
                    ),

                "reminders":
                    llm_data.get(
                        "reminders",
                        []
                    ),

                "ideas":
                    llm_data.get(
                        "ideas",
                        []
                    ),

                "questions":
                    llm_data.get(
                        "questions",
                        []
                    ),

                "priority":
                    llm_data.get(
                        "priority",
                        "Low"
                    ),

                "llm_raw":
                    llm_data
            }
        }
    )

    print(
        "✅ MongoDB LLM Data Updated"
    )


def get_audio_by_id(
    audio_id
):

    return collection.find_one(
        {
            "audio_id":
                audio_id
        }
    )


def get_all_notes():

    return list(
        collection.find()
    )