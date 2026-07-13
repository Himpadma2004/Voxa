from fastapi import APIRouter
from fastapi.responses import JSONResponse
from reminders.reminder_repository import load_all_reminders

router = APIRouter(prefix="/api/reminders", tags=["Reminders"])


@router.get("")
def read_all_reminders():
    """
    Fetch all calendar reminders from MongoDB.
    """
    try:
        reminders = load_all_reminders()
        # Convert ObjectId and datetime to string for JSON serialization
        for r in reminders:
            if "_id" in r:
                r["_id"] = str(r["_id"])
            if "reminder_time" in r and r["reminder_time"]:
                if hasattr(r["reminder_time"], "isoformat"):
                    r["reminder_time"] = r["reminder_time"].isoformat()
                else:
                    r["reminder_time"] = str(r["reminder_time"])
            if "created_at" in r and r["created_at"]:
                if hasattr(r["created_at"], "isoformat"):
                    r["created_at"] = r["created_at"].isoformat()
                else:
                    r["created_at"] = str(r["created_at"])

        return JSONResponse(content={"success": True, "reminders": reminders})
    except Exception as e:
        return JSONResponse(
            status_code=500,
            content={"success": False, "error": str(e)}
        )
