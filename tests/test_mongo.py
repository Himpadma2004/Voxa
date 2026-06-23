# tests/test_mongo.py

from pymongo import MongoClient
from dotenv import load_dotenv
import os

load_dotenv()

uri = os.getenv("MONGO_URI")

print("URI loaded:", uri[:60] + "...")

client = MongoClient(
    uri,
    serverSelectionTimeoutMS=30000
)

print("Pinging Atlas...")
print(client.admin.command("ping"))

print("SUCCESS")