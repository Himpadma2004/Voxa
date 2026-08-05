from fastapi import APIRouter, HTTPException, Query
from fastapi.responses import JSONResponse
from database.mongodb import (
    collection,
    ideas_collection,
    questions_collection,
    tasks_collection,
    others_collection,
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


def _sort_key(record):
    return _safe_str(record.get("created_at") or record.get("processed_at") or record.get("timestamp") or "")


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
        created_at = _safe_str(doc.get("created_at") or doc.get("processed_at") or doc.get("timestamp"))
        source_id = _safe_str(doc.get("audio_id") or doc.get("_id"))
        title = doc.get("title") or doc.get("summary") or doc.get("content") or ""
        content = doc.get("content") or doc.get("title") or ""

        items.append({
            "id": len(items) + 1,
            "title": title,
            "content": content,
            "timestamp": created_at,
            "source_id": source_id,
            "category": doc.get("category", default_category),
            "status": doc.get("status", ""),
            "comments": ""
        })
    return items


def _build_recording_items(notes):
    items = []
    for note in notes:
        created_at = _safe_str(note.get("processed_at") or note.get("created_at") or note.get("timestamp"))
        title = note.get("summary") or note.get("filename") or note.get("audio_id") or "Untitled recording"
        file_path = note.get("audio_url") or note.get("s3_key") or note.get("filename") or ""
        status = note.get("status", "")

        items.append({
            "id": len(items) + 1,
            "title": title,
            "filePath": file_path,
            "durationSeconds": 0,
            "timestamp": created_at if status != "processing" else "Pending",
            "audio_id": _safe_str(note.get("audio_id") or note.get("_id")),
            "status": status,
            "category": note.get("category", "")
        })

    return items


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
    except HTTPException as he:
        raise he
    except Exception as e:
        return JSONResponse(
            status_code=500,
            content={"success": False, "error": str(e)}
        )
