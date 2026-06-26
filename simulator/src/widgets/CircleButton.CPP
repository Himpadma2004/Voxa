#include "CircleButton.h"

#include <cmath>

#include "../graphics/Renderer.h"

namespace VOXA
{
    CircleButton::CircleButton(float centerX, float centerY, float radius, Icon icon, SDL_Color fill, SDL_Color iconColor)
        : Widget(Rect { centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f })
        , m_centerX(centerX)
        , m_centerY(centerY)
        , m_radius(radius)
        , m_icon(icon)
        , m_fill(fill)
        , m_iconColor(iconColor)
    {
    }

    bool CircleButton::hitTestCircle(float px, float py) const
    {
        const float dx = px - m_centerX;
        const float dy = py - m_centerY;
        return (dx * dx + dy * dy) <= (m_radius * m_radius);
    }

    void CircleButton::render(Renderer& renderer) const
    {
        renderer.drawGlowCircle(m_centerX, m_centerY, m_radius, SDL_Color { m_fill.r, m_fill.g, m_fill.b, 18 }, 6);
        renderer.drawSoftShadow(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, m_radius, 5, SDL_Color { 15, 15, 20, 18 });
        renderer.fillCircleGradient(m_centerX, m_centerY, m_radius, SDL_Color { 180, 150, 255, 255 }, m_fill);
        renderer.drawCircle(m_centerX, m_centerY, m_radius - 1.0f, SDL_Color { 255, 255, 255, 110 });
        drawIcon(renderer, m_icon, m_centerX - m_radius * 0.5f, m_centerY - m_radius * 0.5f, m_radius, m_iconColor);
    }
}
