import whisper

print("🔄 Loading Whisper model...")

<<<<<<< HEAD
model = whisper.load_model("medium")
=======
model = whisper.load_model("small")
>>>>>>> 9738e2f5fa5949e4fcb8822dbaa324fd68f60e2c

print("✅ Whisper model loaded")


def transcribe_audio(audio_path):

<<<<<<< HEAD
    result = model.transcribe(
        audio_path,
        fp16=False
    )

    print(
        f"Detected Language: {result['language']}"
    )
=======
    print("🔄 Starting transcription...")

    result = model.transcribe(audio_path)

    print("✅ Transcription completed")
>>>>>>> 9738e2f5fa5949e4fcb8822dbaa324fd68f60e2c

    return result["text"]