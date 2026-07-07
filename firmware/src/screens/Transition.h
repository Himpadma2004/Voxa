#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "../screens/ScreenCommon.h"

namespace VOXA
{
    /// Type of screen transition animation.
    enum class TransitionType
    {
        None,
        SlideUp,     ///< New screen slides in from bottom (navigate forward/into)
        SlideDown,   ///< New screen slides in from top (navigate backward/back)
        SlideLeft,   ///< New screen slides in from right (next sibling)
        SlideRight,  ///< New screen slides in from left (prev sibling / back)
        FadeScale,   ///< Cross-fade using scanline interlacing
    };

    extern ScreenId g_lastScreenId;

    /// Determines which transition to use between two screens.
    TransitionType getTransitionType(ScreenId from, ScreenId to);

    /// Handles drawing a single frame of the slide-in transition.
    /// This requires zero extra sprite allocations, preventing heap OOM crashes on ESP32.
    void playSlideInFrame(LGFX_Sprite& canvas, TransitionType type, int frame, int maxFrames);

} // namespace VOXA
