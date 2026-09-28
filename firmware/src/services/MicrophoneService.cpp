#include "MicrophoneService.h"
#include "ApiClient.h"
#include "TimeService.h"
#include "../audio/AudioManager.h"
#include <driver/i2s.h>
#include <algorithm>
#include <cstring>

// ============================================================
// MicrophoneService — Cloud-Direct Recording
//
// Audio flow:
//   I2S mic → PSRAM buffer (raw PCM int16_t)
//       ↓  stopRecording()
//   prepend 44-byte WAV header in a temp heap buffer
//       ↓
//   ApiClient::uploadVoiceFromBuffer() → HTTP POST → backend
//
// No SPIFFS write. No SD card. Audio is never written to flash.
// ============================================================

namespace VOXA
{
    MicrophoneService microphoneService;

    // -----------------------------------------------------------------------
    // State helpers
    // -----------------------------------------------------------------------

    const char* recordingStateToString(RecordingState state)
    {
        switch (state)
        {
            case RecordingState::Idle:      return "Idle";
            case RecordingState::Starting:  return "Starting";
            case RecordingState::Recording: return "Recording";
            case RecordingState::Stopping:  return "Stopping";
            case RecordingState::Uploading: return "Uploading";
            default:                        return "Unknown";
        }
    }

    void MicrophoneService::setState(RecordingState newState, const char* caller)
    {
        m_state = newState;
        Serial.printf("[MicrophoneService] State → %s (caller: %s)\n",
                      recordingStateToString(newState), caller);
    }

    // -----------------------------------------------------------------------
    // Constructor / Destructor
    // -----------------------------------------------------------------------

    MicrophoneService::MicrophoneService() {}

    MicrophoneService::~MicrophoneService()
    {
        stopRecording("destructor", "shutdown");
        if (m_initialized)
            i2s_driver_uninstall(I2S_NUM_0);
        if (m_psramBuffer)
        {
            free(m_psramBuffer);
            m_psramBuffer = nullptr;
        }
    }

    // -----------------------------------------------------------------------
    // begin() — I2S init
    // -----------------------------------------------------------------------

    bool MicrophoneService::begin()
    {
        if (m_initialized) return true;

        Serial.println("[MicrophoneService] Initializing I2S...");

        constexpr gpio_num_t MIC_POWER_PIN = GPIO_NUM_42;
        pinMode(MIC_POWER_PIN, OUTPUT);
        digitalWrite(MIC_POWER_PIN, HIGH);
        delay(50);
        Serial.println("[MicrophoneService] Microphone powered from GPIO42 (HIGH)");

        i2s_config_t i2s_config = {
            .mode               = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate        = 16000,
            .bits_per_sample    = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format     = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags   = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count      = 16,
            .dma_buf_len        = 256,
            .use_apll           = false,
            .tx_desc_auto_clear = false,
            .fixed_mclk         = 0
        };

        i2s_pin_config_t pin_config = {
            .bck_io_num   = 4,   // BCLK → GPIO4
            .ws_io_num    = 5,   // LRCK → GPIO5
            .data_out_num = -1,
            .data_in_num  = 7    // DATA (SD) → GPIO7 (Microphone Serial Data In)
        };

        esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
        if (err != ESP_OK)
        {
            Serial.printf("[MicrophoneService] Driver install failed: %d\n", err);
            return false;
        }

        err = i2s_set_pin(I2S_NUM_0, &pin_config);
        if (err != ESP_OK)
        {
            Serial.printf("[MicrophoneService] Pin config failed: %d\n", err);
            i2s_driver_uninstall(I2S_NUM_0);
            return false;
        }

        m_initialized = true;
        Serial.println("[MicrophoneService] I2S initialized successfully");
        return true;
    }

    // -----------------------------------------------------------------------
    // startRecording()
    // -----------------------------------------------------------------------

    bool MicrophoneService::startRecording(const std::string& title, const char* caller)
    {
        uint32_t nowMs = millis();
        Serial.printf("[MicrophoneService] startRecording() called by: %s @ %u ms\n", caller, nowMs);

        // Stop audio playback cleanly without un-installing drivers
        AudioManager::instance().stop();
        AudioManager::instance().stopBackgroundMusic();

        if (!m_initialized && !begin())
        {
            Serial.printf("[MicrophoneService] I2S init failed (caller: %s)\n", caller);
            return false;
        }

        m_recordingTitle = title;
        m_lastAudioId    = "";

        // Allocate PSRAM buffer
        if (!m_psramBuffer)
        {
            size_t trySizes[] = {2000000, 1048576, 524288, 262144};
            for (size_t sz : trySizes)
            {
                m_psramBuffer = (uint8_t*)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (m_psramBuffer)
                {
                    m_allocatedBufferSize = sz;
                    Serial.printf("[MicrophoneService] PSRAM buffer allocated: %u bytes\n", (unsigned)sz);
                    break;
                }
            }
            if (!m_psramBuffer)
            {
                m_psramBuffer = (uint8_t*)malloc(131072); // 128KB fallback
                if (m_psramBuffer)
                {
                    m_allocatedBufferSize = 131072;
                    Serial.println("[MicrophoneService] WARNING: DRAM fallback buffer (128KB)");
                }
                else
                {
                    Serial.println("[MicrophoneService] ERROR: Failed to allocate audio buffer!");
                    return false;
                }
            }
        }

        m_bufferOffset  = 0;
        m_startMs       = nowMs;
        m_durationMs    = 0;
        m_accumulatedMs = 0;
        m_lastResumeMs  = nowMs;
        m_paused        = false;
        m_lastDb        = -14;
        m_recordedAt    = timeService.getISO8601Time();
        m_recording     = true;

        setState(RecordingState::Recording, caller);

        xTaskCreatePinnedToCore(
            [](void* param) { static_cast<MicrophoneService*>(param)->recordTask(); },
            "MicRecordTask",
            4096,
            this,
            5,            // High priority
            &m_taskHandle,
            1             // Core 1 (UI on Core 0)
        );

        Serial.printf("[MicrophoneService] Recording started (title: %s, time: %s, caller: %s)\n",
                      title.c_str(), m_recordedAt.c_str(), caller);
        return true;
    }

    // -----------------------------------------------------------------------
    // pauseRecording() / resumeRecording() / cancelRecording()
    // -----------------------------------------------------------------------

    bool MicrophoneService::pauseRecording(const char* caller)
    {
        if (m_recording && !m_paused)
        {
            uint32_t nowMs = millis();
            m_accumulatedMs += (nowMs - m_lastResumeMs);
            m_paused = true;
            Serial.printf("[MicrophoneService] Recording PAUSED (caller: %s, accumulated: %u ms)\n",
                          caller, (unsigned)m_accumulatedMs);
            return true;
        }
        return false;
    }

    bool MicrophoneService::resumeRecording(const char* caller)
    {
        if (m_recording && m_paused)
        {
            m_lastResumeMs = millis();
            m_paused = false;
            Serial.printf("[MicrophoneService] Recording RESUMED (caller: %s)\n", caller);
            return true;
        }
        return false;
    }

    bool MicrophoneService::cancelRecording(const char* caller)
    {
        Serial.printf("[MicrophoneService] cancelRecording() caller: %s\n", caller);
        m_recording     = false;
        m_paused        = false;
        m_durationMs    = 0;
        m_accumulatedMs = 0;

        if (m_taskHandle != nullptr)
        {
            vTaskDelay(pdMS_TO_TICKS(120));
            m_taskHandle = nullptr;
        }

        m_bufferOffset = 0;
        setState(RecordingState::Idle, caller);
        return true;
    }

    // -----------------------------------------------------------------------
    // stopRecording() — stop I2S capture, build WAV, upload to cloud
    // -----------------------------------------------------------------------

    bool MicrophoneService::stopRecording(const char* caller, const char* reason)
    {
        uint32_t nowMs = millis();
        Serial.printf("[MicrophoneService] stopRecording() caller: %s, reason: %s @ %u ms\n",
                      caller, reason, nowMs);

        if (!m_recording && m_bufferOffset == 0)
        {
            Serial.println("[MicrophoneService] Not recording — nothing to do.");
            return false;
        }

        if (m_recording && !m_paused)
        {
            m_accumulatedMs += (nowMs - m_lastResumeMs);
        }

        m_recording  = false;
        m_paused     = false;
        m_durationMs = m_accumulatedMs;

        setState(RecordingState::Stopping, caller);

        // Let the I2S task exit
        if (m_taskHandle != nullptr)
        {
            vTaskDelay(pdMS_TO_TICKS(150));
            m_taskHandle = nullptr;
        }

        const uint32_t pcmDataSize = (uint32_t)m_bufferOffset;
        if (pcmDataSize == 0)
        {
            Serial.println("[MicrophoneService] Empty buffer — nothing to upload.");
            setState(RecordingState::Idle, caller);
            return false;
        }

        Serial.printf("[MicrophoneService] PCM data: %u bytes | Duration: %u ms | Time: %s\n",
                      pcmDataSize, m_durationMs, m_recordedAt.c_str());

        // ── Build complete WAV = 44-byte header + PCM in a heap buffer ────────
        constexpr size_t kHeaderSize = 44;
        uint8_t header[kHeaderSize];
        buildWavHeader(header, pcmDataSize);

        setState(RecordingState::Uploading, caller);

        size_t totalWavSize = kHeaderSize + pcmDataSize;
        uint8_t* wavBuf = (uint8_t*)heap_caps_malloc(totalWavSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!wavBuf)
        {
            wavBuf = (uint8_t*)malloc(totalWavSize);
        }

        bool uploadOk = false;
        if (wavBuf)
        {
            memcpy(wavBuf,              header,       kHeaderSize);
            memcpy(wavBuf + kHeaderSize, m_psramBuffer, pcmDataSize);

            Serial.printf("[MicrophoneService] Uploading %u bytes WAV to cloud (timestamp: %s)...\n",
                          (unsigned)totalWavSize, m_recordedAt.c_str());

            ApiResult res = apiClient.uploadVoiceFromBuffer(wavBuf, totalWavSize, m_recordedAt);
            free(wavBuf);

            if (res.success)
            {
                m_lastAudioId = res.text;
                Serial.printf("[MicrophoneService] Cloud upload SUCCESS — audio_id: %s\n",
                              m_lastAudioId.c_str());
                uploadOk = true;
            }
            else
            {
                Serial.printf("[MicrophoneService] Cloud upload FAILED: %s\n", res.error.c_str());
            }
        }
        else
        {
            Serial.println("[MicrophoneService] ERROR: Could not allocate WAV buffer for upload!");
        }

        m_bufferOffset = 0;
        setState(RecordingState::Idle, caller);
        return uploadOk;
    }

    uint32_t MicrophoneService::getDurationMs() const
    {
        if (m_recording)
        {
            if (m_paused) return m_accumulatedMs;
            return m_accumulatedMs + (millis() - m_lastResumeMs);
        }
        return m_durationMs;
    }

    // -----------------------------------------------------------------------
    // recordTask() — I2S read loop with dynamic level measurement
    // -----------------------------------------------------------------------

    void MicrophoneService::recordTask()
    {
        Serial.println("[MicrophoneService] recordTask started");
        constexpr int BUFFER_SAMPLES = 256;

        int32_t rawBuffer[BUFFER_SAMPLES];
        int16_t pcmBuffer[BUFFER_SAMPLES];

        i2s_zero_dma_buffer(I2S_NUM_0);

        size_t totalReads      = 0;
        size_t totalBytesRead  = 0;
        const char* exitReason = "m_recording flag became false";

        while (m_recording)
        {
            size_t bytesRead = 0;
            esp_err_t err = i2s_read(I2S_NUM_0, rawBuffer, sizeof(rawBuffer),
                                     &bytesRead, portMAX_DELAY);
            totalReads++;

            if (err == ESP_OK && bytesRead > 0)
            {
                totalBytesRead += bytesRead;

                int totalSlots = bytesRead / sizeof(int32_t);
                int stereoPairs = totalSlots / 2;
                int32_t maxSample = 0;

                for (int i = 0; i < stereoPairs; i++)
                {
                    int32_t leftSample  = rawBuffer[i * 2];
                    int32_t rightSample = rawBuffer[i * 2 + 1];
                    int32_t rawVal = (std::abs(leftSample) >= std::abs(rightSample)) ? leftSample : rightSample;
                    int32_t sample = rawVal >> 14;
                    if (sample >  32767) sample =  32767;
                    if (sample < -32768) sample = -32768;
                    pcmBuffer[i] = (int16_t)sample;

                    int32_t absS = std::abs(sample);
                    if (absS > maxSample) maxSample = absS;
                }

                if (maxSample > 0)
                {
                    float ratio = (float)maxSample / 32768.0f;
                    int db = (int)(20.0f * std::log10(ratio));
                    if (db < -60) db = -60;
                    if (db > 0) db = 0;
                    m_lastDb = db;
                }

                if (!m_paused)
                {
                    size_t chunkBytes = stereoPairs * sizeof(int16_t);
                    if (m_bufferOffset + chunkBytes <= m_allocatedBufferSize)
                    {
                        memcpy(m_psramBuffer + m_bufferOffset, pcmBuffer, chunkBytes);
                        m_bufferOffset += chunkBytes;
                    }
                    else
                    {
                        exitReason = "Buffer limit reached";
                        Serial.println("[MicrophoneService] PSRAM buffer full — auto-stopping.");
                        m_recording = false;
                        break;
                    }
                }
            }
            else if (err != ESP_OK)
            {
                Serial.printf("[MicrophoneService] i2s_read error: %d\n", (int)err);
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        Serial.printf("[MicrophoneService] recordTask exiting. Reason: %s. Buffer: %u bytes\n",
                      exitReason, (unsigned)m_bufferOffset);
        vTaskDelete(nullptr);
    }

    // -----------------------------------------------------------------------
    // buildWavHeader() — writes 44-byte PCM WAV header into dst
    // -----------------------------------------------------------------------

    void MicrophoneService::buildWavHeader(uint8_t* dst, uint32_t pcmDataSize) const
    {
        // RIFF chunk
        memcpy(dst,      "RIFF", 4);
        uint32_t chunkSize = 36 + pcmDataSize;
        memcpy(dst + 4,  &chunkSize, 4);
        memcpy(dst + 8,  "WAVE", 4);

        // fmt sub-chunk
        memcpy(dst + 12, "fmt ", 4);
        uint32_t subChunk1Size = 16;    memcpy(dst + 16, &subChunk1Size, 4);
        uint16_t audioFormat   = 1;     memcpy(dst + 20, &audioFormat,   2); // PCM
        uint16_t numChannels   = 1;     memcpy(dst + 22, &numChannels,   2); // Mono
        uint32_t sampleRate    = 16000; memcpy(dst + 24, &sampleRate,    4);
        uint32_t byteRate      = 32000; memcpy(dst + 28, &byteRate,      4); // 16000*1*2
        uint16_t blockAlign    = 2;     memcpy(dst + 32, &blockAlign,    2);
        uint16_t bitsPerSample = 16;    memcpy(dst + 34, &bitsPerSample, 2);

        // data sub-chunk
        memcpy(dst + 36, "data", 4);
        memcpy(dst + 40, &pcmDataSize, 4);
    }

} // namespace VOXA