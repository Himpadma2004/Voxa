from motor.motor_asyncio import AsyncIOMotorClient
from app.config import settings

class Database:
    client: AsyncIOMotorClient = None

db = Database()

async def connect_to_mongo():
    db.client = AsyncIOMotorClient(settings.MONGODB_URL)
    print(f"[U'rWay DB] Connected to MongoDB: {settings.DATABASE_NAME}")

async def close_mongo_connection():
    if db.client:
        db.client.close()
        print("[U'rWay DB] MongoDB connection closed.")

def get_database():
    return db.client[settings.DATABASE_NAME]
