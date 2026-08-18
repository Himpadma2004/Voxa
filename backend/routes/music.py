import os
import io
import shutil
from pathlib import Path
from fastapi import APIRouter, Response, HTTPException
from fastapi.responses import FileResponse
import soundfile as sf
import numpy as np
import scipy.signal

router = APIRouter(prefix="/api/music", tags=["Music & Audio"])

ASSETS_DIR = Path(__file__).resolve().parent.parent / "assets"
MUSIC_DIR = ASSETS_DIR / "music"


def ensure_44k_wav(mp3_name: str, wav_name: str) -> Path:
    """
    Ensures a studio standard 44.1kHz (44100 Hz) 16-bit Mono WAV file is pre-converted
    and cached for optimal MAX98357A PLL clock locking and crystal-clear playback.
    """
    MUSIC_DIR.mkdir(parents=True, exist_ok=True)
    wav_path = MUSIC_DIR / wav_name
    
    # Locate source MP3 either in assets/music or assets/
    mp3_path = MUSIC_DIR / mp3_name
    if not mp3_path.exists():
        fallback_mp3 = ASSETS_DIR / mp3_name
        if fallback_mp3.exists():
            shutil.copy(fallback_mp3, mp3_path)
    
    if not mp3_path.exists():
        raise HTTPException(status_code=404, detail=f"Audio asset '{mp3_name}' not found")
    
    # Check if WAV exists and is newer than MP3
    if wav_path.exists() and wav_path.stat().st_mtime >= mp3_path.stat().st_mtime:
        return wav_path

    print(f"[Music] Transcoding {mp3_name} -> {wav_name} (44.1kHz 16-bit PCM)...")
    data, sr = sf.read(str(mp3_path))
    if data.ndim > 1:
        data = data.mean(axis=1) # Convert to Mono

    target_sr = 44100
    if sr != target_sr:
        num_target_samples = int(len(data) * target_sr / sr)
        data = scipy.signal.resample(data, num_target_samples)

    # Normalize to maximum 0 dBFS peak to ensure full loudness
    peak = np.max(np.abs(data))
    if peak > 0:
        data = (data / peak) * 0.98

    data_int16 = (data * 32767).astype(np.int16)
    sf.write(str(wav_path), data_int16, target_sr, subtype="PCM_16", format="WAV")
    print(f"[Music] Transcoding complete: {wav_path} ({wav_path.stat().st_size} bytes)")
    return wav_path


@router.get("/background")
async def get_background_music():
    """
    Streams 'sunrise.mp3' pre-transcoded into standard 44.1kHz 16-bit PCM WAV for MAX98357A.
    """
    try:
        wav_path = ensure_44k_wav("sunrise.mp3", "sunrise_44k.wav")
        return FileResponse(
            path=str(wav_path),
            media_type="audio/wav",
            filename="sunrise_44k.wav"
        )
    except Exception as e:
        print(f"[Music] Error serving background music: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/reminder")
async def get_reminder_music():
    """
    Streams 'reminder.mp3' pre-transcoded into standard 44.1kHz 16-bit PCM WAV for reminder alert.
    """
    try:
        wav_path = ensure_44k_wav("reminder.mp3", "reminder_44k.wav")
        return FileResponse(
            path=str(wav_path),
            media_type="audio/wav",
            filename="reminder_44k.wav"
        )
    except Exception as e:
        print(f"[Music] Error serving reminder music: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/raw/sunrise")
async def get_raw_sunrise_mp3():
    mp3_path = MUSIC_DIR / "sunrise.mp3"
    if not mp3_path.exists():
        raise HTTPException(status_code=404, detail="sunrise.mp3 not found")
    return FileResponse(path=str(mp3_path), media_type="audio/mpeg")


@router.get("/raw/reminder")
async def get_raw_reminder_mp3():
    mp3_path = MUSIC_DIR / "reminder.mp3"
    if not mp3_path.exists():
        mp3_path = ASSETS_DIR / "reminder.mp3"
    if not mp3_path.exists():
        raise HTTPException(status_code=404, detail="reminder.mp3 not found")
    return FileResponse(path=str(mp3_path), media_type="audio/mpeg")
