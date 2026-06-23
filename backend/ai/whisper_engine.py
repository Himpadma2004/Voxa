import whisper

print("🔄 Loading Whisper model...")

model = whisper.load_model("medium")

print("✅ Whisper model loaded")


def transcribe_audio(audio_path):

    result = model.transcribe(
        audio_path,
        fp16=False
    )

    print(
        f"Detected Language: {result['language']}"
    )

    return result["text"]