import whisper

_model = None

def get_model():
    global _model
    if _model is None:
        print("Loading Whisper model...")
        _model = whisper.load_model("small")
        print("Whisper model loaded")
    return _model


def transcribe_audio(audio_path):
    print("Starting transcription...")
    model = get_model()
    result = model.transcribe(
        audio_path,
        fp16=False
    )

    print(f"Detected Language: {result['language']}")
    print("Transcription completed")
    return result["text"]
