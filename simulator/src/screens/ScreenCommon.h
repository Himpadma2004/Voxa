#pragma once

#include <string>

#include "../graphics/Icons.h"

namespace VOXA
{
    class Renderer;

    namespace ScreenCommon
    {
        void renderSurface(Renderer& renderer);
        void renderPageDots(Renderer& renderer, int activeIndex, int count = 3);
        void renderCircularButton(Renderer& renderer, float centerX, float centerY, Icon icon, SDL_Color fill, SDL_Color iconColor);
        void renderHeader(Renderer& renderer, const std::string& title, bool showBack, bool showRightAction, Icon rightIcon);
    }
}
