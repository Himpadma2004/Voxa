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

from vector_db.chroma_client import (
    add_memory
)

add_memory(

    "test123",

    "Integrate YouTube into Bokshar",

    {
        "category": "Idea"
    }
)

print("SUCCESS")