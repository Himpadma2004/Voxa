from fastapi import APIRouter, HTTPException
from fastapi.responses import JSONResponse
from database.mongodb import get_audio_by_id, get_all_notes

router = APIRouter(prefix="/api/notes", tags=["Notes"])


@router.get("")
def read_all_notes():
    """
    Fetch all audio note records from MongoDB.
    """
    try:
        notes = get_all_notes()
        # Convert ObjectId to string for JSON serialization
        for note in notes:
            if "_id" in note:
                note["_id"] = str(note["_id"])
        return JSONResponse(content={"success": True, "notes": notes})
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
        
        if "_id" in note:
            note["_id"] = str(note["_id"])
        return JSONResponse(content={"success": True, "note": note})
    except HTTPException as he:
        raise he
    except Exception as e:
        return JSONResponse(
            status_code=500,
            content={"success": False, "error": str(e)}
        )
