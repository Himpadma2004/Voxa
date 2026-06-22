import sounddevice as sd
import soundfile as sf
import numpy as np
import threading
import uuid
import os
from pathlib import Path

MAX_LOCAL_FILES = 7


def cleanup_old_recordings():

    recordings_dir = Path("recordings")

    if not recordings_dir.exists():
        return

    files = sorted(
        recordings_dir.glob("*.wav"),
        key=os.path.getmtime
    )

    while len(files) > MAX_LOCAL_FILES:

        oldest = files.pop(0)

        try:
            oldest.unlink()

            print(
                f"🗑️ Deleted old recording: "
                f"{oldest.name}"
            )

        except Exception as e:
            print(
                f"❌ Delete Error: {e}"
            )


class AudioRecorder:

    def __init__(self, sample_rate=44100):

        self.sample_rate = sample_rate
        self.recording = False
        self.audio_data = []
        self.thread = None

    def _record(self):

        def callback(
            indata,
            frames,
            time,
            status
        ):

            if status:
                print(status)

            self.audio_data.append(
                indata.copy()
            )

        with sd.InputStream(
            samplerate=self.sample_rate,
            channels=1,
            callback=callback
        ):

            while self.recording:
                sd.sleep(100)

    def start_recording(self):

        self.audio_data = []

        self.recording = True

        self.thread = threading.Thread(
            target=self._record
        )

        self.thread.start()

        print("🎤 Recording started...")

    def stop_recording(self):

        self.recording = False

        self.thread.join()

        os.makedirs(
            "recordings",
            exist_ok=True
        )

        filename = (
            f"{uuid.uuid4()}.wav"
        )

        filepath = os.path.join(
            "recordings",
            filename
        )

        audio = np.concatenate(
            self.audio_data,
            axis=0
        )

        sf.write(
            filepath,
            audio,
            self.sample_rate
        )

        cleanup_old_recordings()

        print(
            f"✅ Saved: {filepath}"
        )

        return filepath