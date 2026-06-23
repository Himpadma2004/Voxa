from vector_db.search_service import (
    search_memories
)

from ai.recall_client import (
    recall_answer
)

from config.settings import (
    ACTIVE_MODEL
)


def recall_memories(
    user_query
):

    results = search_memories(
        user_query,
        n_results=5
    )

    documents = results.get(
        "documents",
        [[]]
    )[0]

    if not documents:

        return "No relevant memories found."

    context = "\n\n".join(
        documents
    )

    prompt = f"""
You are Voxa Memory Recall Engine.

User Query:
{user_query}

Relevant Memories:
{context}

Answer using only the memories provided.

Return a concise response.
"""

    answer = recall_answer(
        prompt,
        ACTIVE_MODEL
    )

    return answer