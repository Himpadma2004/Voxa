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

from vector_db.search_service import (
    search_memories
)

results = search_memories(

    "Bokshar ideas"
)

print(
    "\n===== SEARCH RESULTS =====\n"
)

print(
    results
)