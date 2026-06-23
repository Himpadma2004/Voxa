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
    semantic_search
)

print("STARTING SEARCH")

results = semantic_search(
    "phone call reminder"
)

print("\nRESULTS:")
print(results)