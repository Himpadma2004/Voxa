import chromadb

client = chromadb.PersistentClient(
    path="chroma_storage"
)

collection = client.get_or_create_collection(
    name="voxa_memories"
)


def add_memory(
    memory_id,
    document,
    metadata
):

    collection.add(

        ids=[
            memory_id
        ],

        documents=[
            document
        ],

        metadatas=[
            metadata
        ]
    )

    print(
        "✅ Stored in ChromaDB"
    )


def get_collection():

    return collection