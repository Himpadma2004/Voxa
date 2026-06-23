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

from vector_db.memory_recall import (
    recall_memories
)

query = input(
    "Ask Voxa: "
)

response = recall_memories(
    query
)

print(
    "\n===== VOXA RESPONSE =====\n"
)

print(
    response
)