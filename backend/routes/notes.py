from fastapi import APIRouter, HTTPException
from fastapi.responses import JSONResponse
from database.mongodb import get_audio_by_id, get_all_notes

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


@router.get("")
def read_all_notes():
    """
    Fetch all audio note records from MongoDB.
    """
    try:
        notes = get_all_notes()
        serialized_notes = serialize_mongo_doc(notes)
        return JSONResponse(content={"success": True, "notes": serialized_notes})
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
