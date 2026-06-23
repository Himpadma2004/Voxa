from vector_db.chroma_client import (
    collection
)


def search_memories(
    query,
    n_results=5
):

    results = collection.query(

        query_texts=[
            query
        ],

        n_results=n_results
    )

    return results