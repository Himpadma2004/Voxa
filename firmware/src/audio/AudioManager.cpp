#include "AudioManager.h"
#include "../storage/StorageManager.h"
#include <cmath>
#include <algorithm>

namespace VOXA
{
    AudioManager& AudioManager::instance()
    {
        static AudioManager inst;
        return inst;
    }

    AudioManager::AudioManager()
    {
    }

    AudioManager::~AudioManager()
    {
        stop();
        if (m_initialized)
        {
            i2s_driver_uninstall(AUDIO_I2S_PORT);
        }
    }

    bool AudioManager::initI2S()
    {
        if (m_initialized)
        {
            return true;
        }

        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = AUDIO_SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Duplicate audio to both Left & Right channels for MAX98357A
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 512,
            .use_apll = false,
            .tx_desc_auto_clear = true,
            .fixed_mclk = 0
        };

        i2s_pin_config_t pin_config = {
            .bck_io_num = AUDIO_I2S_BCLK_PIN,
            .ws_io_num = AUDIO_I2S_LRC_PIN,
            .data_out_num = AUDIO_I2S_DOUT_PIN,
            .data_in_num = I2S_PIN_NO_CHANGE
        };

        esp_err_t err = i2s_driver_install(AUDIO_I2S_PORT, &i2s_config, 0, NULL);
        if (err != ESP_OK)
        {
            Serial.printf("[Audio] ERROR:\nI2S initialization failed (install error %d)\n", err);
            return false;
        }

        err = i2s_set_pin(AUDIO_I2S_PORT, &pin_config);
        if (err != ESP_OK)
        {
            Serial.printf("[Audio] ERROR:\nI2S initialization failed (pin config error %d)\n", err);
            i2s_driver_uninstall(AUDIO_I2S_PORT);
            return false;
        }

        i2s_zero_dma_buffer(AUDIO_I2S_PORT);
        m_initialized = true;
        return true;
    }

    bool AudioManager::begin()
    {
        Serial.println("[Audio] Initializing...");

        if (!initI2S())
        {
            return false;
        }

        Serial.println("[Audio] I2S Ready");
        Serial.println("[Audio] MAX98357A Ready");

        // Play Voxa signature startup boot melody: C5 (523Hz) -> E5 (659Hz) -> G5 (784Hz) -> C6 (1046Hz)
        playTone(523, 90);
        delay(20);
        playTone(659, 90);
        delay(20);
        playTone(784, 90);
        delay(20);
        playTone(1046, 220);

        Serial.println("[Audio] Startup boot melody played");
        return true;
    }

    void AudioManager::setVolume(uint8_t volume)
    {
        m_volume = std::min((uint8_t)100, volume);
        Serial.printf("[Audio] Volume set to %u%%\n", m_volume);
    }

    void AudioManager::applyVolumeScaling(int16_t* samples, size_t count)
    {
        if (m_volume >= 100)
        {
            return;
        }
        float scale = m_volume / 100.0f;
        for (size_t i = 0; i < count; ++i)
        {
            samples[i] = (int16_t)(samples[i] * scale);
        }
    }

    bool AudioManager::writePCMChunk(const int16_t* samples, size_t sampleCount)
    {
        if (!m_initialized || !samples || sampleCount == 0)
        {
            return false;
        }

        // Duplicate each 16-bit mono sample into a stereo pair (L, R)
        // so MAX98357A receives data on both Left and Right clock cycles
        std::vector<int16_t> stereoBuffer;
        stereoBuffer.reserve(sampleCount * 2);

        float scale = m_volume / 100.0f;
        for (size_t i = 0; i < sampleCount; ++i)
        {
            int16_t val = (int16_t)(samples[i] * scale);
            stereoBuffer.push_back(val); // Left channel
            stereoBuffer.push_back(val); // Right channel
        }

        size_t bytesWritten = 0;
        size_t bytesToWrite = stereoBuffer.size() * sizeof(int16_t);

        esp_err_t err = i2s_write(
            AUDIO_I2S_PORT,
            stereoBuffer.data(),
            bytesToWrite,
            &bytesWritten,
            portMAX_DELAY
        );

        if (err != ESP_OK)
        {
            Serial.printf("[Audio] ERROR:\nPlayback failed (i2s_write error %d)\n", err);
            return false;
        }
        return true;
    }

    bool AudioManager::playPCM(const int16_t* samples, size_t sampleCount)
    {
        if (!m_initialized || !samples || sampleCount == 0)
        {
            Serial.println("[Audio] ERROR:\nPlayback failed (invalid PCM data)");
            return false;
        }

        m_isPlaying = true;
        bool ok = writePCMChunk(samples, sampleCount);
        m_isPlaying = false;
        return ok;
    }

    bool AudioManager::playTone(uint16_t frequency, uint16_t durationMs)
    {
        if (!m_initialized || frequency == 0 || durationMs == 0)
        {
            return false;
        }

        m_isPlaying = true;
        size_t totalSamples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
        constexpr size_t CHUNK_SIZE = 256;

        std::vector<int16_t> chunk(CHUNK_SIZE);
        double phaseIncrement = (2.0 * M_PI * frequency) / AUDIO_SAMPLE_RATE;
        double phase = 0.0;

        size_t samplesRemaining = totalSamples;
        while (samplesRemaining > 0 && m_isPlaying)
        {
            size_t count = std::min(CHUNK_SIZE, samplesRemaining);
            for (size_t i = 0; i < count; ++i)
            {
                chunk[i] = (int16_t)(sin(phase) * 28000.0); // 28000 peak amplitude to avoid clipping
                phase += phaseIncrement;
                if (phase >= 2.0 * M_PI)
                {
                    phase -= 2.0 * M_PI;
                }
            }

            writePCMChunk(chunk.data(), count);
            samplesRemaining -= count;
        }

        m_isPlaying = false;
        return true;
    }

    bool AudioManager::playWavAsync(const std::string& path)
    {
        stop(); // Stop active playback and delete any existing task

        static std::string s_asyncPath;
        s_asyncPath = path;

        xTaskCreatePinnedToCore(
            [](void* param)
            {
                const char* filePath = static_cast<const char*>(param);
                AudioManager::instance().playWav(std::string(filePath));
                AudioManager::instance().m_audioTaskHandle = nullptr;
                vTaskDelete(NULL);
            },
            "AudioAsync",
            8192,
            (void*)s_asyncPath.c_str(),
            1,
            &m_audioTaskHandle,
            0 // Pin to Core 0 for background audio streaming
        );

        return true;
    }

    bool AudioManager::playWav(const std::string& path)
    {
        if (!m_initialized)
        {
            Serial.println("[Audio] ERROR:\nPlayback failed (audio engine not initialized)");
            return false;
        }

        FS* pFs = &storageManager.getFSForPath(path.c_str());
        const char* relPath = path.c_str();

        if (strncmp(path.c_str(), "/spiffs", 7) == 0)
        {
            pFs = &static_cast<FS&>(SPIFFS);
            relPath = path.c_str() + 7; // strip "/spiffs" prefix for SPIFFS.open()
        }

        File file = pFs->open(relPath, "r");
        if (!file && pFs != &static_cast<FS&>(SPIFFS))
        {
            // Fallback: check SPIFFS if not found on primary FS
            pFs = &static_cast<FS&>(SPIFFS);
            relPath = (strncmp(path.c_str(), "/spiffs", 7) == 0) ? path.c_str() + 7 : path.c_str();
            file = pFs->open(relPath, "r");
        }

        if (!file)
        {
            Serial.printf("[Audio] ERROR:\nPlayback failed (file not found: %s)\n", path.c_str());
            return false;
        }

        // Basic WAV Header Parsing (44 bytes standard header)
        uint8_t header[44];
        if (file.read(header, 44) != 44)
        {
            Serial.println("[Audio] ERROR:\nPlayback failed (invalid WAV header)");
            file.close();
            return false;
        }

        // Verify "RIFF" and "WAVE" signatures
        if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F' ||
            header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E')
        {
            Serial.println("[Audio] ERROR:\nPlayback failed (not a valid WAV file)");
            file.close();
            return false;
        }

        m_isPlaying = true;
        constexpr size_t BUFFER_SAMPLES = 512;
        int16_t pcmBuffer[BUFFER_SAMPLES];

        while (file.available() && m_isPlaying)
        {
            size_t bytesRead = file.read((uint8_t*)pcmBuffer, BUFFER_SAMPLES * sizeof(int16_t));
            size_t samplesRead = bytesRead / sizeof(int16_t);
            if (samplesRead > 0)
            {
                writePCMChunk(pcmBuffer, samplesRead);
            }
        }

        file.close();
        m_isPlaying = false;
        Serial.printf("[Audio] Finished playing WAV file: %s\n", path.c_str());
        return true;
    }

    void AudioManager::stop()
    {
        m_isPlaying = false;
        if (m_audioTaskHandle != nullptr)
        {
            vTaskDelete(m_audioTaskHandle);
            m_audioTaskHandle = nullptr;
        }
        if (m_initialized)
        {
            i2s_zero_dma_buffer(AUDIO_I2S_PORT);
        }
        Serial.println("[Audio] Playback stopped.");
    }

    bool AudioManager::runDiagnostics()
    {
        Serial.println("[Audio] Running audio diagnostics...");

        bool i2sOk = initI2S();
        bool dmaOk = m_initialized;
        bool spkOk = m_initialized;

        // Play diagnostic sweep sequence: 500 Hz -> 700 Hz -> 900 Hz
        bool playOk = playTone(500, 100);
        delay(30);
        playOk &= playTone(700, 100);
        delay(30);
        playOk &= playTone(900, 100);

        Serial.println("\n========== AUDIO DIAGNOSTICS ==========");
        Serial.printf("I2S Initialized : %s\n", i2sOk ? "PASS" : "FAIL");
        Serial.printf("DMA Allocated   : %s\n", dmaOk ? "PASS" : "FAIL");
        Serial.printf("Speaker Enabled : %s\n", spkOk ? "PASS" : "FAIL");
        Serial.printf("Playback Test   : %s\n", playOk ? "PASS" : "FAIL");
        Serial.println("=======================================\n");

        return i2sOk && dmaOk && spkOk && playOk;
    }

} // namespace VOXA
