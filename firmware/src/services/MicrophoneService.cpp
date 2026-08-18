#include "MicrophoneService.h"
#include "ApiClient.h"
#include "TimeService.h"
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
            .channel_format     = I2S_CHANNEL_FMT_ONLY_LEFT,
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
            .data_in_num  = 7    // DATA → GPIO7
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

        if (m_recording)
        {
            Serial.printf("[MicrophoneService] Already recording! Rejecting call from: %s\n", caller);
            return false;
        }

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

        m_bufferOffset = 0;
        m_startMs      = nowMs;
        m_durationMs   = 0;
        m_recordedAt   = timeService.getISO8601Time();
        m_recording    = true;

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

        m_recording  = false;
        m_durationMs = nowMs - m_startMs;

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
        // We keep the PCM in PSRAM and prepend a small stack header copy.
        // This avoids doubling PSRAM by allocating a fresh combined buffer —
        // instead we pass header + data as two segments via sendRequest overload.
        // For simplicity we use a single combined alloc from internal DRAM
        // (header = 44 bytes) + a reference to PSRAM for the body.
        // HTTPClient::POST(buf, len) requires a contiguous buffer, so we build
        // a 44-byte header, write it, then stream the PSRAM in chunks.
        // Simpler: allocate header only; use http.sendRequest with stream pointer.
        // Current approach: build WAV header in stack, post header + PSRAM.

        constexpr size_t kHeaderSize = 44;
        uint8_t header[kHeaderSize];
        buildWavHeader(header, pcmDataSize);

        setState(RecordingState::Uploading, caller);

        // Build a single contiguous WAV buffer if internal DRAM can hold header
        // (44 bytes always fine), then POST it along with PSRAM PCM data.
        // Since HTTPClient::POST(uint8_t*, size_t) needs one contiguous buffer
        // and PCM is in PSRAM, we allocate a tiny header-only heap buf and
        // stream via a combined in-heap buffer only when data fits in DRAM.
        // For cloud upload we always have the PSRAM buffer available;
        // create a combined PSRAM-backed WAV: copy header to start of a fresh alloc.
        size_t totalWavSize = kHeaderSize + pcmDataSize;
        uint8_t* wavBuf = (uint8_t*)heap_caps_malloc(totalWavSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!wavBuf)
        {
            // PSRAM full — try internal DRAM for smaller recordings
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
                m_lastAudioId = res.text; // audio_id from backend JSON
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

        // Clear buffer so duplicate stopRecording() calls exit early
        m_bufferOffset = 0;
        setState(RecordingState::Idle, caller);
        return uploadOk;
    }

    uint32_t MicrophoneService::getDurationMs() const
    {
        if (m_recording) return millis() - m_startMs;
        return m_durationMs;
    }

    // -----------------------------------------------------------------------
    // recordTask() — I2S read loop (unchanged)
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

                if (totalReads == 1 || totalReads % 100 == 0)
                    Serial.printf("[MicrophoneService] i2s_read: %u bytes (total: %u, offset: %u)\n",
                                  (unsigned)bytesRead, (unsigned)totalBytesRead,
                                  (unsigned)m_bufferOffset);

                int sampleCount = bytesRead / sizeof(int32_t);
                for (int i = 0; i < sampleCount; i++)
                {
                    int32_t sample = rawBuffer[i] >> 16;  // upper 16 bits
                    if (sample >  32767) sample =  32767;
                    if (sample < -32768) sample = -32768;
                    pcmBuffer[i] = (int16_t)sample;
                }

                size_t chunkBytes = sampleCount * sizeof(int16_t);
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