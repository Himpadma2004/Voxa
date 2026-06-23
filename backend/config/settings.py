import os
from dotenv import load_dotenv

load_dotenv()

ACTIVE_MODEL = os.getenv(
    "ACTIVE_MODEL"
)