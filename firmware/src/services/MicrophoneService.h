#pragma once

#include <string>
#include <cstdint>
#include <cstddef>
#include <Arduino.h>

// NOTE: FS.h / File removed — recordings are no longer staged to SPIFFS.
// Audio is captured into a PSRAM buffer and uploaded directly to the cloud
// via ApiClient::uploadVoiceFromBuffer() when stopRecording() is called.

namespace VOXA
{
    enum class RecordingState
    {
        Idle,
        Starting,
        Recording,
        Stopping,
        Uploading  // replaces "Saving" — audio goes straight to cloud
    };

    const char* recordingStateToString(RecordingState state);

    class MicrophoneService
    {
    public:
        MicrophoneService();
        ~MicrophoneService();

        /// Initialize I2S peripheral
        bool begin();

        /// Start recording — captures PCM into PSRAM buffer.
        /// filePath is kept for the recording title / metadata only (not a real file path).
        bool startRecording(const std::string& title, const char* caller = "unknown");

        /// Stop recording and upload WAV directly to the cloud backend.
        /// Returns true if the upload succeeded.
        bool stopRecording(const char* caller = "unknown", const char* reason = "normal_stop");

        RecordingState getState()     const { return m_state; }
        bool           isRecording()  const { return m_state == RecordingState::Recording; }
        bool           isBusy()       const { return m_state != RecordingState::Idle; }
        uint32_t       getDurationMs() const;

        /// Returns the cloud audio_id returned by the backend after a successful upload.
        std::string getLastAudioId() const { return m_lastAudioId; }

        /// Returns the recording title / label used for the last recording.
        std::string getFilePath() const { return m_recordingTitle; }

        void recordTask();

    private:
        RecordingState m_state { RecordingState::Idle };
        void setState(RecordingState newState, const char* caller);

        bool        m_initialized   { false };
        bool        m_recording     { false };
        bool        m_saving        { false };
        std::string m_recordingTitle;
        std::string m_recordedAt;
        std::string m_lastAudioId;
        uint32_t    m_startMs       { 0 };
        uint32_t    m_durationMs    { 0 };

        TaskHandle_t m_taskHandle        { nullptr };
        uint8_t*     m_psramBuffer       { nullptr };
        size_t       m_bufferOffset      { 0 };
        size_t       m_allocatedBufferSize { 0 };

        /// Writes a 44-byte WAV header into dst (must be at least 44 bytes).
        void buildWavHeader(uint8_t* dst, uint32_t pcmDataSize) const;
    };

    extern MicrophoneService microphoneService;
}
