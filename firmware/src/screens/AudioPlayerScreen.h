#pragma once

#include "ScreenCommon.h"
#include "../touch/Touch.h"

namespace VOXA
{
    class AudioPlayerScreen
    {
    public:
        static void setRecording(uint32_t id, ScreenId backRoute);
        
        ScreenId show(Touch& touch);

    private:
        static uint32_t s_recordingId;
        static ScreenId s_backRoute;
    };
}
