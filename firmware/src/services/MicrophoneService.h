#pragma once

#include <string>
#include <cstdint>
#include <Arduino.h>
#include <FS.h>


namespace VOXA
{
    enum class RecordingState
    {
        Idle,
        Starting,
        Recording,
        Stopping,
        Saving,
        Uploading
    };

    const char* recordingStateToString(RecordingState state);

    class MicrophoneService
    {
    public:
        MicrophoneService();
        ~MicrophoneService();

        /// Initialize I2S peripheral and verify connection
        bool begin();

        /// Start recording to a WAV file on SPIFFS
        bool startRecording(const std::string& filePath, const char* caller = "unknown");

        /// Stop recording and finalize the WAV header
        bool stopRecording(const char* caller = "unknown", const char* reason = "normal_stop");

        /// Get current recording state
        RecordingState getState() const { return m_state; }

        /// Check if currently recording
        bool isRecording() const { return m_state == RecordingState::Recording; }

        /// Check if busy recording, saving, or uploading
        bool isBusy() const { return m_state != RecordingState::Idle; }

        /// Get current recording duration in milliseconds
        uint32_t getDurationMs() const;

        /// Get the output file path of the current/last recording
        std::string getFilePath() const { return m_filePath; }

        // FreeRTOS task entry point
        void recordTask();

    private:
        RecordingState m_state { RecordingState::Idle };
        void setState(RecordingState newState, const char* caller);

        bool        m_initialized { false };
        bool        m_recording   { false };
        bool        m_saving      { false };
        std::string m_filePath;
        uint32_t    m_startMs     { 0 };
        uint32_t    m_durationMs  { 0 };
        
        TaskHandle_t m_taskHandle { nullptr };
        File         m_file;
        uint8_t*     m_psramBuffer { nullptr };
        size_t       m_bufferOffset { 0 };
        size_t       m_allocatedBufferSize { 0 };
        const size_t m_bufferSize { 2000000 }; // 2 MB (~62 seconds of 16kHz 16-bit mono)

        bool writeWavHeader(File& file, uint32_t dataSize);
    };

    extern MicrophoneService microphoneService;
}
