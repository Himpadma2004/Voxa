import uuid
from datetime import datetime

from services.audio_service import AudioRecorder
from services.s3_service import upload_file

from services.transcription_service import (
    process_audio
)

from database.mongodb import (
    save_audio_metadata
)


def main():

    recorder = AudioRecorder()

    input(
        "Press ENTER to start recording..."
    )

    recorder.start_recording()

    input(
        "Press ENTER to stop recording..."
    )

    file_path = recorder.stop_recording()

    print("📤 Uploading to S3...")

    upload_result = upload_file(
        file_path
    )

    print("✅ Uploaded to S3")

    audio_id = str(
        uuid.uuid4()
    )

    document = {

        "audio_id": audio_id,

        "filename":
            file_path.split("\\")[-1],

        "s3_key":
            upload_result["s3_key"],

        "audio_url":
            upload_result["audio_url"],

        "status":
            "uploaded",

        "created_at":
            datetime.utcnow()
    }

    save_audio_metadata(
        document
    )

    print("✅ MongoDB document created")

    print(
        "\n🔄 Starting Transcription..."
    )

    transcript = process_audio(
        audio_id,
        upload_result["s3_key"]
    )

    print(
        "\n✅ Transcript Generated:"
    )

    print(transcript)


if __name__ == "__main__":
    main()