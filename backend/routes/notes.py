from datetime import datetime
from bson import ObjectId
from fastapi import APIRouter, HTTPException, Query

from fastapi.responses import JSONResponse
from database.mongodb import (
    collection,
    ideas_collection,
    questions_collection,
    tasks_collection,
    others_collection,
    reminders_collection,
    get_audio_by_id,
    get_all_notes
)


router = APIRouter(prefix="/api/notes", tags=["Notes"])


def serialize_mongo_doc(data):
    if isinstance(data, list):
        return [serialize_mongo_doc(item) for item in data]
    elif isinstance(data, dict):
        return {key: serialize_mongo_doc(val) for key, val in data.items()}
    elif hasattr(data, "isoformat"):
        return data.isoformat()
    elif data.__class__.__name__ == "ObjectId":
        return str(data)
    else:
        return data


MONTH_MAP = {
    "jan": 1, "january": 1,
    "feb": 2, "february": 2,
    "mar": 3, "march": 3,
    "apr": 4, "april": 4,
    "may": 5,
    "jun": 6, "june": 6,
    "jul": 7, "july": 7,
    "aug": 8, "august": 8,
    "sep": 9, "september": 9,
    "oct": 10, "october": 10,
    "nov": 11, "november": 11,
    "dec": 12, "december": 12
}


def parse_to_datetime(val, fallback_obj_id=None) -> datetime:
    if isinstance(val, datetime):
        return val

    if fallback_obj_id is not None and hasattr(fallback_obj_id, "generation_time"):
        default_fallback = fallback_obj_id.generation_time
    else:
        default_fallback = datetime.min

    if val is None or not str(val).strip():
        return default_fallback

    val_str = str(val).strip()

    # 1. Try ISO standard formats
    iso_str = val_str.replace("Z", "+00:00")
    try:
        return datetime.fromisoformat(iso_str).replace(tzinfo=None)
    except Exception:
        pass

    # 2. Try common formats
    for fmt in (
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%d %H:%M",
        "%Y-%m-%d",
        "%Y/%m/%d %H:%M:%S",
        "%Y/%m/%d %H:%M",
        "%d-%m-%Y %H:%M:%S",
        "%d-%m-%Y %H:%M",
        "%b %d, %Y, %I:%M %p",
        "%B %d, %Y, %I:%M %p",
        "%b %d, %I:%M %p",
        "%B %d, %I:%M %p",
    ):
        try:
            dt = datetime.strptime(val_str, fmt)
            if dt.year == 1900:
                dt = dt.replace(year=datetime.now().year)
            return dt
        except Exception:
            pass

    # 3. Regex for "Mon Day, [Year,] HH:MM AM/PM"
    import re
    m = re.match(r"^([A-Za-z]+)\s+(\d{1,2})(?:,\s*(\d{4}))?,\s*(\d{1,2}):(\d{2})(?:\s*([APap][Mm]))?", val_str)
    if m:
        mon_str, day_str, year_str, hr_str, min_str, ampm_str = m.groups()
        mon = MONTH_MAP.get(mon_str.lower(), 1)
        day = int(day_str)
        year = int(year_str) if year_str else datetime.now().year
        hr = int(hr_str)
        minute = int(min_str)
        if ampm_str:
            if ampm_str.upper() == "PM" and hr < 12:
                hr += 12
            elif ampm_str.upper() == "AM" and hr == 12:
                hr = 0
        try:
            return datetime(year, mon, day, hr, minute)
        except Exception:
            pass

    # 4. Unix timestamp
    try:
        ts = float(val_str)
        if ts > 1000000000000:
            ts /= 1000.0
        if ts > 100000:
            return datetime.fromtimestamp(ts)
    except Exception:
        pass

    return default_fallback


def _sort_key(record):
    val = (
        record.get("created_at") or
        record.get("processed_at") or
        record.get("timestamp") or
        record.get("dateTime") or
        record.get("reminder_time")
    )
    fallback_id = record.get("_id")
    return parse_to_datetime(val, fallback_id)


def _safe_str(value):
    if value is None:
        return ""
    elif hasattr(value, "isoformat"):
        return value.isoformat()
    else:
        return str(value)


def _build_category_items(docs, default_category):
    items = []
    for doc in docs:
        raw_time = doc.get("created_at") or doc.get("processed_at") or doc.get("timestamp")
        if not raw_time and "_id" in doc and hasattr(doc["_id"], "generation_time"):
            raw_time = doc["_id"].generation_time
        created_at = _safe_str(raw_time)

        # source_id: prefer audio_id (UUID string), fall back to item_id, then serialized _id
        audio_id = _safe_str(doc.get("audio_id") or "")
        item_id = _safe_str(doc.get("item_id") or "")
        mongo_id = _safe_str(doc.get("_id") or "")
        # Use audio_id if present, else item_id, else mongo ObjectId string
        source_id = audio_id or item_id or mongo_id
        title = doc.get("title") or doc.get("summary") or doc.get("content") or ""
        content = doc.get("content") or doc.get("title") or ""
        status = str(doc.get("status", "")).lower()
        is_completed = (status == "completed") or (doc.get("completed") is True)

        items.append({
            "id": len(items) + 1,
            "title": title,
            "content": content,
            "timestamp": created_at,
            "source_id": source_id,   # primary toggle key (audio_id UUID or item_id UUID)
            "mongo_id": mongo_id,      # raw ObjectId string — use as fallback toggle key
            "category": doc.get("category", default_category),
            "status": "completed" if is_completed else "pending",
            "completed": is_completed,
            "comments": ""
        })
    return items



from fastapi.responses import JSONResponse, StreamingResponse
from services.s3_service import s3_client, AWS_BUCKET_NAME

def _build_recording_items(notes):
    items = []
    for note in notes:
        # Prefer original recording timestamp
        raw_time = note.get("created_at") or note.get("processed_at") or note.get("timestamp")
        if not raw_time and "_id" in note and hasattr(note["_id"], "generation_time"):
            raw_time = note["_id"].generation_time
        created_at = _safe_str(raw_time)
        title = note.get("summary") or note.get("filename") or note.get("audio_id") or "Untitled recording"
        audio_id_str = _safe_str(note.get("audio_id") or note.get("_id"))
        file_path = f"/api/audio/{audio_id_str}"
        status = note.get("status", "")

        items.append({
            "id": len(items) + 1,
            "title": title,
            "filePath": file_path,
            "durationSeconds": 0,
            "timestamp": created_at if status != "processing" else "Pending",
            "audio_id": audio_id_str,
            "status": status,
            "category": note.get("category", "")
        })

    return items


@router.get("/audio/{audio_id}")
@router.get("/audio/{audio_id}.wav")
def stream_audio_notes(audio_id: str):
    """
    Streams audio recording from AWS S3 directly to the ESP32 / client.
    """
    try:
        from database.mongodb import collection
        note = collection.find_one({"$or": [{"audio_id": audio_id}, {"filename": audio_id}, {"s3_key": audio_id}]})
        s3_key = None
        if note:
            s3_key = note.get("s3_key")
        if not s3_key:
            if audio_id.startswith("audio/"):
                s3_key = audio_id
            else:
                s3_key = f"audio/{audio_id}"

        print(f"[AudioStream] Streaming S3 object: {s3_key}")
        s3_obj = s3_client.get_object(Bucket=AWS_BUCKET_NAME, Key=s3_key)
        headers = {
            "Content-Type": "audio/wav",
            "Accept-Ranges": "bytes"
        }
        if "ContentLength" in s3_obj:
            headers["Content-Length"] = str(s3_obj["ContentLength"])

        return StreamingResponse(
            s3_obj["Body"],
            media_type="audio/wav",
            headers=headers
        )
    except Exception as e:
        print(f"[AudioStream] Error streaming audio {audio_id}: {e}")
        raise HTTPException(status_code=404, detail=f"Audio not found: {e}")


def _page(items, skip, limit):
    total = len(items)
    page_items = items[skip: skip + limit]
    for index, item in enumerate(page_items):
        item["id"] = skip + index + 1
    return page_items, total


@router.get("")
def read_all_notes(
    category: str = Query("recordings"),
    skip: int = Query(0, ge=0),
    limit: int = Query(20, ge=1, le=200)
):
    """
    Fetch all note records from their dedicated MongoDB category collections.
    """
    try:
        category_key = (category or "recordings").strip().lower()

        if category_key == "ideas":
            raw_docs = list(ideas_collection.find())
            raw_docs.sort(key=_sort_key, reverse=True)
            items = _build_category_items(serialize_mongo_doc(raw_docs), "Idea")

        elif category_key == "questions":
            raw_docs = list(questions_collection.find())
            raw_docs.sort(key=_sort_key, reverse=True)
            items = _build_category_items(serialize_mongo_doc(raw_docs), "Question")

        elif category_key in ("tasks", "task"):
            raw_docs = list(tasks_collection.find())
            raw_docs.sort(key=_sort_key, reverse=True)
            items = _build_category_items(serialize_mongo_doc(raw_docs), "Task")

        elif category_key in ("reminders", "reminder"):
            raw_docs = list(reminders_collection.find())
            raw_docs.sort(key=_sort_key, reverse=True)
            items = _build_category_items(serialize_mongo_doc(raw_docs), "Reminder")

        elif category_key in ("others", "other", "notes"):
            raw_docs = list(others_collection.find())
            raw_docs.sort(key=_sort_key, reverse=True)
            items = _build_category_items(serialize_mongo_doc(raw_docs), "Other")

        else:
            raw_docs = list(collection.find())
            raw_docs.sort(key=_sort_key, reverse=True)
            items = _build_recording_items(serialize_mongo_doc(raw_docs))

        page_items, total = _page(items, skip, limit)

        return JSONResponse(content={
            "success": True,
            "category": category_key,
            "items": page_items,
            "notes": page_items,
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


@router.patch("/{source_id}")
@router.post("/{source_id}/toggle")
def toggle_note_status(
    source_id: str,
    category: str = Query(None, description="Optional: narrow search to a specific category (tasks, ideas, questions, reminders, others)")
):
    """
    Toggle or update the completion status of a task, reminder, idea, question, or note in MongoDB instantly.
    Handles string IDs (e.g. ObjectIds, UUIDs) and numeric string IDs (e.g. "1", "2").
    When 'category' is provided, only that collection is searched — avoiding cross-collection index collisions.
    """
    try:
        # Map friendly category names to their MongoDB collections
        category_map = {
            "tasks": ("tasks", tasks_collection),
            "task": ("tasks", tasks_collection),
            "ideas": ("ideas", ideas_collection),
            "idea": ("ideas", ideas_collection),
            "questions": ("questions", questions_collection),
            "question": ("questions", questions_collection),
            "reminders": ("reminders", reminders_collection),
            "reminder": ("reminders", reminders_collection),
            "others": ("others", others_collection),
            "other": ("others", others_collection),
            "memories": ("others", others_collection),
            "notes": ("notes", collection),
        }

        int_id = None
        try:
            int_id = int(source_id)
        except ValueError:
            pass

        # Build query conditions: try all possible ID fields the document could store
        # CRITICAL: MongoDB _id is a BSON ObjectId, NOT a plain string — must convert!
        query_conditions = [
            {"audio_id": source_id},    # UUID string stored in audio_id field
            {"item_id": source_id},     # UUID string stored in item_id field
            {"source_id": source_id},   # fallback source_id field
            {"id": source_id},          # string id field
        ]

        # Try ObjectId conversion for _id field (the only way to match MongoDB _id)
        try:
            oid = ObjectId(source_id)
            query_conditions.insert(0, {"_id": oid})  # highest priority
        except Exception:
            pass  # source_id is not a valid ObjectId hex string

        if int_id is not None:
            query_conditions.extend([{"id": int_id}])

        query = {"$or": query_conditions}

        # If category is specified, ONLY search that collection to prevent cross-collection index collisions
        if category and category.lower() in category_map:
            col_name, col = category_map[category.lower()]
            target_collections = [(col_name, col)]
            print(f"[MongoDB] Toggle scoped to category='{category}' -> collection='{col_name}'", flush=True)
        else:
            # Search all collections in priority order: tasks -> ideas -> questions -> others -> reminders -> audio notes
            target_collections = [
                ("tasks", tasks_collection),
                ("ideas", ideas_collection),
                ("questions", questions_collection),
                ("others", others_collection),
                ("reminders", reminders_collection),
                ("notes", collection)
            ]

        def _toggle_doc(col_name, col, doc):
            current_status = str(doc.get("status", "")).lower()
            current_completed = doc.get("completed") is True or current_status in ("completed", "answered", "cleared", "dismissed")
            new_completed = not current_completed
            new_status = "completed" if new_completed else "pending"
            col.update_one(
                {"_id": doc["_id"]},
                {"$set": {
                    "status": new_status,
                    "completed": new_completed,
                    "answered": new_completed if col_name == "questions" else doc.get("answered", False),
                    "updated_at": datetime.utcnow()
                }}
            )
            return new_completed, new_status

        # Phase 1: Try matching by source_id / audio_id / UUID string / ObjectId
        for col_name, col in target_collections:
            doc = col.find_one(query)
            if doc:
                new_completed, new_status = _toggle_doc(col_name, col, doc)
                print(f"[MongoDB] Instantly toggled {col_name} document ID '{source_id}' -> completed: {new_completed}, status: '{new_status}'", flush=True)
                return {"success": True, "category": col_name, "status": new_status, "completed": new_completed}

        # Phase 2: Fallback — match by 1-based positional index within the specific category (or all categories)
        if int_id is not None and int_id > 0:
            for col_name, col in target_collections:
                all_docs = list(col.find().sort("created_at", -1))  # Sort newest-first, same order as ESP32 sync
                if 0 < int_id <= len(all_docs):
                    doc = all_docs[int_id - 1]
                    new_completed, new_status = _toggle_doc(col_name, col, doc)
                    print(f"[MongoDB] Instantly toggled {col_name} (index {int_id}) document ID '{doc['_id']}' -> completed: {new_completed}, status: '{new_status}'", flush=True)
                    return {"success": True, "category": col_name, "status": new_status, "completed": new_completed}

        return JSONResponse(status_code=404, content={"success": False, "error": f"Item '{source_id}' not found in {'category: ' + category if category else 'any MongoDB collection'}"})
    except Exception as e:
        print(f"[MongoDB Error] Failed to toggle note status: {e}", flush=True)
        return JSONResponse(status_code=500, content={"success": False, "error": str(e)})



@router.get("/{audio_id}")
def read_note_by_id(audio_id: str):
    """
    Fetch a specific processed audio note and its AI response details from MongoDB.
    This corresponds to the ESP32 fetching the completed AI response.
    """
    try:
        note = get_audio_by_id(audio_id)
        if not note:
            raise HTTPException(status_code=404, detail="Audio note not found")
        
        serialized_note = serialize_mongo_doc(note)
        return JSONResponse(content={"success": True, "note": serialized_note})
    except HTTPException as he:
        raise he
    except Exception as e:
        return JSONResponse(
            status_code=500,
            content={"success": False, "error": str(e)}
        )

