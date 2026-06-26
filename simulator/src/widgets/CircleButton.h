#pragma once

#include "../core/Widget.h"
#include "../graphics/Icons.h"

namespace VOXA
{
    class CircleButton : public Widget
    {
    public:
        CircleButton(float centerX, float centerY, float radius, Icon icon, SDL_Color fill, SDL_Color iconColor);

        [[nodiscard]] bool hitTestCircle(float px, float py) const;
        void render(Renderer& renderer) const override;

    private:
        float m_centerX;
        float m_centerY;
        float m_radius;
        Icon m_icon;
        SDL_Color m_fill;
        SDL_Color m_iconColor;
    };
}
