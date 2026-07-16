import sys
import os
import uuid
import shutil
from datetime import datetime

# Add the current directory to sys.path to ensure modular imports work correctly
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from fastapi import FastAPI, UploadFile, File, BackgroundTasks, Request
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


def run_upload_pipeline(audio_id: str, temp_filepath: str, temp_filename: str):
    try:
        # 1. Upload the audio file to AWS S3
        print(f"\n[Background] Uploading to AWS S3 for audio_id: {audio_id}...")
        upload_result = upload_file(temp_filepath)
        s3_key = upload_result["s3_key"]
        audio_url = upload_result["audio_url"]
        print(f"[Background] Uploaded to S3. Key: {s3_key}")

        # Update initial metadata document with S3 key and URL
        from database.mongodb import collection
        collection.update_one(
            {"audio_id": audio_id},
            {"$set": {
                "s3_key": s3_key,
                "audio_url": audio_url,
                "status": "uploaded"
            }}
        )

        # 2. Transcribe audio via transcription service (downloads from S3 & runs Whisper)
        print("[Background] Transcribing audio...")
        transcript = process_audio(audio_id, s3_key)
        print(f"[Background] Transcription completed: \"{transcript}\"")

        # 3. LLM Analysis and parsing
        print("[Background] Processing transcript with LLM...")
        structured_data = process_transcript(transcript, ACTIVE_MODEL)

        # 4. Save final LLM details to MongoDB
        update_llm_result(audio_id, structured_data, ACTIVE_MODEL)
        print("[Background] Updated MongoDB with LLM structured result")

        # 5. Extract and save reminders
        print("[Background] Processing reminders...")
        process_reminders(audio_id, transcript, structured_data)

        # 6. Formulate and store memory in ChromaDB
        print("[Background] Storing memory to vector database...")
        memory_text = build_memory_text(transcript, structured_data)
        add_memory(
            audio_id,
            memory_text,
            {"category": structured_data.get("category", "Other")}
        )
        print(f"[Background] Pipeline completed successfully for audio_id: {audio_id}!")

    except Exception as e:
        print(f"[Background] ERROR: failed to process audio pipeline for {audio_id}: {e}")
        from database.mongodb import collection
        collection.update_one(
            {"audio_id": audio_id},
            {"$set": {
                "status": "error",
                "error": str(e)
            }}
        )
    finally:
        # Clean up local temporary file to conserve disk space
        if os.path.exists(temp_filepath):
            try:
                os.remove(temp_filepath)
                print(f"[Background] Deleted local temporary file: {temp_filepath}")
            except Exception as ex:
                print(f"[Background] Warning: failed to delete {temp_filepath}: {ex}")


@app.post("/api/voice/upload")
async def upload_voice(
    request: Request,
    background_tasks: BackgroundTasks,
    file: UploadFile = File(...)
):
    print("=" * 60)
    print("UPLOAD REQUEST RECEIVED")
    print("Method:", request.method)
    print("Client:", request.client)
    print("Headers:")
    for header, value in request.headers.items():
        print(f"  {header}: {value}")
    print("Filename:", file.filename)
    print("ContentType:", file.content_type)
    print("=" * 60)
    sys.stdout.flush()
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

        # Store initial metadata with "processing" status in MongoDB
        audio_id = str(uuid.uuid4())
        document = {
            "audio_id": audio_id,
            "filename": temp_filename,
            "status": "processing",
            "created_at": datetime.utcnow()
        }
        save_audio_metadata(document)
        print(f"[Server] Saved initial metadata in MongoDB. Audio ID: {audio_id}")

        # Queue the heavy work in FastAPI background tasks
        background_tasks.add_task(run_upload_pipeline, audio_id, temp_filepath, temp_filename)
        print(f"[Server] Queued processing pipeline for audio_id: {audio_id}")

        # Respond immediately to client
        return JSONResponse(content={
            "success": True,
            "audio_id": audio_id,
            "status": "processing"
        })

    except Exception as e:
        print(f"[Server] ERROR: failed to handle upload request: {e}")
        if os.path.exists(temp_filepath):
            try:
                os.remove(temp_filepath)
            except:
                pass
        return JSONResponse(
            status_code=500,
            content={
                "success": False,
                "error": str(e)
            }
        )


if __name__ == "__main__":
    import uvicorn
    # Start uvicorn server on port 8000
    print("[Server] Starting VOXA FastAPI server...")
    uvicorn.run("server:app", host="0.0.0.0", port=8000, reload=True)
