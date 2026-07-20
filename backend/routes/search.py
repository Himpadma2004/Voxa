import os
import uuid
import shutil
import tempfile
from fastapi import APIRouter, Query, UploadFile, File, Request, Body
from fastapi.responses import JSONResponse
from fastapi.concurrency import run_in_threadpool
from vector_db.memory_recall import recall_memories
from ai.whisper_engine import transcribe_audio

router = APIRouter(prefix="/api/search", tags=["Search"])


@router.get("")
async def search_get(q: str = Query(..., description="The query string to search database memories for")):
    """
    Type Search (GET): Search database memories in MongoDB & ChromaDB using LLM.
    """
    try:
        print(f"\n[Search] Type Search (GET) for: \"{q}\"")
        answer = await run_in_threadpool(recall_memories, q)
        return JSONResponse(content={
            "success": True,
            "type": "text",
            "query": q,
            "answer": answer
        })
    except Exception as e:
        print(f"[Search] Error in GET search: {e}")
        return JSONResponse(
            status_code=500,
            content={
                "success": False,
                "type": "text",
                "error": str(e)
            }
        )


@router.post("")
async def search_post(request: Request):
    """
    Type Search (POST): Search database memories using JSON body `{"query": "when I need to call Jennifer"}`.
    """
    try:
        body = await request.json()
        query = body.get("query") or body.get("q") or ""
        if not query.strip():
            return JSONResponse(
                status_code=400,
                content={"success": False, "type": "text", "error": "Query string is required."}
            )

        print(f"\n[Search] Type Search (POST) for: \"{query}\"")
        answer = await run_in_threadpool(recall_memories, query)
        return JSONResponse(content={
            "success": True,
            "type": "text",
            "query": query,
            "answer": answer
        })
    except Exception as e:
        print(f"[Search] Error in POST search: {e}")
        return JSONResponse(
            status_code=500,
            content={
                "success": False,
                "type": "text",
                "error": str(e)
            }
        )


@router.post("/audio")
async def search_audio(file: UploadFile = File(...)):
    """
    Audio Search (Multipart File): Upload voice recording query (WAV/MP3/M4A/etc.),
    transcribe using Whisper, search database records using LLM, and return answer.
    """
    temp_dir = os.path.join(tempfile.gettempdir(), "voxa_audio_search")
    os.makedirs(temp_dir, exist_ok=True)
    temp_filepath = os.path.join(temp_dir, f"search_{uuid.uuid4()}.wav")

    try:
        print(f"\n[Search] Audio Search file upload received: {file.filename}")

        def save_file():
            with open(temp_filepath, "wb") as buffer:
                shutil.copyfileobj(file.file, buffer)

        await run_in_threadpool(save_file)
        print(f"[Search] Saved query audio to: {temp_filepath}")

        # 1. Transcribe voice search query using Whisper
        transcribed_query = await run_in_threadpool(transcribe_audio, temp_filepath)
        print(f"[Search] Audio Query Transcribed: \"{transcribed_query}\"")

        if not transcribed_query or not transcribed_query.strip():
            return JSONResponse(content={
                "success": False,
                "type": "audio",
                "error": "No speech detected in audio search query. Please try recording again."
            })

        # 2. Perform Database Recall Search using LLM
        answer = await run_in_threadpool(recall_memories, transcribed_query)

        return JSONResponse(content={
            "success": True,
            "type": "audio",
            "query": transcribed_query,
            "answer": answer
        })
    except Exception as e:
        print(f"[Search] Error in Audio Search: {e}")
        return JSONResponse(
            status_code=500,
            content={
                "success": False,
                "type": "audio",
                "error": str(e)
            }
        )
    finally:
        if os.path.exists(temp_filepath):
            try:
                os.remove(temp_filepath)
            except Exception:
                pass


@router.post("/audio-raw")
async def search_audio_raw(request: Request):
    """
    Audio Search (Raw WAV Body): Stream raw WAV audio body,
    transcribe using Whisper, search database records using LLM, and return answer.
    """
    temp_dir = os.path.join(tempfile.gettempdir(), "voxa_audio_search")
    os.makedirs(temp_dir, exist_ok=True)
    temp_filepath = os.path.join(temp_dir, f"search_{uuid.uuid4()}.wav")

    try:
        raw_body = await request.body()
        file_size = len(raw_body)
        print(f"\n[Search] Audio Search (Raw) body received: {file_size} bytes")

        if file_size == 0:
            return JSONResponse(
                status_code=400,
                content={"success": False, "type": "audio", "error": "Empty audio body received."}
            )

        def write_file():
            with open(temp_filepath, "wb") as f:
                f.write(raw_body)

        await run_in_threadpool(write_file)

        # 1. Transcribe voice search query using Whisper
        transcribed_query = await run_in_threadpool(transcribe_audio, temp_filepath)
        print(f"[Search] Audio Raw Query Transcribed: \"{transcribed_query}\"")

        if not transcribed_query or not transcribed_query.strip():
            return JSONResponse(content={
                "success": False,
                "type": "audio",
                "error": "No speech detected in audio search query. Please try recording again."
            })

        # 2. Perform Database Recall Search using LLM
        answer = await run_in_threadpool(recall_memories, transcribed_query)

        return JSONResponse(content={
            "success": True,
            "type": "audio",
            "query": transcribed_query,
            "answer": answer
        })
    except Exception as e:
        print(f"[Search] Error in Audio Search Raw: {e}")
        return JSONResponse(
            status_code=500,
            content={
                "success": False,
                "type": "audio",
                "error": str(e)
            }
        )
    finally:
        if os.path.exists(temp_filepath):
            try:
                os.remove(temp_filepath)
            except Exception:
                pass
