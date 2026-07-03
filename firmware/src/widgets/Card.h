#pragma once

#include "../core/Widget.h"

namespace VOXA
{
    class Card : public Widget
    {
    public:
        Card(Rect bounds, SDL_Color fill, float radius = 16.0f);

        void setShadow(SDL_Color color, int layers);
        void setBorder(SDL_Color color);
        void render(Renderer& renderer) const override;

    private:
        SDL_Color m_fill;
        SDL_Color m_border { 0, 0, 0, 0 };
        SDL_Color m_shadow { 18, 18, 18, 16 };
        float m_radius;
        int m_shadowLayers { 5 };
    };
}
