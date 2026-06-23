import whisper

print("🔄 Loading Whisper model...")

model = whisper.load_model("small")

print("✅ Whisper model loaded")


def transcribe_audio(audio_path):

    print("🔄 Starting transcription...")

    result = model.transcribe(audio_path)

    print("✅ Transcription completed")

    return result["text"]