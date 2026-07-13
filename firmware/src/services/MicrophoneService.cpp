#include "MicrophoneService.h"
#include <driver/i2s.h>
#include <SPIFFS.h>
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
        if (m_initialized) return true;

        Serial.println("[MicrophoneService] Initializing I2S...");

        // Setup I2S configuration for I2S microphone (e.g., INMP441)
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
            .bck_io_num = 4,        // BCLK pin 4
            .ws_io_num = 5,         // LRCK pin 5
            .data_out_num = -1,     // Not used
            .data_in_num = 7        // SD pin 6
        };

        Serial.println("[MicrophoneService] Configuring I2S parameters:");
        Serial.printf("  - Sample Rate: %d Hz\n", i2s_config.sample_rate);
        Serial.printf("  - Bits per Sample: %d-bit\n", i2s_config.bits_per_sample);
        Serial.printf("  - Mode: RX Master\n");
        Serial.printf("  - Format: Mono\n");
        Serial.printf("  - Pin Mapping: BCLK=GPIO %d, LRCK=GPIO %d, SD=GPIO %d\n",
                      pin_config.bck_io_num, pin_config.ws_io_num, pin_config.data_in_num);

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


    bool MicrophoneService::startRecording(const std::string& filePath)
    {
        if (m_recording) return false;
        if (!m_initialized && !begin()) return false;

        m_filePath = filePath;
        
        m_file = SPIFFS.open(filePath.c_str(), "w");
        if (!m_file)
        {
            Serial.printf("[MicrophoneService] Failed to open file for writing: %s\n", filePath.c_str());
            return false;
        }

        // Write dummy WAV header first (will update it on stop)
        writeWavHeader(m_file, 0);

        m_startMs = millis();
        m_durationMs = 0;
        m_recording = true;

        // Spawn high-priority background audio reader task
        xTaskCreatePinnedToCore(
            [](void* param) { static_cast<MicrophoneService*>(param)->recordTask(); },
            "MicRecordTask",
            4096,
            this,
            5, // High priority
            &m_taskHandle,
            1  // Run on Core 1 (UI is on Core 0)
        );

        Serial.printf("[MicrophoneService] Recording started → %s\n", filePath.c_str());
        return true;
    }

    bool MicrophoneService::stopRecording()
    {
        if (!m_recording) return false;

        m_recording = false;
        m_durationMs = millis() - m_startMs;

        // Wait for background task to self-terminate
        if (m_taskHandle != nullptr)
        {
            delay(100);
            m_taskHandle = nullptr;
        }

        if (m_file)
        {
            uint32_t dataSize = m_file.size() - 44;
            // Seek to start and rewrite the correct WAV header with actual data size
            m_file.seek(0);
            writeWavHeader(m_file, dataSize);
            m_file.close();
            
            // Validate recorded WAV file on SPIFFS
            File checkFile = SPIFFS.open(m_filePath.c_str(), "r");
            bool isValidWav = false;
            uint32_t sz = 0;
            if (checkFile)
            {
                sz = checkFile.size();
                if (sz >= 44)
                {
                    uint8_t hdr[44];
                    checkFile.read(hdr, 44);
                    // Confirm standard riff chunk markers
                    bool riffOk = (memcmp(&hdr[0], "RIFF", 4) == 0);
                    bool waveOk = (memcmp(&hdr[8], "WAVE", 4) == 0);
                    bool fmtOk  = (memcmp(&hdr[12], "fmt ", 4) == 0);
                    bool dataOk = (memcmp(&hdr[36], "data", 4) == 0);
                    isValidWav  = riffOk && waveOk && fmtOk && dataOk;
                }
                checkFile.close();
            }

            if (isValidWav)
            {
                Serial.printf("[MicrophoneService] WAV file validation SUCCESS: %s (%u bytes, %u ms)\n",
                              m_filePath.c_str(), sz, m_durationMs);
            }
            else
            {
                Serial.printf("[MicrophoneService] WAV file validation ERROR (invalid format/size): %s (%u bytes)\n",
                              m_filePath.c_str(), sz);
            }
        }

        return true;
    }


    uint32_t MicrophoneService::getDurationMs() const
    {
        if (m_recording) return millis() - m_startMs;
        return m_durationMs;
    }

    void MicrophoneService::recordTask()
{
    constexpr int BUFFER_SAMPLES = 256;

    int32_t rawBuffer[BUFFER_SAMPLES];
    int16_t pcmBuffer[BUFFER_SAMPLES];

    i2s_zero_dma_buffer(I2S_NUM_0);

    while (m_recording)
    {
        size_t bytesRead = 0;

        esp_err_t err = i2s_read(
            I2S_NUM_0,
            rawBuffer,
            sizeof(rawBuffer),
            &bytesRead,
            portMAX_DELAY);
        static bool once = true;

        // if (once)
        // {
        //     once = false;

        //     Serial.println("===== RAW I2S SAMPLES =====");

        //     for (int i = 0; i < 50; i++)
        //     {
        //         Serial.println(rawBuffer[i]);
        //     }

        //     Serial.println("===========================");
        // }

        if (err == ESP_OK && bytesRead > 0)
        {
            int sampleCount = bytesRead / sizeof(int32_t);

            for (int i = 0; i < sampleCount; i++)
            {
                int32_t sample = rawBuffer[i];

                // Extract the upper 16 bits from the 32-bit sample
                sample = sample >> 16;

                // Clamp
                if (sample > 32767) sample = 32767;
                if (sample < -32768) sample = -32768;

                pcmBuffer[i] = (int16_t)sample;
            }

            m_file.write(
                (uint8_t*)pcmBuffer,
                sampleCount * sizeof(int16_t));
        }
    }

    vTaskDelete(nullptr);
}

    // WAV Header format
    struct WavHeader
    {
        char     riff[4]          { 'R', 'I', 'F', 'F' };
        uint32_t chunkSize        { 0 };
        char     wave[4]          { 'W', 'A', 'V', 'E' };
        char     fmt[4]           { 'f', 'm', 't', ' ' };
        uint32_t subChunk1Size    { 16 };
        uint16_t audioFormat      { 1 }; // PCM
        uint16_t numChannels      { 1 }; // Mono
        uint32_t sampleRate       { 16000 }; // 16 kHz
        uint32_t byteRate         { 32000 }; // 16000 * 1 * 2
        uint16_t blockAlign       { 2 }; // 1 * 2
        uint16_t bitsPerSample    { 16 }; // 16-bit
        char     data[4]          { 'd', 'a', 't', 'a' };
        uint32_t subChunk2Size    { 0 };
    };

    bool MicrophoneService::writeWavHeader(File& file, uint32_t dataSize)
    {
        WavHeader header;
        header.subChunk2Size = dataSize;
        header.chunkSize = 36 + dataSize;
        
        size_t written = file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(WavHeader));
        return written == sizeof(WavHeader);
    }
}
