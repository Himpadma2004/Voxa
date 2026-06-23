from services.s3_service import (
    download_file
)

from ai.whisper_engine import (
    transcribe_audio
)

from database.mongodb import (
    update_transcript
)


def process_audio(
    audio_id,
    s3_key
):

    temp_file = download_file(
        s3_key
    )

    transcript = transcribe_audio(
        temp_file
    )

    update_transcript(
        audio_id,
        transcript
    )

    return transcript