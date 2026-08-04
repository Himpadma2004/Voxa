import os
from pydantic_settings import BaseSettings

class Settings(BaseSettings):
    PROJECT_NAME: str = "U'rWay"
    PROJECT_TAGLINE: str = "Your way, made visible."
    VERSION: str = "1.0.0"
    API_V1_STR: str = "/api/v1"
    
    # MongoDB Database settings
    MONGODB_URL: str = os.getenv("URWAY_MONGODB_URL", "mongodb://localhost:27017")
    DATABASE_NAME: str = os.getenv("URWAY_DB_NAME", "urway_db")
    
    # Security Settings (PBKDF2 & AES-256-GCM)
    JWT_SECRET_KEY: str = os.getenv("URWAY_JWT_SECRET", "urway-super-secret-jwt-key-2026-secure")
    ALGORITHM: str = "HS256"
    ACCESS_TOKEN_EXPIRE_MINUTES: int = 60 * 24 * 7 # 7 days
    
    # Master System Encryption Key (32 bytes hex encoded for AES-256)
    MASTER_ENCRYPTION_KEY: str = os.getenv("URWAY_MASTER_KEY", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")

    class Config:
        case_sensitive = True

settings = Settings()
