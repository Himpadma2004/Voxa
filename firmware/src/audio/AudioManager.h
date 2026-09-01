#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <vector>

// I2S Pin definitions for MAX98357A Mono Amplifier
#ifndef AUDIO_I2S_BCLK_PIN
#define AUDIO_I2S_BCLK_PIN 15 // Bit Clock (BCLK) -> GPIO 15
#endif

#ifndef AUDIO_I2S_LRC_PIN
#define AUDIO_I2S_LRC_PIN 16 // Word Select / Left-Right Clock (LRC/WS) -> GPIO 16
#endif

#ifndef AUDIO_I2S_DOUT_PIN
#define AUDIO_I2S_DOUT_PIN 7 // Serial Data (DIN) -> GPIO 7
#endif

#ifndef AUDIO_I2S_PORT
#define AUDIO_I2S_PORT I2S_NUM_1 // Dedicated I2S Controller 1
#endif

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 16000 // 16 kHz Smooth Sample Rate
#endif

namespace VOXA
{
    class AudioManager
    {
    public:
        static AudioManager &instance();

        // Core lifecycle
        bool begin();
        void stop();

        // Playback methods
        bool playTone(uint16_t frequency, uint16_t durationMs);
        bool playPCM(const int16_t *samples, size_t sampleCount);
        bool writePCMChunk(const int16_t *samples, size_t sampleCount);
        bool playUrl(const std::string &url);
        bool playUrlAsync(const std::string &url);
        bool playWavAsync(const std::string &path) { return playUrlAsync(path); }
        bool playTapSoundAsync() { return playTone(1800, 25); }
        bool playBootChimeAsync() { return playBootMelody(); }
        bool playBootMelody();
        bool playConnectedChime();

        // Background & Reminder audio triggers
        bool playBackgroundMusicLoopAsync();
        void stopBackgroundMusic();
        bool playReminderMusicAsync();
        void stopReminderMusic();
        // Forensic isolation test
        bool runI2SSyntheticDiagnosticTest();

        // Volume control (0 - 100)
        void setVolume(uint8_t volume, bool saveToNvs = false);
        uint8_t getVolume() const { return m_volume; }

        // State accessors
        bool isPlaying() const { return m_isPlaying || m_voiceStreamPlaying; }
        bool isVoiceStreamPlaying() const { return m_voiceStreamPlaying; }
        bool isReminderPlaying() const { return m_reminderPlaying; }
        bool isBackgroundPlaying() const { return m_backgroundPlaying; }

        void uninstallI2SDriver();

    private:
        AudioManager();
        ~AudioManager();
        AudioManager(const AudioManager &) = delete;
        AudioManager &operator=(const AudioManager &) = delete;

        bool initI2SDriver();

        bool m_initialized{false};
        volatile bool m_isPlaying{false};
        volatile bool m_voiceStreamPlaying{false};
        volatile bool m_reminderPlaying{false};
        volatile bool m_backgroundPlaying{false};
        uint8_t m_volume{100}; // Full volume

        SemaphoreHandle_t m_i2sMutex{nullptr};
        TaskHandle_t m_playbackTaskHandle{nullptr};
        TaskHandle_t m_reminderTaskHandle{nullptr};
        TaskHandle_t m_bgTaskHandle{nullptr};
    };
} // namespace VOXA