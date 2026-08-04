#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include <vector>
#include <string>

// ── Configurable GPIO Definitions for MAX98357A I2S Mono Amplifier ────────────
// Dedicated Speaker Wiring: BCLK -> GPIO40, LRC(WS) -> GPIO38, DIN -> GPIO39
#ifndef AUDIO_I2S_BCLK_PIN
#define AUDIO_I2S_BCLK_PIN 40  // Bit Clock (BCLK) -> GPIO40 (dedicated)
#endif

#ifndef AUDIO_I2S_LRC_PIN
#define AUDIO_I2S_LRC_PIN 41   // Left/Right Clock (LRC / WS) -> GPIO38 (dedicated)
#endif

#ifndef AUDIO_I2S_DOUT_PIN
#define AUDIO_I2S_DOUT_PIN 39  // Data In (DIN) -> GPIO39 (dedicated)
#endif

#ifndef AUDIO_I2S_PORT
#define AUDIO_I2S_PORT I2S_NUM_1 // Dedicated I2S Port 1 (Port 0 used for INMP441 Mic)
#endif

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 16000 // 16 kHz Mono Audio Standard
#endif

namespace VOXA
{
    /**
     * @brief Production Audio Subsystem for VOXA using MAX98357A I2S Mono Amplifier.
     *
     * Features:
     * - Dedicated I2S_NUM_1 TX pipeline with DMA (non-blocking, FreeRTOS background task)
     * - Tone generation & startup acoustic sequence
     * - WAV file playback (SPIFFS / SD Card)
     * - PCM buffer playback & real-time PCM chunk streaming (for TTS / LLM stream)
     * - Software volume scaling (0%, 25%, 50%, 75%, 100%)
     * - Automated diagnostic self-test
     */
    class AudioManager
    {
    public:
        AudioManager(const AudioManager &) = delete;
        AudioManager &operator=(const AudioManager &) = delete;

        /// Singleton instance accessor.
        static AudioManager &instance();

        /**
         * @brief Initializes the I2S_NUM_1 TX driver and DMA buffers.
         * Plays the startup sequence tone.
         * @return true if initialized successfully.
         */
        bool begin();

        /**
         * @brief Sets software volume level (0 to 100%).
         */
        void setVolume(uint8_t volume);

        /**
         * @brief Returns current software volume level (0 to 100%).
         */
        [[nodiscard]] uint8_t getVolume() const { return m_volume; }

        /**
         * @brief Plays a WAV audio file asynchronously in a FreeRTOS background task.
         * @param path Full file path
         * @return true if background playback task started.
         */
        bool playWavAsync(const std::string &path);
        bool playWavAsync(const String &path) { return playWavAsync(std::string(path.c_str())); }

        /**
         * @brief Plays a WAV audio file from SPIFFS or SD Card (synchronous/blocking).
         * @param path Full file path (e.g. "/spiffs/audio/startup.wav" or "/recordings/sample.wav")
         * @return true if playback started successfully.
         */
        bool playWav(const std::string &path);
        bool playWav(const String &path) { return playWav(std::string(path.c_str())); }

        /**
         * @brief Plays a buffer of 16-bit signed PCM mono audio samples.
         * @param samples Pointer to sample array
         * @param sampleCount Number of 16-bit samples
         * @return true if playback succeeded.
         */
        bool playPCM(const int16_t *samples, size_t sampleCount);

        /**
         * @brief Writes a stream chunk of 16-bit PCM samples to I2S DMA.
         * Designed for real-time streaming TTS (OpenAI, Piper, Coqui, etc.).
         * @param samples Pointer to PCM sample chunk
         * @param sampleCount Number of samples in chunk
         * @return true if chunk written successfully.
         */
        bool writePCMChunk(const int16_t *samples, size_t sampleCount);

        /**
         * @brief Plays a single-frequency sine wave tone.
         * @param frequency Frequency in Hz (e.g. 1000 Hz)
         * @param durationMs Duration in milliseconds (e.g. 100 ms)
         * @return true if tone played successfully.
         */
        bool playTone(uint16_t frequency, uint16_t durationMs);

        /**
         * @brief Stops all active audio playback immediately.
         */
        void stop();

        /**
         * @brief Returns true if audio is currently playing.
         */
        [[nodiscard]] bool isPlaying() const { return m_isPlaying; }

        /**
         * @brief Runs full automated audio diagnostic suite.
         * Plays test frequencies (500 Hz, 700 Hz, 900 Hz) and prints PASS/FAIL report.
         * @return true if all diagnostic checks pass.
         */
        bool runDiagnostics();

    private:
        AudioManager();
        ~AudioManager();

        bool initI2S();
        void applyVolumeScaling(int16_t *samples, size_t count);

        bool m_initialized{false};
        bool m_isPlaying{false};
        uint8_t m_volume{75}; // Default 75% volume

        TaskHandle_t m_audioTaskHandle{nullptr};
    };
} // namespace VOXA