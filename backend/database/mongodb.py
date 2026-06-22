from pymongo import MongoClient
from dotenv import load_dotenv
import os

load_dotenv()

client = MongoClient(
    os.getenv("MONGO_URI")
)

db = client[
    os.getenv("MONGO_DB")
]

collection = db[
    os.getenv("MONGO_COLLECTION")
]


def save_audio_metadata(
    document
):

    result = (
        collection.insert_one(
            document
        )
    )

    return str(
        result.inserted_id
    )