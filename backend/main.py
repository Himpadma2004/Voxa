import uuid
from datetime import datetime
from services.audio_service import (
    AudioRecorder
)
from services.s3_service import (
    upload_file
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
    file_path = (
        recorder.stop_recording()
    )
    upload_result = upload_file(
        file_path
    )
    document = {
        "audio_id": str(
            uuid.uuid4()
        ),
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
    mongo_id = (
        save_audio_metadata(
            document
        )
    )
    print("\n✅ SUCCESS")
    print(
        f"MongoDB ID: "
        f"{mongo_id}"
    )
    print(
        f"Audio URL: "
        f"{upload_result['audio_url']}"
    )

if __name__ == "__main__":
    main()