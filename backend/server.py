import sys
import os
import uuid
import shutil
from datetime import datetime

# Add the current directory to sys.path to ensure modular imports work correctly
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from fastapi import FastAPI, UploadFile, File
from fastapi.responses import JSONResponse

# Import existing backend modules and services
from config.settings import ACTIVE_MODEL
from services.s3_service import upload_file
from database.mongodb import save_audio_metadata, update_llm_result
from services.transcription_service import process_audio
from services.processing_service import process_transcript
from reminders.reminder_service import process_reminders
from vector_db.memory_formatter import build_memory_text
from vector_db.chroma_client import add_memory

app = FastAPI(title="VOXA REST API Server")

# Include modular routes/endpoints
from routes import notes, reminders, search, summary
app.include_router(notes.router)
app.include_router(reminders.router)
app.include_router(search.router)
app.include_router(summary.router)



@app.get("/")
def read_root():
    return {"status": "online", "service": "VOXA API Backend"}


@app.post("/api/voice/upload")
async def upload_voice(file: UploadFile = File(...)):
    print(f"\n[Server] Received file upload request: {file.filename}")

    # Ensure recordings temp directory exists
    os.makedirs("recordings", exist_ok=True)
    temp_filename = f"{uuid.uuid4()}.wav"
    temp_filepath = os.path.join("recordings", temp_filename)

    try:
        # Save uploaded stream into local temporary WAV file
        with open(temp_filepath, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)
        print(f"[Server] Saved uploaded file to temporary path: {temp_filepath}")

        # 1. Upload the audio file to AWS S3
        print("[Server] Uploading to AWS S3...")
        upload_result = upload_file(temp_filepath)
        s3_key = upload_result["s3_key"]
        audio_url = upload_result["audio_url"]
        print(f"[Server] Uploaded to S3. Key: {s3_key}")

        # 2. Store initial audio metadata in MongoDB
        audio_id = str(uuid.uuid4())
        document = {
            "audio_id": audio_id,
            "filename": temp_filename,
            "s3_key": s3_key,
            "audio_url": audio_url,
            "status": "uploaded",
            "created_at": datetime.utcnow()
        }
        save_audio_metadata(document)
        print("[Server] Saved audio metadata in MongoDB")

        # 3. Transcribe audio via transcription service (downloads from S3 & runs Whisper)
        print("[Server] Transcribing audio...")
        transcript = process_audio(audio_id, s3_key)
        print(f"[Server] Transcription completed: \"{transcript}\"")

        # 4. LLM Analysis and parsing
        print("[Server] Processing transcript with LLM...")
        structured_data = process_transcript(transcript, ACTIVE_MODEL)

        # 5. Save final LLM details to MongoDB
        update_llm_result(audio_id, structured_data, ACTIVE_MODEL)
        print("[Server] Updated MongoDB with LLM structured result")

        # 6. Extract and save reminders
        print("[Server] Processing reminders...")
        process_reminders(audio_id, transcript, structured_data)

        # 7. Formulate and store memory in ChromaDB
        print("[Server] Storing memory to vector database...")
        memory_text = build_memory_text(transcript, structured_data)
        add_memory(
            audio_id,
            memory_text,
            {"category": structured_data.get("category", "Other")}
        )

        print("[Server] Request pipeline completed successfully!")
        return JSONResponse(content={
            "success": True,
            "text": transcript
        })

    except Exception as e:
        print(f"[Server] ERROR: failed to process upload request: {e}")
        return JSONResponse(
            status_code=500,
            content={
                "success": False,
                "error": str(e)
            }
        )

    finally:
        # Clean up local temporary file to conserve disk space
        if os.path.exists(temp_filepath):
            try:
                os.remove(temp_filepath)
                print(f"[Server] Deleted local temporary file: {temp_filepath}")
            except Exception as ex:
                print(f"[Server] Warning: failed to delete {temp_filepath}: {ex}")


if __name__ == "__main__":
    import uvicorn
    # Start uvicorn server on port 8000
    print("[Server] Starting VOXA FastAPI server...")
    uvicorn.run("server:app", host="0.0.0.0", port=8000, reload=True)
