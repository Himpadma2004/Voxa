import os
import io
import struct
import shutil
import subprocess
from pathlib import Path
from typing import Optional, List, Dict, Any
from fastapi import APIRouter, Response, HTTPException, UploadFile, File, Query
from fastapi.responses import FileResponse
import httpx
import yt_dlp

from services.s3_service import s3_client, AWS_BUCKET_NAME

router = APIRouter(prefix="/api/music", tags=["Music & Audio"])

ASSETS_DIR = Path(__file__).resolve().parent.parent / "assets"
MUSIC_DIR = ASSETS_DIR / "music"
MUSIC_DIR.mkdir(parents=True, exist_ok=True)

# In-memory track metadata cache
TRACK_CACHE: Dict[str, Dict[str, Any]] = {}

# Built-in curated popular full-length tracks (with real high-definition audio & full duration)
DEFAULT_CURATED_TRACKS = [
    {
        "id": "yt_yKNxeF4KMsY",
        "title": "Yellow",
        "artist": "Coldplay",
        "duration": 273,
        "genre": "Alternative / Rock",
        "query": "Coldplay Yellow official audio"
    },
    {
        "id": "yt_4NRXx6U8ABQ",
        "title": "Blinding Lights",
        "artist": "The Weeknd",
        "duration": 200,
        "genre": "Synthwave / Pop",
        "query": "The Weeknd Blinding Lights official"
    },
    {
        "id": "yt_JGwWNGJdvx8",
        "title": "Shape of You",
        "artist": "Ed Sheeran",
        "duration": 233,
        "genre": "Pop",
        "query": "Ed Sheeran Shape of You official"
    },
    {
        "id": "yt_7wtfhZwyrcc",
        "title": "Believer",
        "artist": "Imagine Dragons",
        "duration": 217,
        "genre": "Alternative Rock",
        "query": "Imagine Dragons Believer official"
    },
    {
        "id": "yt_H5v3kku4y6Q",
        "title": "As It Was",
        "artist": "Harry Styles",
        "duration": 167,
        "genre": "Pop",
        "query": "Harry Styles As It Was official"
    },
    {
        "id": "yt_hT_nvWreIhg",
        "title": "Counting Stars",
        "artist": "OneRepublic",
        "duration": 257,
        "genre": "Pop Rock",
        "query": "OneRepublic Counting Stars official"
    },
    {
        "id": "yt_kTJczUoc26U",
        "title": "Stay",
        "artist": "The Kid LAROI & Justin Bieber",
        "duration": 141,
        "genre": "Pop",
        "query": "The Kid LAROI Justin Bieber Stay official"
    },
    {
        "id": "yt_TUVcZfQe-Kw",
        "title": "Levitating",
        "artist": "Dua Lipa",
        "duration": 203,
        "genre": "Dance Pop",
        "query": "Dua Lipa Levitating official"
    }
]

for t in DEFAULT_CURATED_TRACKS:
    TRACK_CACHE[t["id"]] = t


def get_s3_music_list() -> List[Dict[str, Any]]:
    """
    Fetches all audio files present in AWS S3 under the music/ prefix.
    """
    tracks = []
    try:
        res = s3_client.list_objects_v2(Bucket=AWS_BUCKET_NAME, Prefix="music/")
        for obj in res.get("Contents", []):
            key = obj["Key"]
            if key == "music/" or not (key.endswith(".wav") or key.endswith(".mp3")):
                continue
            fname = os.path.basename(key)
            stem = os.path.splitext(fname)[0]
            clean_title = stem.replace("_", " ").title()
            track_id = f"s3_{stem}"
            t_data = {
                "id": track_id,
                "title": clean_title,
                "artist": "Voxa Cloud S3",
                "duration": 180,
                "genre": "S3 Cloud",
                "s3_key": key,
                "streamUrl": f"/api/music/stream/{track_id}"
            }
            TRACK_CACHE[track_id] = t_data
            tracks.append(t_data)
    except Exception as e:
        print(f"[S3 Music Query Warning] {e}")
    return tracks


def download_and_transcode_yt(video_id_or_url: str, output_wav_path: Path) -> bool:
    """
    Uses yt-dlp to download the highest quality audio stream and transcode
    it to a 16kHz 16-bit Mono PCM WAV file for the ESP32 MAX98357A I2S DAC.
    """
    try:
        url = video_id_or_url
        if not url.startswith("http"):
            url = f"https://www.youtube.com/watch?v={video_id_or_url}"

        temp_template = str(MUSIC_DIR / f"temp_{Path(output_wav_path).stem}.%(ext)s")
        ydl_opts = {
            'format': 'bestaudio/best',
            'outtmpl': temp_template,
            'postprocessors': [{
                'key': 'FFmpegExtractAudio',
                'preferredcodec': 'wav',
                'preferredquality': '192',
            }],
            'postprocessor_args': [
                '-ar', '16000',
                '-ac', '1',
                '-c:a', 'pcm_s16le'
            ],
            'quiet': True,
            'no_warnings': True,
            'noplaylist': True,
        }

        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            ydl.download([url])

        # yt-dlp outputs temp_<stem>.wav
        expected_wav = MUSIC_DIR / f"temp_{Path(output_wav_path).stem}.wav"
        if expected_wav.exists():
            if output_wav_path.exists():
                output_wav_path.unlink()
            shutil.move(str(expected_wav), str(output_wav_path))
            print(f"[Music Engine] Successfully extracted & transcoded: {output_wav_path.name} ({output_wav_path.stat().st_size} bytes)")
            return True

        return False
    except Exception as e:
        print(f"[Music Engine yt-dlp error] {e}")
        return False


@router.get("/library")
async def get_music_library():
    """
    Returns the complete Voxa Music Library containing full-length songs from AWS S3
    and curated popular full-length tracks available for immediate streaming.
    """
    tracks = []
    
    # 1. Add songs from AWS S3 library
    s3_tracks = get_s3_music_list()
    tracks.extend(s3_tracks)

    # 2. Add curated popular full-length tracks
    for t in DEFAULT_CURATED_TRACKS:
        tracks.append({
            "id": t["id"],
            "title": t["title"],
            "artist": t["artist"],
            "duration": t["duration"],
            "genre": t["genre"],
            "streamUrl": f"/api/music/stream/{t['id']}"
        })

    return {"success": True, "count": len(tracks), "tracks": tracks}


@router.get("/search")
async def search_music(q: str = Query(..., description="Song title, artist, or keyword")):
    """
    Searches YouTube and the Free Music catalog for any song requested by the user,
    returning full-length tracks that can be streamed immediately.
    """
    query_str = q.strip()
    if not query_str:
        return {"success": False, "error": "Query cannot be empty", "tracks": []}

    try:
        ydl_opts = {
            'format': 'bestaudio/best',
            'noplaylist': True,
            'quiet': True,
            'no_warnings': True,
            'extract_flat': True
        }
        
        results = []
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            search_query = f"ytsearch6:{query_str}"
            info = ydl.extract_info(search_query, download=False)
            entries = info.get("entries", [])
            for e in entries:
                if not e:
                    continue
                vid = e.get("id")
                if not vid:
                    continue
                
                raw_title = e.get("title", query_str)
                # Clean up title if it contains "Official Video" / "Lyrics" / etc.
                clean_title = raw_title
                for rm in ["(Official Music Video)", "(Official Video)", "[Official Music Video]", "[Official Video]", "(Lyrics)", "[Lyrics]", "(Audio)", "[Audio]", "(Official Audio)", "[Official Audio]"]:
                    clean_title = clean_title.replace(rm, "")
                clean_title = clean_title.strip()

                uploader = e.get("uploader") or e.get("channel") or "Artist"
                duration = int(e.get("duration") or 210)
                
                # Split title/artist if formatted like "Artist - Title"
                artist_name = uploader
                song_title = clean_title
                if " - " in clean_title:
                    parts = clean_title.split(" - ", 1)
                    artist_name = parts[0].strip()
                    song_title = parts[1].strip()

                track_id = f"yt_{vid}"
                track_obj = {
                    "id": track_id,
                    "title": song_title[:32],
                    "artist": artist_name[:32],
                    "duration": duration,
                    "genre": "Full Music",
                    "streamUrl": f"/api/music/stream/{track_id}"
                }
                TRACK_CACHE[track_id] = track_obj
                results.append(track_obj)

        return {"success": True, "count": len(results), "tracks": results}
    except Exception as e:
        print(f"[Music Search Error] {e}")
        return {"success": False, "error": str(e), "tracks": []}


@router.get("/play")
@router.get("/play-by-query")
async def play_by_query(q: str = Query(..., description="Play any song by title or artist")):
    """
    Instant play endpoint: Searches for any song by name/artist, downloads/transcodes
    it on the fly, and streams the full audio directly to the ESP32 speaker.
    """
    search_res = await search_music(q)
    tracks = search_res.get("tracks", [])
    if not tracks:
        raise HTTPException(status_code=404, detail=f"No matching track found for: {q}")
    
    first_track = tracks[0]
    return await stream_track(first_track["id"])


@router.post("/upload")
async def upload_music_file(file: UploadFile = File(...)):
    """
    Uploads a music file (.mp3, .wav) directly into the AWS S3 Voxa Music Library.
    """
    try:
        temp_dest = MUSIC_DIR / file.filename
        with open(temp_dest, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)
        
        # Transcode to 16kHz WAV for seamless ESP32 hardware compatibility
        s3_key = f"music/{file.filename}"
        wav_name = f"{Path(file.filename).stem}_16k.wav"
        wav_path = MUSIC_DIR / wav_name
        
        cmd = ["ffmpeg", "-y", "-i", str(temp_dest), "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le", str(wav_path)]
        subprocess.run(cmd, check=True)
        s3_key = f"music/{wav_name}"
        s3_client.upload_file(str(wav_path), AWS_BUCKET_NAME, s3_key)
        
        clean_name = Path(file.filename).stem.replace("_", " ").title()
        track_id = f"s3_{Path(file.filename).stem}"
        
        TRACK_CACHE[track_id] = {
            "id": track_id,
            "title": clean_name,
            "artist": "Uploaded to S3",
            "duration": 180,
            "genre": "S3 Cloud",
            "s3_key": s3_key
        }
        
        return {
            "success": True,
            "message": f"Uploaded {file.filename} to AWS S3 Music Library",
            "track": {
                "id": track_id,
                "title": clean_name,
                "artist": "Uploaded to S3",
                "streamUrl": f"/api/music/stream/{track_id}"
            }
        }
    except Exception as e:
        print(f"[Music Upload Error] {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/stream/{track_id}")
@router.get("/stream/{track_id}.wav")
async def stream_track(track_id: str):
    """
    Streams the requested full track as a standard 16kHz 16-bit Mono PCM WAV file
    for the MAX98357A I2S DAC on the ESP32-S3 with explicit Content-Length headers.
    """
    # 1. Stream from AWS S3
    if track_id.startswith("s3_"):
        cached_wav = MUSIC_DIR / f"{track_id}.wav"
        if not cached_wav.exists() or cached_wav.stat().st_size < 100:
            track_info = TRACK_CACHE.get(track_id)
            s3_key = track_info.get("s3_key") if track_info else f"music/{track_id.replace('s3_', '')}.wav"
            try:
                s3_client.download_file(AWS_BUCKET_NAME, s3_key, str(cached_wav))
            except Exception as e:
                print(f"[S3 Download Warning] {e}")
                cached_wav = MUSIC_DIR / "sunrise_16k.wav"
        
        return FileResponse(
            path=str(cached_wav),
            media_type="audio/wav",
            filename=f"{track_id}.wav",
            headers={"Accept-Ranges": "bytes", "Content-Type": "audio/wav"}
        )

    # 2. Stream Full-Length YouTube / Online Track
    clean_yt_id = track_id
    if track_id.startswith("yt_"):
        clean_yt_id = track_id[3:]
    elif track_id.startswith("itunes_"):
        # Map legacy iTunes IDs to full song queries
        track_info = TRACK_CACHE.get(track_id)
        if track_info and "title" in track_info:
            clean_yt_id = f"{track_info.get('artist', '')} {track_info.get('title', '')}"
        else:
            clean_yt_id = track_id.replace("itunes_", "")

    cached_wav = MUSIC_DIR / f"yt_{clean_yt_id.replace(' ', '_')}.wav"
    if cached_wav.exists() and cached_wav.stat().st_size > 1000:
        return FileResponse(
            path=str(cached_wav),
            media_type="audio/wav",
            filename=f"{track_id}.wav",
            headers={"Accept-Ranges": "bytes", "Content-Type": "audio/wav"}
        )

    # Download and transcode the full song using yt-dlp
    print(f"[Music Stream] Fetching full audio for: {clean_yt_id}...")
    success = download_and_transcode_yt(clean_yt_id, cached_wav)
    if success and cached_wav.exists():
        return FileResponse(
            path=str(cached_wav),
            media_type="audio/wav",
            filename=f"{track_id}.wav",
            headers={"Accept-Ranges": "bytes", "Content-Type": "audio/wav"}
        )

    # Fallback to local sunrise wav if all else fails
    sunrise_wav = MUSIC_DIR / "sunrise_16k.wav"
    return FileResponse(path=str(sunrise_wav), media_type="audio/wav", filename="sunrise.wav")


@router.get("/background")
async def get_background_music():
    wav_path = MUSIC_DIR / "sunrise_16k.wav"
    return FileResponse(path=str(wav_path), media_type="audio/wav", filename="sunrise.wav")


@router.get("/reminder")
async def get_reminder_music():
    wav_path = MUSIC_DIR / "reminder_16k.wav"
    return FileResponse(path=str(wav_path), media_type="audio/wav", filename="reminder.wav")
