#pragma once

#include "CircleButton.h"

namespace VOXA
{
    class FloatingButton : public CircleButton
    {
    public:
        FloatingButton(float centerX, float centerY, float radius, Icon icon, SDL_Color fill, SDL_Color iconColor)
            : CircleButton(centerX, centerY, radius, icon, fill, iconColor)
        {
        }
    };
}
