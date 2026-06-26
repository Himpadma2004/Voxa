#include "AudioEngine.h"

#include <cmath>
#include <vector>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
}

namespace VOXA
{
    AudioEngine::AudioEngine() = default;

    AudioEngine::~AudioEngine()
    {
        shutdown();
    }

    bool AudioEngine::initialize()
    {
        m_spec.format = SDL_AUDIO_F32;
        m_spec.channels = 1;
        m_spec.freq = 48000;

        m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &m_spec, nullptr, nullptr);
        if (m_stream == nullptr)
        {
            return false;
        }

        return SDL_ResumeAudioStreamDevice(m_stream);
    }

    void AudioEngine::shutdown()
    {
        if (m_stream != nullptr)
        {
            SDL_DestroyAudioStream(m_stream);
            m_stream = nullptr;
        }
    }

    void AudioEngine::playBoot()
    {
        playToneSequence(320.0f, 1.45f, 0.22f, 4, true);
    }

    void AudioEngine::playClick()
    {
        playToneSequence(620.0f, 0.08f, 0.12f, 2, false);
    }

    void AudioEngine::playSoftConfirm()
    {
        playToneSequence(520.0f, 0.18f, 0.11f, 3, true);
    }

    void AudioEngine::playToneSequence(float baseFrequency, float durationSeconds, float volume, int harmonics, bool rising)
    {
        if (m_stream == nullptr)
        {
            return;
        }

        const int sampleCount = static_cast<int>(durationSeconds * static_cast<float>(m_spec.freq));
        std::vector<float> samples(sampleCount, 0.0f);

        for (int i = 0; i < sampleCount; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(m_spec.freq);
            const float progress = static_cast<float>(i) / static_cast<float>(sampleCount);
            const float envelope = std::sin(progress * kPi);
            const float pitch = rising ? (baseFrequency + progress * 120.0f) : (baseFrequency - progress * 90.0f);

            float sample = 0.0f;
            for (int h = 1; h <= harmonics; ++h)
            {
                sample += std::sin(2.0f * kPi * pitch * static_cast<float>(h) * t) / static_cast<float>(h);
            }

            const float shimmer = std::sin(2.0f * kPi * 7.0f * t) * 0.08f;
            samples[i] = (sample / static_cast<float>(harmonics) + shimmer) * volume * envelope;
        }

        SDL_PutAudioStreamData(m_stream, samples.data(), static_cast<int>(samples.size() * sizeof(float)));
    }
}
