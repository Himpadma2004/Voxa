import sys
import os

sys.path.append(
    os.path.abspath(
        os.path.join(
            os.path.dirname(__file__),
            "..",
            "backend"
        )
    )
)

import uuid

from vector_db.chroma_client import add_memory


memory_id = str(uuid.uuid4())

add_memory(
    memory_id=memory_id,
    document="Tomorrow remind me to call Rahul at 8 PM",
    metadata={
        "category": "Reminder"
    }
)

print("SUCCESS")