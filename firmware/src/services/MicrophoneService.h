#pragma once

#include <string>
#include <cstdint>
#include <Arduino.h>
#include <FS.h>


namespace VOXA
{
    class MicrophoneService
    {
    public:
        MicrophoneService();
        ~MicrophoneService();

        /// Initialize I2S peripheral and verify connection
        bool begin();

        /// Start recording to a WAV file on SPIFFS
        bool startRecording(const std::string& filePath);

        /// Stop recording and finalize the WAV header
        bool stopRecording();

        /// Check if currently recording
        bool isRecording() const { return m_recording; }

        /// Get current recording duration in milliseconds
        uint32_t getDurationMs() const;

        /// Get the output file path of the current/last recording
        std::string getFilePath() const { return m_filePath; }

        // FreeRTOS task entry point
        void recordTask();

    private:
        bool        m_initialized { false };
        bool        m_recording   { false };
        std::string m_filePath;
        uint32_t    m_startMs     { 0 };
        uint32_t    m_durationMs  { 0 };
        
        TaskHandle_t m_taskHandle { nullptr };
        File         m_file;

        bool writeWavHeader(File& file, uint32_t dataSize);
    };

    extern MicrophoneService microphoneService;
}
