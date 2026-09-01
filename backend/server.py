import sys
import os
import uuid
import shutil
import tempfile
from datetime import datetime

# Add the current directory to sys.path to ensure modular imports work correctly
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from fastapi import FastAPI, UploadFile, File, BackgroundTasks, Request
from fastapi.responses import JSONResponse
from fastapi.concurrency import run_in_threadpool
from fastapi.middleware.cors import CORSMiddleware

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

# Enable CORS for web applications
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.middleware("http")
async def log_requests(request: Request, call_next):
    # Only log meaningful API requests (silence noisy static/polling if needed)
    path = request.url.path
    response = await call_next(request)
    if response.status_code >= 400:
        print(f"[HTTP {response.status_code}] {request.method} {path} from {request.client.host if request.client else 'unknown'}")
    elif path.startswith("/api/voice") or path.startswith("/api/audio"):
        print(f"[HTTP] {request.method} {path} -> {response.status_code}")
    return response

@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    import traceback
    print(f"\n❌ [Server Error 500] {request.method} {request.url.path}: {exc}")
    traceback.print_exc()
    return JSONResponse(
        status_code=500,
        content={"success": False, "error": str(exc)}
    )

# Include modular routes/endpoints
from routes import notes, reminders, search, summary, music
app.include_router(notes.router)
app.include_router(reminders.router)
app.include_router(search.router)
app.include_router(summary.router)
app.include_router(music.router)

from fastapi.responses import StreamingResponse
from services.s3_service import s3_client, AWS_BUCKET_NAME

@app.get("/api/audio/{audio_id}")
@app.get("/api/audio/{audio_id}.wav")
def stream_audio_direct(audio_id: str):
    """
    Directly streams audio from AWS S3 to the ESP32 speaker.
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

        print(f"[Server AudioStream] Streaming S3 object: {s3_key}")
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
        print(f"[Server AudioStream] Error streaming {audio_id}: {e}")
        from fastapi import HTTPException
        raise HTTPException(status_code=404, detail=f"Audio not found: {e}")



@app.get("/")
def read_root():
    return {"status": "online", "service": "VOXA API Backend"}


@app.on_event("startup")
def startup_event():
    try:
        from services.reminder_scheduler import start_scheduler
        start_scheduler()
    except Exception as e:
        print(f"[Startup] Warning starting reminder scheduler: {e}")
    try:
        from database.mongodb import migrate_all_existing_data
        migrate_all_existing_data()
    except Exception as e:
        print(f"[Startup] Warning migrating data: {e}")


def run_upload_pipeline(audio_id: str, temp_filepath: str, temp_filename: str):
    try:
        # 0. Standardize and normalize recorded audio to 16kHz 16-bit Mono PCM WAV (matches ESP32 mic and Whisper AI)
        try:
            import soundfile as sf
            import scipy.signal
            import numpy as np

            audio_data, sr = sf.read(temp_filepath)
            if audio_data.ndim > 1:
                audio_data = audio_data.mean(axis=1) # Mono
            target_sr = 16000
            if sr != target_sr:
                num_samples = int(len(audio_data) * target_sr / sr)
                audio_data = scipy.signal.resample(audio_data, num_samples)
            peak = np.max(np.abs(audio_data))
            if peak > 0:
                audio_data = (audio_data / peak) * 0.95
            int16_data = (audio_data * 32767).astype(np.int16)
            sf.write(temp_filepath, int16_data, target_sr, subtype="PCM_16", format="WAV")
        except Exception as resample_err:
            print(f"[Warning] Audio normalization note: {resample_err}")

        # 1. Upload the standardized 16kHz audio file to AWS S3
        upload_result = upload_file(temp_filepath)
        s3_key = upload_result["s3_key"]
        audio_url = upload_result["audio_url"]

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
        print(f"\n========================================================")
        print(f"🎙️ [AUDIO RECEIVED] audio_id: {audio_id}")
        transcript = process_audio(audio_id, s3_key)
        print(f"📝 [TRANSCRIPT] \"{transcript}\"")

        if not transcript or not transcript.strip():
            print(f"⚠️ [EMPTY TRANSCRIPT] No speech detected. Automatically discarding empty recording.")
            print(f"========================================================\n")
            from database.mongodb import collection
            collection.delete_one({"audio_id": audio_id})
            return

        # 3. LLM Analysis and parsing
        structured_data = process_transcript(transcript, ACTIVE_MODEL)
        print(f"🤖 [ANALYSIS] Category: {structured_data.get('category')} | Summary: {structured_data.get('summary')}")
        print(f"========================================================\n")

        # 4. Save final LLM details to MongoDB
        update_llm_result(audio_id, structured_data, ACTIVE_MODEL)

        # 5. Extract and save reminders
        process_reminders(audio_id, transcript, structured_data)

        # 6. Formulate and store memory in ChromaDB
        memory_text = build_memory_text(transcript, structured_data)
        add_memory(
            audio_id,
            memory_text,
            {"category": structured_data.get("category", "Other")}
        )

    except Exception as e:
        print(f"❌ [PIPELINE ERROR] {audio_id}: {e}")
        from database.mongodb import collection
        collection.update_one(
            {"audio_id": audio_id},
            {"$set": {
                "status": "error",
                "error": str(e)
            }}
        )
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


def extract_record_time(request: Request) -> datetime:
    """
    Extract the original recording timestamp provided by the client (ESP32/browser/app),
    or fall back to the current local datetime.
    """
    header_val = (
        request.headers.get("x-recorded-at") or
        request.headers.get("x-timestamp") or
        request.headers.get("x-created-at") or
        request.query_params.get("recorded_at") or
        request.query_params.get("timestamp")
    )
    if header_val and header_val.strip():
        val = header_val.strip()
        try:
            return datetime.fromisoformat(val.replace("Z", "+00:00"))
        except Exception:
            try:
                return datetime.fromtimestamp(float(val))
            except Exception:
                pass
    return datetime.now()


@app.post("/api/voice/upload")
async def upload_voice(
    request: Request,
    background_tasks: BackgroundTasks,
    file: UploadFile = File(...)
):
    print("=== upload_voice ENTERED ===")
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

    rec_time = extract_record_time(request)

    # Ensure recordings temp directory exists outside watched paths to avoid Uvicorn reload loops
    temp_dir = os.path.join(tempfile.gettempdir(), "voxa_recordings")
    os.makedirs(temp_dir, exist_ok=True)
    temp_filename = f"{uuid.uuid4()}.wav"
    temp_filepath = os.path.join(temp_dir, temp_filename)

    try:
        # Save uploaded stream into local temporary WAV file
        def save_file():
            with open(temp_filepath, "wb") as buffer:
                shutil.copyfileobj(file.file, buffer)
        await run_in_threadpool(save_file)
        print(f"[Server] Saved uploaded file to temporary path: {temp_filepath}")

        # Store initial metadata with "processing" status and accurate recording timestamp in MongoDB
        audio_id = str(uuid.uuid4())
        document = {
            "audio_id": audio_id,
            "filename": temp_filename,
            "status": "processing",
            "created_at": rec_time
        }
        await run_in_threadpool(save_audio_metadata, document)
        print(f"[Server] Saved initial metadata in MongoDB. Audio ID: {audio_id}, Recorded At: {rec_time}")

        # Queue the heavy work in FastAPI background tasks
        background_tasks.add_task(run_upload_pipeline, audio_id, temp_filepath, temp_filename)
        print(f"[Server] Queued processing pipeline for audio_id: {audio_id}")

        # Respond immediately to client
        return JSONResponse(content={
            "success": True,
            "audio_id": audio_id,
            "text": "Processing...",
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

@app.post("/api/voice/upload-raw")
async def upload_voice_raw(
    request: Request,
    background_tasks: BackgroundTasks
):
    rec_time = extract_record_time(request)

    # Ensure recordings temp directory exists outside watched paths to avoid Uvicorn reload loops
    temp_dir = os.path.join(tempfile.gettempdir(), "voxa_recordings")
    os.makedirs(temp_dir, exist_ok=True)
    temp_filename = f"{uuid.uuid4()}.wav"
    temp_filepath = os.path.join(temp_dir, temp_filename)

    try:
        # Read the entire raw WAV body at once
        raw_body = await request.body()
        file_size = len(raw_body)
        print(f"[Upload] Received raw audio stream ({file_size} bytes)")

        if file_size == 0:
            return JSONResponse(
                status_code=400,
                content={"success": False, "error": "Empty request body"}
            )

        # Write body to disk
        def write_file():
            with open(temp_filepath, "wb") as f:
                f.write(raw_body)
        await run_in_threadpool(write_file)

        # Store initial metadata in MongoDB
        audio_id = str(uuid.uuid4())
        document = {
            "audio_id": audio_id,
            "filename": temp_filename,
            "status": "processing",
            "created_at": rec_time
        }
        await run_in_threadpool(save_audio_metadata, document)

        # Queue processing pipeline in FastAPI background tasks
        background_tasks.add_task(run_upload_pipeline, audio_id, temp_filepath, temp_filename)

        # Respond immediately to client
        return JSONResponse(content={
            "success": True,
            "audio_id": audio_id,
            "text": "Processing...",
            "status": "processing"
        })

    except Exception as e:
        print(f"❌ [Upload Error] {e}")
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
    # Start uvicorn server on port 8000 with clean, quiet logging
    print("\n🚀 [VOXA Server] Running on http://0.0.0.0:8000")
    uvicorn.run("server:app", host="0.0.0.0", port=8000, reload=False, log_level="warning")