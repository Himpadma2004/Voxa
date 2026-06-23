from vector_db.chroma_client import (
    get_collection
)

from sentence_transformers import (
    SentenceTransformer
)

print("🔄 Loading Search Embedding Model...")

model = SentenceTransformer(
    "all-MiniLM-L6-v2"
)

print("✅ Search Model Loaded")


def semantic_search(
    query,
    limit=5
):

    print("🔄 Creating Query Embedding...")

    collection = get_collection()

    query_embedding = model.encode(
        query
    ).tolist()

    print("🔄 Searching ChromaDB...")

    results = collection.query(
        query_embeddings=[
            query_embedding
        ],
        n_results=limit
    )

    print("✅ Search Completed")

    return results