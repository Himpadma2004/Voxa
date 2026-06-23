import whisper
print("🔄 Loading Whisper model...")
model = whisper.load_model("medium")
model = whisper.load_model("small")
print("✅ Whisper model loaded")


def transcribe_audio(audio_path):

    result = model.transcribe(
        audio_path,
        fp16=False
    )

    print(
        f"Detected Language: {result['language']}"
    )

    print("🔄 Starting transcription...")

    result = model.transcribe(audio_path)

    print("✅ Transcription completed")

    return result["text"]