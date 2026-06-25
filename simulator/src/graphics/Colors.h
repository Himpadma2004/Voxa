#pragma once

#include <SDL3/SDL.h>

namespace VOXA
{
    namespace Colors
    {
        // ===========================
        // Backgrounds
        // ===========================

        extern const SDL_Color Background;
        extern const SDL_Color Surface;
        extern const SDL_Color Card;
        extern const SDL_Color CardHover;

        // ===========================
        // Primary Theme
        // ===========================

        extern const SDL_Color Primary;
        extern const SDL_Color PrimaryLight;
        extern const SDL_Color PrimaryDark;

        // ===========================
        // Text
        // ===========================

        extern const SDL_Color TextPrimary;
        extern const SDL_Color TextSecondary;
        extern const SDL_Color TextDisabled;

        // ===========================
        // Borders
        // ===========================

        extern const SDL_Color Border;
        extern const SDL_Color Divider;

        // ===========================
        // Status Colors
        // ===========================

        extern const SDL_Color Success;
        extern const SDL_Color Warning;
        extern const SDL_Color Error;

        // ===========================
        // Misc
        // ===========================

        extern const SDL_Color White;
        extern const SDL_Color Black;
        extern const SDL_Color Transparent;
    }
}