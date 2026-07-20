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

from vector_db.memory_recall import recall_memories

print("=" * 60)
print("VOXA DATABASE & MEMORY SEARCH TEST")
print("=" * 60)

test_query = "when I need to call Jennifer"
print(f"\n[Test] Querying: \"{test_query}\"...\n")

try:
    answer = recall_memories(test_query)
    print("===== VOXA LLM SEARCH ANSWER =====")
    print(answer)
    print("===================================")
except Exception as e:
    print(f"Error executing recall query: {e}")
