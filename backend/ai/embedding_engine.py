from sentence_transformers import (
    SentenceTransformer
)

print(
    "🔄 Loading Embedding Model..."
)

model = SentenceTransformer(
    "all-MiniLM-L6-v2"
)

print(
    "✅ Embedding Model Loaded"
)


def generate_embedding(
    text
):

    embedding = model.encode(
        text
    )

    return embedding.tolist()