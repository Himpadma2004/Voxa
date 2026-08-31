#include "AudioManager.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <cmath>
#include <Preferences.h>
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
        m_i2sMutex = xSemaphoreCreateMutex();
    }

    AudioManager::~AudioManager()
    {
        stop();
        uninstallI2SDriver();
        if (m_i2sMutex)
        {
            vSemaphoreDelete(m_i2sMutex);
            m_i2sMutex = nullptr;
        }
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

        // Standard Philips I2S TX Configuration (44.1kHz, 16-bit, Stereo DMA with deep buffer)
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = AUDIO_SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 16,
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

        // Load persisted volume setting from NVS
        Preferences prefs;
        if (prefs.begin("audio_cfg", true))
        {
            m_volume = prefs.getUChar("volume", 100);
            prefs.end();
            Serial.printf("[AudioManager] Restored volume setting: %u%%\n", m_volume);
        }

        Serial.println("[AudioManager] Ready.");
        return true;
    }

    bool AudioManager::playBootMelody()
    {
        if (!m_initialized)
        {
            if (!begin()) return false;
        }

        Serial.println("[AudioManager] Playing Boot Melody (Warm Octave)...");
        // Soothing warm chord: G4 (392Hz) -> A4 (440Hz) -> C5 (523Hz) -> E5 (659Hz)
        uint16_t notes[] = { 392, 440, 523, 659 };
        uint16_t durations[] = { 100, 100, 120, 240 };
        for (int i = 0; i < 4; ++i)
        {
            playTone(notes[i], durations[i]);
            delay(15);
        }
        return true;
    }

    bool AudioManager::playConnectedChime()
    {
        if (!m_initialized)
        {
            if (!begin()) return false;
        }

        Serial.println("[AudioManager] Playing Connected Chime...");
        // Gentle cheerful chime: A4 (440Hz) -> E5 (659Hz)
        playTone(440, 90);
        delay(25);
        playTone(659, 180);
        return true;
    }

    void AudioManager::stop()
    {
        m_isPlaying = false;
        m_voiceStreamPlaying = false;
        m_reminderPlaying = false;

        // Give streaming tasks time to exit their loops gracefully
        uint32_t waitStart = millis();
        while ((m_playbackTaskHandle != nullptr || m_reminderTaskHandle != nullptr) && (millis() - waitStart < 200))
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (m_initialized)
        {
            i2s_zero_dma_buffer(AUDIO_I2S_PORT);
        }
    }

    void AudioManager::setVolume(uint8_t volume, bool saveToNvs)
    {
        m_volume = std::min((uint8_t)100, volume);
        if (saveToNvs)
        {
            Preferences prefs;
            if (prefs.begin("audio_cfg", false))
            {
                prefs.putUChar("volume", m_volume);
                prefs.end();
            }
            Serial.printf("[AudioManager] Volume persisted to NVS: %u%%\n", m_volume);
        }
    }

    bool AudioManager::writePCMChunk(const int16_t *samples, size_t sampleCount)
    {
        if (!m_initialized || !samples || sampleCount == 0)
        {
            return false;
        }

        if (m_i2sMutex && xSemaphoreTake(m_i2sMutex, pdMS_TO_TICKS(150)) != pdTRUE)
        {
            return false;
        }

        // Static stereo conversion buffer (avoids FreeRTOS heap alloc/free churn)
        static int16_t s_stereoBuffer[2048];
        size_t processed = 0;
        float scale = m_volume / 100.0f;

        while (processed < sampleCount)
        {
            size_t batch = std::min(sampleCount - processed, (size_t)1024);
            for (size_t i = 0; i < batch; ++i)
            {
                int32_t val = samples[processed + i];
                if (m_volume < 100)
                {
                    val = (int32_t)(val * scale);
                }
                
                if (val > 32767) val = 32767;
                if (val < -32768) val = -32768;

                int16_t sample = (int16_t)val;
                s_stereoBuffer[i * 2]     = sample; // Left Channel
                s_stereoBuffer[i * 2 + 1] = sample; // Right Channel
            }

            size_t bytesToWrite = batch * 2 * sizeof(int16_t);
            size_t bytesWritten = 0;

            esp_err_t err = i2s_write(
                AUDIO_I2S_PORT,
                s_stereoBuffer,
                bytesToWrite,
                &bytesWritten,
                pdMS_TO_TICKS(200)
            );

            if (err != ESP_OK || bytesWritten != bytesToWrite)
            {
                if (m_i2sMutex) xSemaphoreGive(m_i2sMutex);
                return false;
            }

            processed += batch;
        }

        if (m_i2sMutex) xSemaphoreGive(m_i2sMutex);
        return true;
    }

    bool AudioManager::playPCM(const int16_t *samples, size_t sampleCount)
    {
        if (!m_initialized || !samples || sampleCount == 0)
        {
            return false;
        }

        bool ok = writePCMChunk(samples, sampleCount);
        return ok;
    }

    bool AudioManager::playTone(uint16_t frequency, uint16_t durationMs)
    {
        if (!m_initialized || frequency == 0 || durationMs == 0 || m_volume == 0)
        {
            return false;
        }

        size_t totalSamples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
        constexpr size_t CHUNK_SIZE = 256;
        static int16_t chunk[CHUNK_SIZE];

        double phaseIncrement = (2.0 * M_PI * frequency) / AUDIO_SAMPLE_RATE;
        double phase = 0.0;

        size_t attackSamples = std::min((size_t)(AUDIO_SAMPLE_RATE * 0.008), totalSamples / 4);
        size_t decaySamples  = std::min((size_t)(AUDIO_SAMPLE_RATE * 0.008), totalSamples / 4);

        size_t samplesRemaining = totalSamples;
        size_t sampleIndex = 0;

        while (samplesRemaining > 0)
        {
            size_t count = std::min(CHUNK_SIZE, samplesRemaining);
            for (size_t i = 0; i < count; ++i)
            {
                float envelope = 1.0f;
                if (attackSamples > 0 && sampleIndex < attackSamples)
                {
                    envelope = (float)sampleIndex / (float)attackSamples;
                }
                else if (decaySamples > 0 && sampleIndex > totalSamples - decaySamples)
                {
                    envelope = (float)(totalSamples - sampleIndex) / (float)decaySamples;
                }

                // Calibrated 15,000 peak amplitude (Sweet spot for 1-2W micro speakers: loud & crystal-clear without cracking)
                float rawVal = sin(phase) * 15000.0f * envelope;
                if (rawVal > 28000.0f) rawVal = 28000.0f;
                if (rawVal < -28000.0f) rawVal = -28000.0f;

                chunk[i] = (int16_t)rawVal;
                phase += phaseIncrement;
                if (phase >= 2.0 * M_PI)
                {
                    phase -= 2.0 * M_PI;
                }
                sampleIndex++;
            }

            writePCMChunk(chunk, count);
            samplesRemaining -= count;
        }

        return true;
    }

    namespace
    {
        class HttpAudioStreamReader
        {
        public:
            HttpAudioStreamReader(WiFiClient* client, bool isChunked, size_t contentLength)
                : m_client(client), m_isChunked(isChunked), m_contentLength(contentLength),
                  m_chunkRemaining(0), m_totalBytesRead(0), m_eof(false)
            {
            }

            size_t read(uint8_t* dst, size_t len)
            {
                if (m_eof || !m_client || len == 0)
                {
                    return 0;
                }

                if (!m_isChunked)
                {
                    if (m_contentLength > 0 && m_totalBytesRead >= m_contentLength)
                    {
                        m_eof = true;
                        return 0;
                    }

                    size_t toRead = len;
                    if (m_contentLength > 0)
                    {
                        toRead = std::min(toRead, m_contentLength - m_totalBytesRead);
                    }

                    int bytesRead = m_client->read(dst, toRead);
                    if (bytesRead > 0)
                    {
                        m_totalBytesRead += bytesRead;
                        return (size_t)bytesRead;
                    }
                    else if (bytesRead < 0 || !m_client->connected())
                    {
                        m_eof = true;
                        return 0;
                    }
                    return 0;
                }

                size_t totalWritten = 0;
                while (totalWritten < len && !m_eof)
                {
                    if (m_chunkRemaining == 0)
                    {
                        if (!readNextChunkHeader())
                        {
                            m_eof = true;
                            break;
                        }
                    }

                    size_t wanted = std::min(len - totalWritten, m_chunkRemaining);
                    int r = m_client->read(dst + totalWritten, wanted);
                    if (r > 0)
                    {
                        totalWritten += r;
                        m_chunkRemaining -= r;
                        m_totalBytesRead += r;

                        if (m_chunkRemaining == 0)
                        {
                            consumeCRLF();
                        }
                    }
                    else if (r < 0 || !m_client->connected())
                    {
                        m_eof = true;
                        break;
                    }
                    else
                    {
                        // No bytes currently available in TCP buffer
                        break;
                    }
                }

                return totalWritten;
            }

            size_t readExact(uint8_t* dst, size_t len)
            {
                size_t total = 0;
                uint32_t retryStart = millis();
                while (total < len && !m_eof)
                {
                    size_t r = read(dst + total, len - total);
                    if (r > 0)
                    {
                        total += r;
                        retryStart = millis();
                    }
                    else
                    {
                        if (!m_client || !m_client->connected() || millis() - retryStart > 2000)
                        {
                            break;
                        }
                        vTaskDelay(pdMS_TO_TICKS(1));
                    }
                }
                return total;
            }

            bool isEof() const { return m_eof; }
            size_t getTotalBytesRead() const { return m_totalBytesRead; }
            bool isChunked() const { return m_isChunked; }

        private:
            bool readNextChunkHeader()
            {
                String line = "";
                uint32_t startMs = millis();
                while (millis() - startMs < 5000)
                {
                    if (m_client->available())
                    {
                        char c = (char)m_client->read();
                        if (c == '\n')
                        {
                            line.trim();
                            if (line.length() > 0)
                            {
                                break;
                            }
                        }
                        else
                        {
                            line += c;
                        }
                    }
                    else
                    {
                        if (!m_client->connected())
                        {
                            return false;
                        }
                        vTaskDelay(pdMS_TO_TICKS(1));
                    }
                }

                line.trim();
                if (line.length() == 0)
                {
                    return false;
                }

                int semiPos = line.indexOf(';');
                if (semiPos >= 0)
                {
                    line = line.substring(0, semiPos);
                    line.trim();
                }

                char* endPtr = nullptr;
                unsigned long chunkSize = strtoul(line.c_str(), &endPtr, 16);
                if (endPtr == line.c_str())
                {
                    return false;
                }

                if (chunkSize == 0)
                {
                    m_eof = true;
                    return false;
                }

                m_chunkRemaining = (size_t)chunkSize;
                return true;
            }

            void consumeCRLF()
            {
                uint32_t startMs = millis();
                while (millis() - startMs < 2000)
                {
                    if (m_client->available())
                    {
                        char c = (char)m_client->read();
                        if (c == '\n')
                        {
                            break;
                        }
                    }
                    else
                    {
                        if (!m_client->connected()) break;
                        vTaskDelay(pdMS_TO_TICKS(1));
                    }
                }
            }

            WiFiClient* m_client;
            bool m_isChunked;
            size_t m_contentLength;
            size_t m_chunkRemaining;
            size_t m_totalBytesRead;
            bool m_eof;
        };
    }

    bool AudioManager::playUrl(const std::string &url)
    {
        if (!m_initialized || url.empty())
        {
            return false;
        }

        if (!WiFi.isConnected())
        {
            return false;
        }

        Serial.printf("[AudioManager] Streaming audio from: %s\n", url.c_str());

        m_voiceStreamPlaying = true;
        m_isPlaying = true;
        vTaskDelay(pdMS_TO_TICKS(120)); // Allow background loop to pause and yield I2S

        HTTPClient http;
        http.begin(url.c_str());
        http.setTimeout(15000);
        http.setReuse(false);

        const char* headerKeys[] = {"Content-Type", "Content-Length", "Transfer-Encoding"};
        http.collectHeaders(headerKeys, 3);

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK)
        {
            Serial.printf("[AudioManager] HTTP GET failed (Code: %d)\n", httpCode);
            http.end();
            m_voiceStreamPlaying = false;
            m_isPlaying = false;
            return false;
        }

        String contentType = http.header("Content-Type");
        String contentLengthStr = http.header("Content-Length");
        String transferEncoding = http.header("Transfer-Encoding");

        bool isChunked = (transferEncoding.indexOf("chunked") >= 0);
        size_t contentLength = contentLengthStr.length() > 0 ? (size_t)contentLengthStr.toInt() : 0;

        WiFiClient *stream = http.getStreamPtr();
        if (!stream)
        {
            Serial.println("[AudioManager] Stream pointer NULL");
            http.end();
            m_voiceStreamPlaying = false;
            m_isPlaying = false;
            return false;
        }

        HttpAudioStreamReader reader(stream, isChunked, contentLength);

        // 1. Parse WAV RIFF Header from clean decoded byte stream
        uint8_t riffHead[12];
        size_t headRead = reader.read(riffHead, 12);
        if (headRead < 12 || memcmp(riffHead, "RIFF", 4) != 0 || memcmp(riffHead + 8, "WAVE", 4) != 0)
        {
            Serial.println("[AUDIO ERROR] Invalid WAV stream: RIFF/WAVE header not found");
            http.end();
            m_voiceStreamPlaying = false;
            m_isPlaying = false;
            return false;
        }

        uint16_t audioFormat = 1;
        uint16_t numChannels = 1;
        uint32_t wavSampleRate = 16000;
        uint32_t byteRate = 32000;
        uint16_t blockAlign = 2;
        uint16_t bitsPerSample = 16;
        uint32_t dataChunkSize = 0;
        uint32_t dataChunkOffset = 12;
        bool dataFound = false;

        // 2. Parse Subchunks until "data" chunk
        while (!reader.isEof())
        {
            uint8_t chunkHeader[8];
            if (reader.read(chunkHeader, 8) < 8) break;
            dataChunkOffset += 8;

            uint32_t chunkSize = (uint32_t)chunkHeader[4] | ((uint32_t)chunkHeader[5] << 8) |
                                 ((uint32_t)chunkHeader[6] << 16) | ((uint32_t)chunkHeader[7] << 24);

            if (memcmp(chunkHeader, "fmt ", 4) == 0)
            {
                uint8_t fmtData[16];
                size_t toReadFmt = std::min((size_t)chunkSize, sizeof(fmtData));
                if (reader.read(fmtData, toReadFmt) < toReadFmt) break;
                dataChunkOffset += toReadFmt;

                if (toReadFmt >= 16)
                {
                    audioFormat   = fmtData[0] | (fmtData[1] << 8);
                    numChannels   = fmtData[2] | (fmtData[3] << 8);
                    wavSampleRate = fmtData[4] | (fmtData[5] << 8) | (fmtData[6] << 16) | (fmtData[7] << 24);
                    byteRate      = fmtData[8] | (fmtData[9] << 8) | (fmtData[10] << 16) | (fmtData[11] << 24);
                    blockAlign    = fmtData[12] | (fmtData[13] << 8);
                    bitsPerSample = fmtData[14] | (fmtData[15] << 8);
                }
                if (chunkSize > toReadFmt)
                {
                    size_t skip = chunkSize - toReadFmt;
                    dataChunkOffset += skip;
                    while (skip > 0)
                    {
                        uint8_t dummy[64];
                        size_t toSkip = std::min(skip, sizeof(dummy));
                        size_t r = reader.read(dummy, toSkip);
                        if (r == 0) break;
                        skip -= r;
                    }
                }
            }
            else if (memcmp(chunkHeader, "data", 4) == 0)
            {
                dataChunkSize = chunkSize;
                dataFound = true;
                break;
            }
            else
            {
                // Skip metadata chunks (LIST, JUNK, PEAK, etc.)
                size_t skip = chunkSize;
                dataChunkOffset += skip;
                while (skip > 0)
                {
                    uint8_t dummy[64];
                    size_t toSkip = std::min(skip, sizeof(dummy));
                    size_t r = reader.read(dummy, toSkip);
                    if (r == 0) break;
                    skip -= r;
                }
            }
        }

        if (!dataFound || dataChunkSize == 0)
        {
            Serial.println("[AUDIO ERROR] Invalid WAV stream: 'data' chunk missing or empty");
            http.end();
            m_voiceStreamPlaying = false;
            m_isPlaying = false;
            return false;
        }

        if (audioFormat != 1 || bitsPerSample != 16 || (numChannels != 1 && numChannels != 2))
        {
            Serial.printf("[AUDIO ERROR] Unsupported audio format: Format=%u, Bits=%u, Channels=%u\n",
                          audioFormat, bitsPerSample, numChannels);
            http.end();
            m_voiceStreamPlaying = false;
            m_isPlaying = false;
            return false;
        }

        // Diagnostic logs
        Serial.println("\n[VOXA AUDIO]");
        Serial.printf("HTTP status: %d\n", httpCode);
        Serial.printf("Content-Type: %s\n", contentType.c_str());
        Serial.printf("Content-Length: %s\n", contentLengthStr.c_str());
        Serial.printf("Transfer-Encoding: %s\n", transferEncoding.c_str());
        if (isChunked)
        {
            Serial.println("HTTP chunked transfer detected");
        }
        Serial.println("WAV detected: RIFF/WAVE");
        Serial.printf("AudioFormat: %u (PCM)\n", audioFormat);
        Serial.printf("Channels: %u\n", numChannels);
        Serial.printf("SampleRate: %u Hz\n", wavSampleRate);
        Serial.printf("BitsPerSample: %u\n", bitsPerSample);
        Serial.printf("DataChunkOffset: %u\n", dataChunkOffset);
        Serial.printf("DataChunkSize: %u bytes\n", dataChunkSize);
        Serial.println("PCM playback started");

        // Diagnostic I2S Config
        uint32_t expectedBclk = wavSampleRate * 16 * 2;
        if (wavSampleRate == 44100)
        {
            Serial.println("\n[VOXA I2S 44K DIAGNOSTIC]");
            Serial.printf("WAV Sample Rate: %u\n", wavSampleRate);
            Serial.printf("Bits: %u\n", bitsPerSample);
            Serial.printf("Channels: %u\n", numChannels);
            Serial.printf("I2S Sample Rate Requested: %u\n", wavSampleRate);
            Serial.printf("Expected BCLK: %u\n", expectedBclk);
            Serial.printf("DMA buffers: 16\n");
            Serial.printf("DMA length: 512\n");
            Serial.printf("PCM input bytes: %u\n", dataChunkSize);
            Serial.printf("Stereo output bytes: %u\n", numChannels == 1 ? dataChunkSize * 2 : dataChunkSize);
        }
        else
        {
            Serial.println("\n[VOXA I2S CONFIG]");
            Serial.printf("Sample rate: %u Hz\n", wavSampleRate);
            Serial.printf("Bits per sample: 16\n");
            Serial.printf("Channels: %u\n", numChannels);
            Serial.printf("Channel format: RIGHT_LEFT (Stereo)\n");
            Serial.printf("Communication format: I2S_COMM_FORMAT_STAND_I2S (Philips)\n");
            Serial.printf("DMA configuration: 16 buffers x 512 samples\n");
            Serial.printf("BCLK: %u Hz\n", expectedBclk);
            Serial.printf("WS/LRCK: %u Hz\n", wavSampleRate);
        }

        // Safe I2S Clock Transition with Mutex Lock, Hardware Pause, and DMA Flush
        if (m_i2sMutex && xSemaphoreTake(m_i2sMutex, pdMS_TO_TICKS(300)) == pdTRUE)
        {
            i2s_stop(AUDIO_I2S_PORT);
            i2s_zero_dma_buffer(AUDIO_I2S_PORT);
            if (wavSampleRate >= 8000 && wavSampleRate <= 48000)
            {
                i2s_set_sample_rates(AUDIO_I2S_PORT, wavSampleRate);
            }
            gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_BCLK_PIN, GPIO_DRIVE_CAP_3);
            gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_LRC_PIN, GPIO_DRIVE_CAP_3);
            gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_DOUT_PIN, GPIO_DRIVE_CAP_3);
            i2s_zero_dma_buffer(AUDIO_I2S_PORT);
            i2s_start(AUDIO_I2S_PORT);
            xSemaphoreGive(m_i2sMutex);
        }

        constexpr size_t READ_SAMPLES = 512;
        static int16_t s_rawBuffer[READ_SAMPLES];
        static int16_t s_stereoBuffer[READ_SAMPLES * 2];

        uint32_t dataBytesRemaining = dataChunkSize;
        uint32_t totalPcmBytesPlayed = 0;
        uint32_t totalI2sWrites = 0;
        uint32_t totalBytesWritten = 0;
        uint32_t timeoutErrors = 0;
        uint32_t partialWriteErrors = 0;
        uint32_t playbackStartMs = millis();
        float scale = m_volume / 100.0f;

        while (http.connected() && m_voiceStreamPlaying && dataBytesRemaining > 0 && !reader.isEof())
        {
            size_t bytesWanted = std::min({(size_t)(READ_SAMPLES * sizeof(int16_t)), (size_t)dataBytesRemaining});
            // Ensure strict 2-byte sample boundary alignment
            size_t frameBytes = numChannels * sizeof(int16_t);
            if (frameBytes == 0) frameBytes = 2;
            bytesWanted -= (bytesWanted % frameBytes);
            if (bytesWanted == 0) break;

            size_t bytesRead = reader.readExact((uint8_t *)s_rawBuffer, bytesWanted);
            if (bytesRead == 0) break;

            dataBytesRemaining -= bytesRead;
            totalPcmBytesPlayed += bytesRead;

            size_t samplesRead = bytesRead / sizeof(int16_t);

            if (m_i2sMutex && xSemaphoreTake(m_i2sMutex, pdMS_TO_TICKS(150)) == pdTRUE)
            {
                if (numChannels == 1)
                {
                    // Mono -> Stereo expansion
                    for (size_t i = 0; i < samplesRead; ++i)
                    {
                        int32_t val = s_rawBuffer[i];
                        if (m_volume < 100) val = (int32_t)(val * scale);
                        if (val > 32767) val = 32767;
                        if (val < -32768) val = -32768;

                        int16_t s = (int16_t)val;
                        s_stereoBuffer[i * 2]     = s;
                        s_stereoBuffer[i * 2 + 1] = s;
                    }
                    size_t bytesToWrite = samplesRead * 2 * sizeof(int16_t);
                    size_t bytesWritten = 0;
                    totalI2sWrites++;
                    esp_err_t err = i2s_write(AUDIO_I2S_PORT, s_stereoBuffer, bytesToWrite, &bytesWritten, pdMS_TO_TICKS(300));
                    totalBytesWritten += bytesWritten;
                    if (err != ESP_OK)
                    {
                        timeoutErrors++;
                    }
                    else if (bytesWritten != bytesToWrite)
                    {
                        partialWriteErrors++;
                    }
                }
                else
                {
                    // Stereo native
                    for (size_t i = 0; i < samplesRead; ++i)
                    {
                        int32_t val = s_rawBuffer[i];
                        if (m_volume < 100) val = (int32_t)(val * scale);
                        if (val > 32767) val = 32767;
                        if (val < -32768) val = -32768;
                        s_rawBuffer[i] = (int16_t)val;
                    }
                    size_t bytesWritten = 0;
                    totalI2sWrites++;
                    esp_err_t err = i2s_write(AUDIO_I2S_PORT, s_rawBuffer, bytesRead, &bytesWritten, pdMS_TO_TICKS(300));
                    totalBytesWritten += bytesWritten;
                    if (err != ESP_OK)
                    {
                        timeoutErrors++;
                    }
                    else if (bytesWritten != bytesRead)
                    {
                        partialWriteErrors++;
                    }
                }

                if (m_i2sMutex) xSemaphoreGive(m_i2sMutex);
            }

            // Yield cooperatively to WiFi TCP stack
            taskYIELD();
        }

        uint32_t playbackDurationMs = millis() - playbackStartMs;
        uint32_t expectedDurationMs = (wavSampleRate > 0) ? (dataChunkSize * 1000ULL) / (wavSampleRate * numChannels * sizeof(int16_t)) : 0;

        Serial.printf("PCM bytes played: %u\n", totalPcmBytesPlayed);
        if (isChunked)
        {
            Serial.println("HTTP chunks decoded successfully");
        }

        Serial.printf("I2S writes: %u\n", totalI2sWrites);
        Serial.printf("Requested bytes: %u\n", numChannels == 1 ? totalPcmBytesPlayed * 2 : totalPcmBytesPlayed);
        Serial.printf("Written bytes: %u\n", totalBytesWritten);
        Serial.printf("Partial writes: %u\n", partialWriteErrors);
        Serial.printf("Timeouts: %u\n", timeoutErrors);
        Serial.printf("Playback duration: %u ms\n", playbackDurationMs);
        Serial.printf("Expected duration: %u ms\n", expectedDurationMs);

        http.end();
        // Restore default system sample rate for tones and melodies safely with mutex and hardware stop/start
        if (m_i2sMutex && xSemaphoreTake(m_i2sMutex, pdMS_TO_TICKS(300)) == pdTRUE)
        {
            i2s_stop(AUDIO_I2S_PORT);
            i2s_zero_dma_buffer(AUDIO_I2S_PORT);
            i2s_set_sample_rates(AUDIO_I2S_PORT, AUDIO_SAMPLE_RATE);
            gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_BCLK_PIN, GPIO_DRIVE_CAP_3);
            gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_LRC_PIN, GPIO_DRIVE_CAP_3);
            gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_DOUT_PIN, GPIO_DRIVE_CAP_3);
            i2s_zero_dma_buffer(AUDIO_I2S_PORT);
            i2s_start(AUDIO_I2S_PORT);
            xSemaphoreGive(m_i2sMutex);
        }

        m_voiceStreamPlaying = false;
        m_isPlaying = false;
        Serial.println("[AudioManager] Audio stream finished cleanly.");
        return true;
    }

    bool AudioManager::playUrlAsync(const std::string &url)
    {
        stop();
        std::string *pUrl = new std::string(url);

        xTaskCreatePinnedToCore(
            [](void *param)
            {
                std::string *pStreamUrl = static_cast<std::string *>(param);
                std::string urlStr = *pStreamUrl;
                delete pStreamUrl;

                AudioManager::instance().playUrl(urlStr);
                AudioManager::instance().m_playbackTaskHandle = nullptr;
                vTaskDelete(NULL);
            },
            "AudioStreamTask",
            8192,
            static_cast<void *>(pUrl),
            1,
            &m_playbackTaskHandle,
            1
        );

        return true;
    }

    bool AudioManager::playBackgroundMusicLoopAsync()
    {
        if (m_backgroundPlaying)
        {
            return true;
        }

        m_backgroundPlaying = true;

        xTaskCreatePinnedToCore(
            [](void *param)
            {
                Serial.println("[AudioManager] Background Song Loop Started (Canon in D Melody)...");

                struct Note {
                    uint16_t freq;
                    uint16_t duration;
                };

                // Canon in D Harmonic Song Melody (Calibrated Warm Mid-Octave for 8 Ohm 0.5W Speaker)
                static const Note s_bgSongNotes[] = {
                    // Section 1 - Warm Canon Theme
                    { 370, 320 }, { 330, 320 }, { 294, 320 }, { 277, 320 }, // F#4, E4, D4, C#4
                    { 247, 320 }, { 220, 320 }, { 247, 320 }, { 277, 320 }, // B3, A3, B3, C#4
                    { 294, 320 }, { 277, 320 }, { 247, 320 }, { 220, 320 }, // D4, C#4, B3, A3
                    { 196, 320 }, { 185, 320 }, { 196, 320 }, { 165, 320 }, // G3, F#3, G3, E3

                    // Section 2 - Resonant Ascending Chord Arpeggios
                    { 294, 220 }, { 370, 220 }, { 440, 220 }, { 587, 280 }, // D4, F#4, A4, D5
                    { 220, 220 }, { 277, 220 }, { 330, 220 }, { 440, 280 }, // A3, C#4, E4, A4
                    { 247, 220 }, { 294, 220 }, { 370, 220 }, { 494, 280 }, // B3, D4, F#4, B4
                    { 185, 220 }, { 220, 220 }, { 277, 220 }, { 370, 280 }, // F#3, A3, C#4, F#4
                    { 196, 220 }, { 247, 220 }, { 294, 220 }, { 392, 280 }, // G3, B3, D4, G4
                    { 294, 220 }, { 370, 220 }, { 440, 220 }, { 587, 280 }, // D4, F#4, A4, D5
                    { 196, 220 }, { 247, 220 }, { 294, 220 }, { 392, 280 }, // G3, B3, D4, G4
                    { 220, 220 }, { 277, 220 }, { 330, 220 }, { 440, 280 }, // A3, C#4, E4, A4

                    // Section 3 - Lyrical Echo
                    { 370, 180 }, { 294, 180 }, { 330, 180 }, { 277, 180 },
                    { 294, 180 }, { 247, 180 }, { 277, 180 }, { 220, 180 },
                    { 247, 180 }, { 196, 180 }, { 220, 180 }, { 185, 180 },
                    { 196, 180 }, { 165, 180 }, { 185, 180 }, { 294, 400 },

                    // Gentle Breath Pause
                    { 0, 800 }
                };

                const size_t noteCount = sizeof(s_bgSongNotes) / sizeof(s_bgSongNotes[0]);

                while (AudioManager::instance().m_backgroundPlaying)
                {
                    for (size_t i = 0; i < noteCount && AudioManager::instance().m_backgroundPlaying; ++i)
                    {
                        // Automatically pause background music whenever voice memo or reminder audio is playing!
                        if (AudioManager::instance().m_reminderPlaying || AudioManager::instance().m_voiceStreamPlaying)
                        {
                            vTaskDelay(pdMS_TO_TICKS(150));
                            continue;
                        }

                        uint16_t freq = s_bgSongNotes[i].freq;
                        uint16_t dur  = s_bgSongNotes[i].duration;

                        if (freq == 0)
                        {
                            vTaskDelay(pdMS_TO_TICKS(dur));
                        }
                        else
                        {
                            AudioManager::instance().playTone(freq, dur);
                            vTaskDelay(pdMS_TO_TICKS(18)); // Soft staccato note gap
                        }
                    }
                    vTaskDelay(pdMS_TO_TICKS(500));
                }

                AudioManager::instance().m_bgTaskHandle = nullptr;
                vTaskDelete(NULL);
            },
            "BgMusicTask",
            4096,
            nullptr,
            1,
            &m_bgTaskHandle,
            1
        );

        return true;
    }

    void AudioManager::stopBackgroundMusic()
    {
        m_backgroundPlaying = false;
        stop();
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
                    AudioManager::instance().playTone(880, 120);
                    vTaskDelay(pdMS_TO_TICKS(40));
                    AudioManager::instance().playTone(1175, 120);
                    vTaskDelay(pdMS_TO_TICKS(40));
                    AudioManager::instance().playTone(1318, 240);
                    vTaskDelay(pdMS_TO_TICKS(1200));
                }
                AudioManager::instance().m_reminderTaskHandle = nullptr;
                vTaskDelete(NULL);
            },
            "ReminderAudioTask",
            4096,
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

    bool AudioManager::runI2SSyntheticDiagnosticTest()
    {
        if (!m_initialized)
        {
            if (!begin()) return false;
        }

        Serial.println("\n============================================================");
        Serial.println(">>> VOXA AUDIO FORENSIC TEST: LOCAL I2S SINE WAVE ISOLATION <<<");
        Serial.println("============================================================");

        // 1. Isolate test from background music and other tasks
        m_voiceStreamPlaying = true;
        m_isPlaying = true;
        vTaskDelay(pdMS_TO_TICKS(150)); // Ensure BgMusicTask is paused

        // Helper lambda to run a synthetic tone at a given sample rate
        auto runSineTest = [this](uint32_t sampleRate, const char* testLabel)
        {
            uint32_t expectedBclk = sampleRate * 16 * 2;
            constexpr uint32_t DURATION_SEC = 3;
            uint32_t totalFrames = sampleRate * DURATION_SEC;
            uint32_t totalStereoBytes = totalFrames * 2 * sizeof(int16_t);

            Serial.printf("\n--- %s (%u Hz, 16-bit Stereo, 1000 Hz Sine, %u sec) ---\n", testLabel, sampleRate, DURATION_SEC);
            Serial.printf("Requested Sample Rate : %u Hz\n", sampleRate);
            Serial.printf("Bits per sample       : 16\n");
            Serial.printf("Slots/Channels        : 2 (Stereo Right/Left)\n");
            Serial.printf("Communication Format  : I2S_COMM_FORMAT_STAND_I2S (Philips)\n");
            Serial.printf("DMA Buffer Config     : 16 buffers x 512 samples\n");
            Serial.printf("Expected BCLK         : %u Hz\n", expectedBclk);
            Serial.printf("Expected WS/LRCK      : %u Hz\n", sampleRate);
            Serial.printf("Expected Stereo Frames: %u\n", totalFrames);
            Serial.printf("Expected Stereo Bytes : %u bytes\n", totalStereoBytes);
            Serial.printf("Expected Duration     : %u ms\n", DURATION_SEC * 1000);

            // Reconfigure I2S clock safely
            if (m_i2sMutex && xSemaphoreTake(m_i2sMutex, pdMS_TO_TICKS(500)) == pdTRUE)
            {
                i2s_stop(AUDIO_I2S_PORT);
                i2s_zero_dma_buffer(AUDIO_I2S_PORT);
                i2s_set_sample_rates(AUDIO_I2S_PORT, sampleRate);
                gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_BCLK_PIN, GPIO_DRIVE_CAP_3);
                gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_LRC_PIN, GPIO_DRIVE_CAP_3);
                gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_DOUT_PIN, GPIO_DRIVE_CAP_3);
                i2s_zero_dma_buffer(AUDIO_I2S_PORT);
                i2s_start(AUDIO_I2S_PORT);
                xSemaphoreGive(m_i2sMutex);
            }

            constexpr size_t BLOCK_SAMPLES = 512;
            int16_t stereoBlock[BLOCK_SAMPLES * 2];
            double phase = 0.0;
            double phaseIncrement = (2.0 * M_PI * 1000.0) / (double)sampleRate;
            constexpr float AMPLITUDE = 8000.0f; // 24% full scale, conservative non-clipping

            uint32_t framesRemaining = totalFrames;
            uint32_t totalWrites = 0;
            uint32_t totalBytesWritten = 0;
            uint32_t partialWrites = 0;
            uint32_t timeouts = 0;
            uint32_t minWriteUs = 0xFFFFFFFF;
            uint32_t maxWriteUs = 0;
            uint64_t sumWriteUs = 0;

            uint32_t startMs = millis();

            while (framesRemaining > 0)
            {
                size_t currentBatch = std::min((size_t)BLOCK_SAMPLES, (size_t)framesRemaining);
                for (size_t i = 0; i < currentBatch; ++i)
                {
                    int16_t sample = (int16_t)(sin(phase) * AMPLITUDE);
                    stereoBlock[i * 2]     = sample; // Left Channel
                    stereoBlock[i * 2 + 1] = sample; // Right Channel
                    phase += phaseIncrement;
                    if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
                }

                size_t bytesToWrite = currentBatch * 2 * sizeof(int16_t);
                size_t bytesWritten = 0;

                if (m_i2sMutex && xSemaphoreTake(m_i2sMutex, pdMS_TO_TICKS(150)) == pdTRUE)
                {
                    uint32_t t0 = micros();
                    esp_err_t err = i2s_write(AUDIO_I2S_PORT, stereoBlock, bytesToWrite, &bytesWritten, pdMS_TO_TICKS(200));
                    uint32_t dt = micros() - t0;

                    totalWrites++;
                    totalBytesWritten += bytesWritten;
                    sumWriteUs += dt;
                    if (dt < minWriteUs) minWriteUs = dt;
                    if (dt > maxWriteUs) maxWriteUs = dt;

                    if (err != ESP_OK) timeouts++;
                    else if (bytesWritten != bytesToWrite) partialWrites++;

                    xSemaphoreGive(m_i2sMutex);
                }

                framesRemaining -= currentBatch;
            }

            uint32_t elapsedMs = millis() - startMs;
            uint32_t avgWriteUs = totalWrites > 0 ? (uint32_t)(sumWriteUs / totalWrites) : 0;

            Serial.println("\n[SYNTHETIC TEST RESULTS]");
            Serial.printf("Elapsed Playback Time : %u ms (Expected: %u ms)\n", elapsedMs, DURATION_SEC * 1000);
            Serial.printf("Total Frames Produced : %u\n", totalFrames);
            Serial.printf("Total Bytes Requested : %u\n", totalStereoBytes);
            Serial.printf("Total Bytes Written   : %u\n", totalBytesWritten);
            Serial.printf("Total I2S Writes      : %u\n", totalWrites);
            Serial.printf("Partial Writes        : %u\n", partialWrites);
            Serial.printf("Timeouts              : %u\n", timeouts);
            Serial.printf("Average Write Duration: %u us\n", avgWriteUs);
            Serial.printf("Min Write Duration    : %u us\n", minWriteUs == 0xFFFFFFFF ? 0 : minWriteUs);
            Serial.printf("Max Write Duration    : %u us\n", maxWriteUs);
        };

        // Run 44.1 kHz Test first
        runSineTest(44100, "TEST 1: 44.1 kHz Sine Wave");

        vTaskDelay(pdMS_TO_TICKS(500));

        // Run 16 kHz Test as control
        runSineTest(16000, "TEST 2: 16.0 kHz Control Sine Wave");

        // Restore baseline 16kHz configuration
        if (m_i2sMutex && xSemaphoreTake(m_i2sMutex, pdMS_TO_TICKS(500)) == pdTRUE)
        {
            i2s_stop(AUDIO_I2S_PORT);
            i2s_zero_dma_buffer(AUDIO_I2S_PORT);
            i2s_set_sample_rates(AUDIO_I2S_PORT, AUDIO_SAMPLE_RATE);
            gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_BCLK_PIN, GPIO_DRIVE_CAP_3);
            gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_LRC_PIN, GPIO_DRIVE_CAP_3);
            gpio_set_drive_capability((gpio_num_t)AUDIO_I2S_DOUT_PIN, GPIO_DRIVE_CAP_3);
            i2s_zero_dma_buffer(AUDIO_I2S_PORT);
            i2s_start(AUDIO_I2S_PORT);
            xSemaphoreGive(m_i2sMutex);
        }

        m_voiceStreamPlaying = false;
        m_isPlaying = false;
        Serial.println("\n============================================================");
        Serial.println(">>> FORENSIC TEST COMPLETED — NORMAL AUDIO RESTORED <<<");
        Serial.println("============================================================\n");
        return true;
    }

} // namespace VOXA
