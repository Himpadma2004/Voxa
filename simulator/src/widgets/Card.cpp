#include "Card.h"

#include "../graphics/Renderer.h"

namespace VOXA
{
    Card::Card(Rect bounds, SDL_Color fill, float radius)
        : Widget(bounds)
        , m_fill(fill)
        , m_radius(radius)
    {
    }

    void Card::setShadow(SDL_Color color, int layers)
    {
        m_shadow = color;
        m_shadowLayers = layers;
    }

    void Card::setBorder(SDL_Color color)
    {
        m_border = color;
    }

    void Card::render(Renderer& renderer) const
    {
        renderer.drawSoftShadow(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, m_radius, m_shadowLayers, m_shadow);
        renderer.fillRoundedRect(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, m_radius, m_fill);
        if (m_border.a > 0)
        {
            renderer.drawRoundedRect(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, m_radius, m_border);
        }
    }
}
