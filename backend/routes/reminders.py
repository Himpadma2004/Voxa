from fastapi import APIRouter, Query
from fastapi.responses import JSONResponse
from reminders.reminder_repository import load_all_reminders

router = APIRouter(prefix="/api/reminders", tags=["Reminders"])


def _serialize_value(value):
    if hasattr(value, "isoformat"):
        return value.isoformat()
    elif isinstance(value, list):
        return [_serialize_value(item) for item in value]
    elif isinstance(value, dict):
        return {key: _serialize_value(val) for key, val in value.items()}
    else:
        return value


def _normalize_reminder(reminder, index):
    reminder_time = reminder.get("reminder_time") or reminder.get("dateTime") or reminder.get("timestamp")
    created_at = reminder.get("created_at")
    status = str(reminder.get("status", "pending")).lower()

    return {
        "id": index + 1,
        "title": reminder.get("title", ""),
        "dateTime": _serialize_value(reminder_time) if reminder_time else "",
        "completed": status == "completed",
        "comments": reminder.get("comments", ""),
        "created_at": _serialize_value(created_at) if created_at else "",
        "audio_id": reminder.get("audio_id", ""),
        "status": reminder.get("status", "pending"),
    }


@router.get("")
def read_all_reminders(
    skip: int = Query(0, ge=0),
    limit: int = Query(20, ge=1, le=200)
):
    """
    Fetch all calendar reminders from MongoDB.
    """
    try:
        reminders = load_all_reminders()
        reminders.sort(
            key=lambda r: _serialize_value(r.get("created_at") or r.get("reminder_time") or r.get("_id") or ""),
            reverse=True
        )

        total = len(reminders)
        page_items = reminders[skip: skip + limit]
        items = [_normalize_reminder(reminder, skip + index) for index, reminder in enumerate(page_items)]

        return JSONResponse(content={
            "success": True,
            "items": items,
            "reminders": items,
            "page": {
                "skip": skip,
                "limit": limit,
                "total": total,
                "has_more": (skip + limit) < total,
            }
        })
    except Exception as e:
        return JSONResponse(
            status_code=500,
            content={"success": False, "error": str(e)}
        )
