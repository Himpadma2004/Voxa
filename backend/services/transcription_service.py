from services.s3_service import download_file
from ai.whisper_engine import transcribe_audio
from database.mongodb import update_transcript

import os


def process_audio(audio_id, s3_key):

    print("📥 Downloading audio from S3...")

    temp_file = download_file(s3_key)

    print(f"✅ Downloaded: {temp_file}")

    transcript = transcribe_audio(temp_file)

    update_transcript(
        audio_id,
        transcript
    )

    print("✅ MongoDB updated")

    if os.path.exists(temp_file):
        os.remove(temp_file)
        print("🗑️ Temporary file deleted")

    return transcript