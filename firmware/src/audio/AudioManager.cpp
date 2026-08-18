#include "AudioManager.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
#include "../services/ApiClient.h"

extern VOXA::ApiClient apiClient;

namespace VOXA
{
    AudioManager &AudioManager::instance()
    {
        static AudioManager s_instance;
        return s_instance;
    }

    AudioManager::AudioManager()
    {
    }

    AudioManager::~AudioManager()
    {
        stop();
        uninstallI2SDriver();
    }

    bool AudioManager::initI2SDriver()
    {
        if (m_initialized)
        {
            return true;
        }

        Serial.println("\n[AudioManager] Installing Brand New MAX98357A I2S Driver...");
        Serial.printf("  - Controller : I2S_NUM_1\n");
        Serial.printf("  - BCLK Pin   : GPIO %d\n", AUDIO_I2S_BCLK_PIN);
        Serial.printf("  - LRC/WS Pin : GPIO %d\n", AUDIO_I2S_LRC_PIN);
        Serial.printf("  - DOUT Pin   : GPIO %d\n", AUDIO_I2S_DOUT_PIN);
        Serial.printf("  - Sample Rate: %d Hz\n", AUDIO_SAMPLE_RATE);

        // Standard Philips I2S TX Configuration (44.1kHz, 16-bit, Stereo DMA)
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = AUDIO_SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 256,
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
            Serial.printf("[AudioManager] ERROR: i2s_driver_install failed: %s\n", esp_err_to_name(err));
            return false;
        }

        err = i2s_set_pin(AUDIO_I2S_PORT, &pin_config);
        if (err != ESP_OK)
        {
            Serial.printf("[AudioManager] ERROR: i2s_set_pin failed: %s\n", esp_err_to_name(err));
            i2s_driver_uninstall(AUDIO_I2S_PORT);
            return false;
        }

        // Set maximum GPIO drive strength for sharp square-wave clock and data signals
        gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_BCLK_PIN, GPIO_DRIVE_CAP_3);
        gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_LRC_PIN, GPIO_DRIVE_CAP_3);
        gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_DOUT_PIN, GPIO_DRIVE_CAP_3);

        i2s_zero_dma_buffer(AUDIO_I2S_PORT);
        m_initialized = true;
        Serial.println("[AudioManager] MAX98357A I2S Driver successfully installed and DMA ready!");
        return true;
    }

    void AudioManager::uninstallI2SDriver()
    {
        if (m_initialized)
        {
            i2s_zero_dma_buffer(AUDIO_I2S_PORT);
            i2s_driver_uninstall(AUDIO_I2S_PORT);
            m_initialized = false;
            Serial.println("[AudioManager] I2S Driver uninstalled.");
        }
    }

    bool AudioManager::begin()
    {
        if (!initI2SDriver())
        {
            return false;
        }

        // Play short 44.1kHz power-up acoustic tone (880 Hz -> 1760 Hz)
        Serial.println("[AudioManager] Playing bootup chime...");
        playTone(880, 100);
        delay(20);
        playTone(1760, 150);
        return true;
    }

    void AudioManager::stop()
    {
        m_isPlaying = false;
        m_reminderPlaying = false;

        if (m_initialized)
        {
            i2s_zero_dma_buffer(AUDIO_I2S_PORT);
        }

        if (m_playbackTaskHandle != nullptr)
        {
            vTaskDelete(m_playbackTaskHandle);
            m_playbackTaskHandle = nullptr;
        }

        if (m_reminderTaskHandle != nullptr)
        {
            vTaskDelete(m_reminderTaskHandle);
            m_reminderTaskHandle = nullptr;
        }
    }

    void AudioManager::setVolume(uint8_t volume)
    {
        m_volume = std::min((uint8_t)100, volume);
        Serial.printf("[AudioManager] Volume set to %u%%\n", m_volume);
    }

    bool AudioManager::writePCMChunk(const int16_t *samples, size_t sampleCount)
    {
        if (!m_initialized || !samples || sampleCount == 0)
        {
            return false;
        }

        // Expand Mono samples into Stereo (L+R) for MAX98357A
        std::vector<int16_t> stereoBuffer;
        stereoBuffer.reserve(sampleCount * 2);

        float scale = m_volume / 100.0f;
        for (size_t i = 0; i < sampleCount; ++i)
        {
            int16_t sample = (m_volume >= 100) ? samples[i] : (int16_t)(samples[i] * scale);
            stereoBuffer.push_back(sample); // Left Channel
            stereoBuffer.push_back(sample); // Right Channel
        }

        size_t bytesToWrite = stereoBuffer.size() * sizeof(int16_t);
        size_t bytesWritten = 0;

        esp_err_t err = i2s_write(
            AUDIO_I2S_PORT,
            stereoBuffer.data(),
            bytesToWrite,
            &bytesWritten,
            portMAX_DELAY
        );

        if (err != ESP_OK)
        {
            Serial.printf("[AudioManager] ERROR: i2s_write failed (%s)\n", esp_err_to_name(err));
            return false;
        }

        return (bytesWritten == bytesToWrite);
    }

    bool AudioManager::playPCM(const int16_t *samples, size_t sampleCount)
    {
        if (!m_initialized || !samples || sampleCount == 0)
        {
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
                // Full scale 0 dBFS peak amplitude (32,700)
                chunk[i] = (int16_t)(sin(phase) * 32700.0);
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

    bool AudioManager::playUrl(const std::string &url)
    {
        if (!m_initialized || url.empty())
        {
            return false;
        }

        if (!WiFi.isConnected())
        {
            Serial.println("[AudioManager] WiFi not connected. Cannot stream audio URL.");
            return false;
        }

        Serial.printf("[AudioManager] Streaming audio from: %s\n", url.c_str());

        HTTPClient http;
        http.begin(url.c_str());
        http.setTimeout(10000);

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK)
        {
            Serial.printf("[AudioManager] HTTP GET failed (Code: %d)\n", httpCode);
            http.end();
            return false;
        }

        WiFiClient *stream = http.getStreamPtr();
        if (!stream)
        {
            Serial.println("[AudioManager] Stream pointer NULL");
            http.end();
            return false;
        }

        // Check if stream begins with a standard 44-byte WAV header
        uint8_t header[44];
        size_t headerBytes = stream->readBytes(header, 44);
        if (headerBytes == 44 && header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F')
        {
            Serial.println("[AudioManager] Verified RIFF/WAVE header. Streaming PCM payload...");
        }

        m_isPlaying = true;
        constexpr size_t BUFFER_SAMPLES = 512;
        int16_t pcmBuffer[BUFFER_SAMPLES];

        while (http.connected() && m_isPlaying)
        {
            if (stream->available() > 0)
            {
                size_t bytesRead = stream->readBytes((uint8_t *)pcmBuffer, BUFFER_SAMPLES * sizeof(int16_t));
                size_t samplesRead = bytesRead / sizeof(int16_t);
                if (samplesRead > 0)
                {
                    writePCMChunk(pcmBuffer, samplesRead);
                }
            }
            else
            {
                delay(2);
            }
        }

        http.end();
        m_isPlaying = false;
        Serial.println("[AudioManager] Audio stream finished.");
        return true;
    }

    bool AudioManager::playUrlAsync(const std::string &url)
    {
        stop();
        static std::string s_asyncUrl;
        s_asyncUrl = url;

        xTaskCreatePinnedToCore(
            [](void *param)
            {
                const char *streamUrl = static_cast<const char *>(param);
                AudioManager::instance().playUrl(std::string(streamUrl));
                AudioManager::instance().m_playbackTaskHandle = nullptr;
                vTaskDelete(NULL);
            },
            "AudioStreamTask",
            8192,
            (void *)s_asyncUrl.c_str(),
            2,
            &m_playbackTaskHandle,
            0
        );

        return true;
    }

    bool AudioManager::playReminderMusicAsync()
    {
        if (m_reminderPlaying)
        {
            return true;
        }

        stop();
        m_reminderPlaying = true;

        xTaskCreatePinnedToCore(
            [](void *param)
            {
                while (AudioManager::instance().m_reminderPlaying)
                {
                    if (WiFi.isConnected())
                    {
                        std::string reminderUrl = VOXA::apiClient.getBaseUrl() + "/api/music/reminder";
                        AudioManager::instance().playUrl(reminderUrl);
                    }
                    else
                    {
                        // Offline alert melody
                        AudioManager::instance().playTone(1000, 200);
                        delay(100);
                        AudioManager::instance().playTone(1500, 300);
                    }
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                AudioManager::instance().m_reminderTaskHandle = nullptr;
                vTaskDelete(NULL);
            },
            "ReminderAudioTask",
            8192,
            nullptr,
            2,
            &m_reminderTaskHandle,
            0
        );

        return true;
    }

    void AudioManager::stopReminderMusic()
    {
        m_reminderPlaying = false;
        stop();
    }

} // namespace VOXA
