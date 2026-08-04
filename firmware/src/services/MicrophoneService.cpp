#include "MicrophoneService.h"
#include "SDCardService.h"
#include "../storage/SpiffsMutex.h"
#include <driver/i2s.h>
#include <SPIFFS.h>
#include <SD.h>
#include <algorithm>

namespace VOXA
{
    MicrophoneService microphoneService;

    MicrophoneService::MicrophoneService()
    {
    }

    MicrophoneService::~MicrophoneService()
    {
        stopRecording();
        if (m_initialized)
        {
            i2s_driver_uninstall(I2S_NUM_0);
        }
    }

    bool MicrophoneService::begin()
    {
        if (m_initialized)
            return true;

        Serial.println("[MicrophoneService] Initializing I2S...");

        // Microphone VCC Power Pin (GPIO42)
        constexpr gpio_num_t MIC_POWER_PIN = GPIO_NUM_42;

        pinMode(MIC_POWER_PIN, OUTPUT);
        digitalWrite(MIC_POWER_PIN, HIGH);

        delay(50);
        Serial.println("[MicrophoneService] Microphone powered from GPIO42 (HIGH)");

        // Setup I2S configuration for I2S microphone (INMP441)
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = 16000,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 16,
            .dma_buf_len = 256,
            .use_apll = false,
            .tx_desc_auto_clear = false,
            .fixed_mclk = 0};

        i2s_pin_config_t pin_config = {
            .bck_io_num = 4,    // BCLK -> GPIO4
            .ws_io_num = 5,     // LRCK -> GPIO5
            .data_out_num = -1, // Not used
            .data_in_num = 7    // DATA -> GPIO7
        };

        Serial.println("[MicrophoneService] Configuring I2S parameters:");
        Serial.printf("  - Sample Rate: %d Hz\n", i2s_config.sample_rate);
        Serial.printf("  - Bits per Sample: %d-bit\n", i2s_config.bits_per_sample);
        Serial.printf("  - Mode: RX Master\n");
        Serial.printf("  - Format: Mono\n");
        Serial.printf("  - Pin Mapping: BCLK=GPIO %d, LRCK=GPIO %d, DATA=GPIO %d, VCC=GPIO %d\n",
                      pin_config.bck_io_num, pin_config.ws_io_num, pin_config.data_in_num, MIC_POWER_PIN);

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
        Serial.println("[MicrophoneService] I2S initialized successfully and verified");
        return true;
    }

    bool MicrophoneService::startRecording(const std::string &filePath, const char *caller)
    {
        uint32_t nowMs = millis();
        Serial.printf("[MicrophoneService] startRecording() called by: %s, timestamp: %u ms\n", caller, nowMs);

        if (m_recording)
        {
            Serial.printf("[MicrophoneService] Already recording! Rejecting startRecording call from: %s\n", caller);
            return false;
        }

        if (!m_initialized && !begin())
        {
            Serial.printf("[MicrophoneService] ERROR: I2S initialization failed on startRecording() called by: %s\n", caller);
            return false;
        }

        m_filePath = filePath;

        // Allocate PSRAM/DRAM buffer if not already done
        if (!m_psramBuffer)
        {
            size_t trySizes[] = {2000000, 1048576, 524288, 262144};
            for (size_t sz : trySizes)
            {
                m_psramBuffer = (uint8_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (m_psramBuffer)
                {
                    m_allocatedBufferSize = sz;
                    Serial.printf("[MicrophoneService] PSRAM buffer successfully allocated (%u bytes)\n", (unsigned int)sz);
                    break;
                }
            }

            if (!m_psramBuffer)
            {
                // Fallback to internal DRAM if PSRAM capability is not present/available
                m_psramBuffer = (uint8_t *)malloc(131072); // 128KB (~4 seconds)
                if (m_psramBuffer)
                {
                    m_allocatedBufferSize = 131072;
                    Serial.println("[MicrophoneService] WARNING: Allocated 128KB internal DRAM buffer (PSRAM unavailable)");
                }
                else
                {
                    Serial.println("[MicrophoneService] ERROR: Failed to allocate audio recording buffer!");
                    return false;
                }
            }
        }

        m_bufferOffset = 0;
        m_startMs = nowMs;
        m_durationMs = 0;
        m_recording = true;

        // Spawn high-priority background audio reader task
        xTaskCreatePinnedToCore(
            [](void *param)
            { static_cast<MicrophoneService *>(param)->recordTask(); },
            "MicRecordTask",
            4096,
            this,
            5, // High priority
            &m_taskHandle,
            1 // Run on Core 1 (UI is on Core 0)
        );

        Serial.printf("[MicrophoneService] Recording started → %s (caller: %s)\n", filePath.c_str(), caller);
        return true;
    }

    bool MicrophoneService::stopRecording(const char *caller, const char *reason)
    {
        uint32_t nowMs = millis();
        Serial.printf("[MicrophoneService] stopRecording() called by: %s, timestamp: %u ms, reason: %s\n", caller, nowMs, reason);

        // Allow saving if the task is still running OR if it auto-stopped but has data in the buffer
        if (!m_recording && m_bufferOffset == 0)
        {
            Serial.printf("[MicrophoneService] stopRecording() early return: not recording and 0 bufferOffset (caller: %s, reason: %s)\n", caller, reason);
            return false;
        }

        m_recording = false;
        m_saving = true;
        m_durationMs = nowMs - m_startMs;

        // Wait for background task to self-terminate
        if (m_taskHandle != nullptr)
        {
            vTaskDelay(pdMS_TO_TICKS(100)); // yield to task so it can exit
            m_taskHandle = nullptr;
        }

        bool isValidWav = false;
        uint32_t sz = 0;

        {
            SpiffsLock lock("MicrophoneService::stopRecording");

            Serial.printf("[MicrophoneService] Saving %u bytes from PSRAM to SPIFFS: %s...\n", (unsigned int)m_bufferOffset, m_filePath.c_str());

            const size_t neededBytes = m_bufferOffset + 44 + 4096; // data + header + safety margin
            int freedCount = 0;
            while ((SPIFFS.totalBytes() - SPIFFS.usedBytes()) < neededBytes && freedCount < 50)
            {
                File root = SPIFFS.open("/");
                String oldestName;
                time_t oldestTime = 0;
                File f = root.openNextFile();
                while (f)
                {
                    String fname = f.name();
                    if (fname.length() > 0 && fname[0] != '/')
                        fname = "/" + fname;
                    if (!f.isDirectory() && fname != String(m_filePath.c_str()) &&
                        (oldestTime == 0 || f.getLastWrite() < oldestTime))
                    {
                        oldestTime = f.getLastWrite();
                        oldestName = fname;
                    }
                    f = root.openNextFile();
                }
                root.close();

                if (oldestName.isEmpty())
                    break;

                Serial.printf("[MicrophoneService] Low on space (need %u bytes) — deleting oldest file: %s\n",
                              (unsigned int)neededBytes, oldestName.c_str());
                if (!SPIFFS.remove(oldestName))
                {
                    Serial.printf("[MicrophoneService] WARNING: SPIFFS.remove(%s) failed! Breaking cleanup loop.\n", oldestName.c_str());
                    break;
                }
                freedCount++;
            }
            Serial.printf("[MicrophoneService] SPIFFS free space before write: %u / %u bytes\n",
                          (unsigned int)(SPIFFS.totalBytes() - SPIFFS.usedBytes()), (unsigned int)SPIFFS.totalBytes());

            m_file = SPIFFS.open(m_filePath.c_str(), "w");
            if (!m_file)
            {
                Serial.printf("[MicrophoneService] ERROR: Failed to open SPIFFS file for writing: %s (caller: %s, reason: %s)\n", m_filePath.c_str(), caller, reason);
                m_saving = false;
                return false;
            }
            Serial.printf("[MicrophoneService] File successfully opened for writing: %s\n", m_filePath.c_str());

            bool hdrWritten = writeWavHeader(m_file, m_bufferOffset);
            if (hdrWritten)
            {
                Serial.printf("[MicrophoneService] WAV header written (44 bytes header, dataSize: %u bytes)\n", (unsigned int)m_bufferOffset);
            }
            else
            {
                Serial.printf("[MicrophoneService] ERROR: WAV header write failed for %s\n", m_filePath.c_str());
            }

            size_t written = 0;
            const size_t stagingSize = 4096;
            uint8_t *stagingBuf = (uint8_t *)malloc(stagingSize);
            if (!stagingBuf)
            {
                Serial.println("[MicrophoneService] ERROR: Failed to allocate staging buffer in internal RAM!");
                m_file.close();
                m_saving = false;
                return false;
            }
            bool writeError = false;
            while (written < m_bufferOffset)
            {
                size_t toWrite = std::min(stagingSize, m_bufferOffset - written);
                memcpy(stagingBuf, m_psramBuffer + written, toWrite);
                size_t bytesWritten = m_file.write(stagingBuf, toWrite);
                if (bytesWritten != toWrite)
                {
                    Serial.printf("[MicrophoneService] ERROR: file.write() mismatch! Expected bytes: %u, Actual bytes: %u, ESP error: SPIFFS write failed\n",
                                  (unsigned int)toWrite, (unsigned int)bytesWritten);
                    writeError = true;
                    break;
                }
                written += bytesWritten;
                if ((written % 32768) == 0 || written == m_bufferOffset)
                {
                    Serial.printf("[MicrophoneService] SPIFFS write progress: %u/%u bytes\n",
                                  (unsigned int)written, (unsigned int)m_bufferOffset);
                }
            }
            free(stagingBuf);

            m_file.flush();
            Serial.println("[MicrophoneService] file.flush() executed");

            size_t sizeBeforeClose = m_file.size();
            Serial.printf("[MicrophoneService] Final file size before closing: %u bytes\n", (unsigned int)sizeBeforeClose);

            m_file.close();
            Serial.println("[MicrophoneService] File closed successfully");

            File checkFile = SPIFFS.open(m_filePath.c_str(), "r");
            if (checkFile)
            {
                sz = checkFile.size();
                if (sz >= 44)
                {
                    uint8_t hdr[44];
                    checkFile.read(hdr, 44);
                    bool riffOk = (memcmp(&hdr[0], "RIFF", 4) == 0);
                    bool waveOk = (memcmp(&hdr[8], "WAVE", 4) == 0);
                    bool fmtOk = (memcmp(&hdr[12], "fmt ", 4) == 0);
                    bool dataOk = (memcmp(&hdr[36], "data", 4) == 0);
                    uint32_t declaredDataSize;
                    memcpy(&declaredDataSize, &hdr[40], 4);
                    bool sizeOk = (sz == 44 + declaredDataSize);
                    isValidWav = riffOk && waveOk && fmtOk && dataOk && sizeOk;
                    if (!sizeOk)
                    {
                        Serial.printf("[MicrophoneService] ERROR: WAV truncated — header declares %u data bytes but file only has %u\n",
                                      (unsigned int)declaredDataSize, (unsigned int)(sz - 44));
                    }
                }
                checkFile.close();
            }
        } // SpiffsLock released here

        m_saving = false;
        m_bufferOffset = 0; // Clear buffer so any duplicate stopRecording calls exit safely

        Serial.printf("[MicrophoneService] Final file size after close: %u bytes for %s\n", (unsigned int)sz, m_filePath.c_str());

        if (isValidWav)
        {
            Serial.printf("[MicrophoneService] WAV file validation SUCCESS: %s (%u bytes, %u ms duration)\n",
                          m_filePath.c_str(), sz, m_durationMs);
        }
        else
        {
            Serial.printf("[MicrophoneService] WAV file validation ERROR (invalid format/size): %s (%u bytes)\n",
                          m_filePath.c_str(), sz);
        }

        return isValidWav;
    }

    uint32_t MicrophoneService::getDurationMs() const
    {
        if (m_recording)
            return millis() - m_startMs;
        return m_durationMs;
    }

    void MicrophoneService::recordTask()
    {
        Serial.println("[MicrophoneService] recordTask background thread started");
        constexpr int BUFFER_SAMPLES = 256;

        int32_t rawBuffer[BUFFER_SAMPLES];
        int16_t pcmBuffer[BUFFER_SAMPLES];

        i2s_zero_dma_buffer(I2S_NUM_0);

        size_t totalReads = 0;
        size_t totalBytesReadCount = 0;
        const char *exitReason = "m_recording flag became false";

        while (m_recording)
        {
            size_t bytesRead = 0;

            esp_err_t err = i2s_read(
                I2S_NUM_0,
                rawBuffer,
                sizeof(rawBuffer),
                &bytesRead,
                portMAX_DELAY);

            totalReads++;

            if (err == ESP_OK && bytesRead > 0)
            {
                totalBytesReadCount += bytesRead;

                if (totalReads == 1 || totalReads % 100 == 0)
                {
                    Serial.printf("[MicrophoneService] i2s_read() result: err=0 (ESP_OK), bytesRead=%u, Total bytesRead=%u, m_bufferOffset=%u\n",
                                  (unsigned int)bytesRead, (unsigned int)totalBytesReadCount, (unsigned int)m_bufferOffset);
                }

                int sampleCount = bytesRead / sizeof(int32_t);

                for (int i = 0; i < sampleCount; i++)
                {
                    int32_t sample = rawBuffer[i];

                    // Extract the upper 16 bits from the 32-bit sample
                    sample = sample >> 16;

                    // Clamp
                    if (sample > 32767)
                        sample = 32767;
                    if (sample < -32768)
                        sample = -32768;

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
                    exitReason = "Audio Buffer limit reached";
                    Serial.println("[MicrophoneService] Audio Buffer limit reached! Auto-stopping task.");
                    m_recording = false;
                    break;
                }
            }
            else
            {
                Serial.printf("[MicrophoneService] i2s_read() result: err=%d, bytesRead=%u\n", (int)err, (unsigned int)bytesRead);
                if (err != ESP_OK)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }

        Serial.printf("[MicrophoneService] recordTask thread exiting. Exact reason: %s. Final bufferOffset (Total recorded bytes): %u\n",
                      exitReason, (unsigned int)m_bufferOffset);
        vTaskDelete(nullptr);
    }

    // WAV Header format
    struct WavHeader
    {
        char riff[4]{'R', 'I', 'F', 'F'};
        uint32_t chunkSize{0};
        char wave[4]{'W', 'A', 'V', 'E'};
        char fmt[4]{'f', 'm', 't', ' '};
        uint32_t subChunk1Size{16};
        uint16_t audioFormat{1};    // PCM
        uint16_t numChannels{1};    // Mono
        uint32_t sampleRate{16000}; // 16 kHz
        uint32_t byteRate{32000};   // 16000 * 1 * 2
        uint16_t blockAlign{2};     // 1 * 2
        uint16_t bitsPerSample{16}; // 16-bit
        char data[4]{'d', 'a', 't', 'a'};
        uint32_t subChunk2Size{0};
    };

    bool MicrophoneService::writeWavHeader(File &file, uint32_t dataSize)
    {
        WavHeader header;
        header.subChunk2Size = dataSize;
        header.chunkSize = 36 + dataSize;

        size_t written = file.write(reinterpret_cast<const uint8_t *>(&header), sizeof(WavHeader));
        return written == sizeof(WavHeader);
    }
}