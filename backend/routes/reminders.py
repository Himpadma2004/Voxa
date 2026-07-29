from fastapi import APIRouter, Query, Request
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
    request: Request,
    skip: int = Query(0, ge=0),
    limit: int = Query(20, ge=1, le=200)
):
    """
    Fetch all calendar reminders from MongoDB.
    """
    try:
        ip = request.client.host
        set_esp32_ip(ip)
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


from fastapi import Request
from pydantic import BaseModel
import time
from database.mongodb import db
from services.reminder_scheduler import set_esp32_ip

reminders_collection = db["reminders"]

class SnoozeRequest(BaseModel):
    minutes: int

class RescheduleRequest(BaseModel):
    reminder_time: int  # timestamp

@router.post("/register")
def register_esp32(request: Request):
    ip = request.client.host
    set_esp32_ip(ip)
    return {"success": True, "registered_ip": ip}

@router.get("/active")
def get_active_reminders(request: Request):
    ip = request.client.host
    set_esp32_ip(ip)
    
    try:
        active = list(reminders_collection.find({"status": "active"}))
        items = []
        for index, reminder in enumerate(active):
            items.append({
                "id": reminder.get("reminder_id"),
                "title": reminder.get("title", ""),
                "description": reminder.get("description", "") or reminder.get("comments", "") or "",
                "reminderTime": int(reminder.get("reminder_time").timestamp()) if reminder.get("reminder_time") else int(time.time()),
                "status": "active"
            })
        return {"success": True, "items": items}
    except Exception as e:
        return JSONResponse(status_code=500, content={"success": False, "error": str(e)})

@router.post("/{reminder_id}/dismiss")
def dismiss_reminder(reminder_id: str):
    try:
        res = reminders_collection.update_one(
            {"reminder_id": reminder_id},
            {"$set": {"status": "completed", "completed_at": datetime.utcnow()}}
        )
        if res.modified_count > 0:
            print("[Scheduler] Reminder completed", flush=True)
            return {"success": True}
        return {"success": False, "error": "Reminder not found"}
    except Exception as e:
        return JSONResponse(status_code=500, content={"success": False, "error": str(e)})

@router.post("/{reminder_id}/snooze")
def snooze_reminder(reminder_id: str, req: SnoozeRequest):
    try:
        snooze_until = datetime.utcnow() + timedelta(minutes=req.minutes)
        res = reminders_collection.update_one(
            {"reminder_id": reminder_id},
            {"$set": {"status": "snoozed", "snooze_until": snooze_until}}
        )
        if res.modified_count > 0:
            print(f"[Scheduler] Reminder snoozed for {req.minutes} minutes", flush=True)
            return {"success": True}
        return {"success": False, "error": "Reminder not found"}
    except Exception as e:
        return JSONResponse(status_code=500, content={"success": False, "error": str(e)})

@router.post("/{reminder_id}/reschedule")
def reschedule_reminder(reminder_id: str, req: RescheduleRequest):
    try:
        new_time = datetime.utcfromtimestamp(req.reminder_time)
        res = reminders_collection.update_one(
            {"reminder_id": reminder_id},
            {"$set": {"status": "pending", "reminder_time": new_time, "snooze_until": None}}
        )
        if res.modified_count > 0:
            print(f"[Scheduler] Reminder rescheduled to {new_time}", flush=True)
            return {"success": True}
        return {"success": False, "error": "Reminder not found"}
    except Exception as e:
        return JSONResponse(status_code=500, content={"success": False, "error": str(e)})
