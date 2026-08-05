from datetime import datetime, timedelta
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

    is_missed = False
    notice = ""
    now = datetime.utcnow()

    parsed_time = None
    if isinstance(reminder_time, datetime):
        parsed_time = reminder_time
    elif isinstance(reminder_time, str) and reminder_time.strip():
        try:
            parsed_time = datetime.fromisoformat(reminder_time.replace("Z", "+00:00"))
        except Exception:
            pass

    if parsed_time and parsed_time < now and status not in ("completed", "cleared", "dismissed"):
        is_missed = True
        status = "missed"
        notice = "This reminder was missed by you."

    return {
        "id": index + 1,
        "reminder_id": str(reminder.get("reminder_id") or reminder.get("_id") or (index + 1)),
        "title": reminder.get("title", ""),
        "dateTime": _serialize_value(reminder_time) if reminder_time else "",
        "completed": status in ("completed", "cleared", "dismissed"),
        "is_missed": is_missed,
        "notice": notice,
        "comments": notice if notice else reminder.get("comments", ""),
        "created_at": _serialize_value(created_at) if created_at else "",
        "audio_id": reminder.get("audio_id", ""),
        "status": status,
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

        # Update status in DB for any overdue reminders
        now = datetime.utcnow()
        for r in reminders:
            r_time = r.get("reminder_time") or r.get("dateTime")
            st = str(r.get("status", "pending")).lower()
            if isinstance(r_time, datetime) and r_time < now and st not in ("completed", "cleared", "dismissed"):
                reminders_collection.update_one(
                    {"_id": r["_id"]},
                    {"$set": {"status": "missed", "notice": "This reminder was missed by you."}}
                )
                r["status"] = "missed"
                r["notice"] = "This reminder was missed by you."

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
        active = list(reminders_collection.find({"status": {"$in": ["active", "pending", "missed"]}}))
        items = []
        now = datetime.utcnow()
        for index, reminder in enumerate(active):
            r_time = reminder.get("reminder_time")
            st = reminder.get("status", "pending")
            is_missed = False
            notice = ""
            if isinstance(r_time, datetime) and r_time < now and st != "completed":
                is_missed = True
                notice = "This reminder was missed by you."

            items.append({
                "id": reminder.get("reminder_id") or str(reminder.get("_id")),
                "title": reminder.get("title", ""),
                "description": notice if notice else (reminder.get("description", "") or reminder.get("comments", "") or ""),
                "reminderTime": int(r_time.timestamp()) if isinstance(r_time, datetime) else int(time.time()),
                "status": "missed" if is_missed else st,
                "is_missed": is_missed,
                "notice": notice
            })
        return {"success": True, "items": items}
    except Exception as e:
        return JSONResponse(status_code=500, content={"success": False, "error": str(e)})

@router.post("/{reminder_id}/dismiss")
@router.post("/{reminder_id}/clear")
@router.delete("/{reminder_id}")
def dismiss_reminder(reminder_id: str):
    try:
        res = reminders_collection.update_one(
            {"$or": [{"reminder_id": reminder_id}, {"_id": reminder_id}]},
            {"$set": {"status": "completed", "completed_at": datetime.utcnow()}}
        )
        if res.modified_count > 0 or res.matched_count > 0:
            print("[Scheduler] Reminder cleared/dismissed", flush=True)
            return {"success": True}
        return {"success": False, "error": "Reminder not found"}
    except Exception as e:
        return JSONResponse(status_code=500, content={"success": False, "error": str(e)})

@router.post("/{reminder_id}/snooze")
def snooze_reminder(reminder_id: str, req: SnoozeRequest):
    try:
        snooze_until = datetime.utcnow() + timedelta(minutes=req.minutes)
        res = reminders_collection.update_one(
            {"$or": [{"reminder_id": reminder_id}, {"_id": reminder_id}]},
            {"$set": {"status": "snoozed", "snooze_until": snooze_until}}
        )
        if res.modified_count > 0 or res.matched_count > 0:
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
            {"$or": [{"reminder_id": reminder_id}, {"_id": reminder_id}]},
            {"$set": {"status": "pending", "reminder_time": new_time, "snooze_until": None}}
        )
        if res.modified_count > 0 or res.matched_count > 0:
            print(f"[Scheduler] Reminder rescheduled to {new_time}", flush=True)
            return {"success": True}
        return {"success": False, "error": "Reminder not found"}
    except Exception as e:
        return JSONResponse(status_code=500, content={"success": False, "error": str(e)})
