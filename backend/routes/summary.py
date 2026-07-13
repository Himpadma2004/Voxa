from fastapi import APIRouter
from fastapi.responses import JSONResponse
from summaries.daily_summary_service import generate_daily_summary

router = APIRouter(prefix="/api/summary", tags=["Summary"])


@router.get("/daily")
def get_daily_summary():
    """
    Generate and fetch the AI summary of today's notes.
    """
    try:
        print("\n[Server] Generating daily summary...")
        summary = generate_daily_summary()
        return JSONResponse(content={
            "success": True,
            "summary": summary
        })
    except Exception as e:
        print(f"[Server] Summary error: {e}")
        return JSONResponse(
            status_code=500,
            content={
                "success": False,
                "error": str(e)
            }
        )
