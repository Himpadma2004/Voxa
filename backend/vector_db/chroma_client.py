import chromadb

client = chromadb.PersistentClient(
    path="chroma_db"
)

collection = client.get_or_create_collection(
    name="voxa_memories"
)


def add_memory(
    memory_id,
    text,
    metadata=None
):

    print(
        "\n===== CHROMA INSERT ====="
    )

    print(
        f"Memory ID: {memory_id}"
    )

    print(
        f"Metadata: {metadata}"
    )

    print(
        "\nMemory Text:"
    )

    print(
        text
    )

    print(
        "\n========================="
    )

    try:

        collection.add(

            ids=[
                memory_id
            ],

            documents=[
                text
            ],

            metadatas=[
                metadata or {}
            ]
        )

        print(
            "✅ Stored in ChromaDB"
        )

    except Exception as e:

        print(
            f"❌ ChromaDB Error: {e}"
        )


def show_memory_count():

    count = collection.count()

    print(
        f"\n📊 Total Memories: {count}"
    )

    return count