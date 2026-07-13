from fastapi import APIRouter, Query
from fastapi.responses import JSONResponse
from vector_db.memory_recall import recall_memories

router = APIRouter(prefix="/api/search", tags=["Search"])


@router.get("")
def search(q: str = Query(..., description="The query to search memories for")):
    """
    Search memories in ChromaDB and answer the query using the LLM.
    """
    try:
        print(f"\n[Server] Searching memories for: \"{q}\"")
        answer = recall_memories(q)
        return JSONResponse(content={
            "success": True,
            "query": q,
            "answer": answer
        })
    except Exception as e:
        print(f"[Server] Search error: {e}")
        return JSONResponse(
            status_code=500,
            content={
                "success": False,
                "error": str(e)
            }
        )
