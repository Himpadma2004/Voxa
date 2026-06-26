#pragma once

#include <SDL3/SDL.h>

namespace VOXA
{
    class AudioEngine
    {
    public:
        AudioEngine();
        ~AudioEngine();

        bool initialize();
        void shutdown();

        void playBoot();
        void playClick();
        void playSoftConfirm();

    private:
        void playToneSequence(float baseFrequency, float durationSeconds, float volume, int harmonics, bool rising);

        SDL_AudioStream* m_stream { nullptr };
        SDL_AudioSpec m_spec {};
    };
}
