import uuid

from datetime import datetime

from services.audio_service import (
    AudioRecorder
)

from services.s3_service import (
    upload_file
)

from services.transcription_service import (
    process_audio
)

from services.processing_service import (
    process_transcript
)

from database.mongodb import (
    save_audio_metadata,
    update_llm_result
)

from reminders.reminder_service import (
    process_reminders
)

from vector_db.chroma_client import (
    add_memory
)

from vector_db.memory_formatter import (
    build_memory_text
)

from config.settings import (
    ACTIVE_MODEL
)


def main():
    print(
        "\nVoxa Started"
    )

    recorder = AudioRecorder()

    input(
        "\nPress ENTER to start recording..."
    )

    recorder.start_recording()

    input(
        "Press ENTER to stop recording..."
    )

    file_path = recorder.stop_recording()

    print(
        "\nUploading to S3..."
    )

    upload_result = upload_file(
        file_path
    )

    print(
        "Uploaded to S3"
    )

    audio_id = str(
        uuid.uuid4()
    )

    document = {
        "audio_id":
            audio_id,
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

    print(
        "MongoDB document created"
    )

    print(
        "\nStarting Transcription..."
    )

    transcript = process_audio(
        audio_id,
        upload_result[
            "s3_key"
        ]
    )

    print(
        "\nTranscript Generated:"
    )

    print(
        transcript
    )

    print(
        "\nStarting LLM Processing..."
    )

    structured_data = process_transcript(
        transcript,
        ACTIVE_MODEL
    )

    print(
        "\nStructured Output:"
    )

    print(
        structured_data
    )

    update_llm_result(
        audio_id,
        structured_data,
        ACTIVE_MODEL
    )

    process_reminders(
        audio_id,
        transcript,
        structured_data
    )

    memory_text = build_memory_text(
        transcript,
        structured_data
    )

    print(
        "\n===== MEMORY TEXT ====="
    )

    print(
        memory_text
    )

    print(
        "=======================\n"
    )

    add_memory(
        audio_id,
        memory_text,
        {
            "category":
                structured_data.get(
                    "category",
                    "Other"
                )
        }
    )

    print(
        "\nMemory saved to vector database"
    )

    print(
        "\nVoxa Pipeline Completed"
    )


if __name__ == "__main__":
    main()
