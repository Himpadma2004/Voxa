#include "ListTile.h"

#include <utility>

#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Renderer.h"

namespace VOXA
{
    ListTile::ListTile(Rect bounds, Icon icon, std::string title, std::string subtitle, SDL_Color iconColor, SDL_Color trailingDot, bool showChevron)
        : Widget(bounds)
        , m_icon(icon)
        , m_title(std::move(title))
        , m_subtitle(std::move(subtitle))
        , m_iconColor(iconColor)
        , m_trailingDot(trailingDot)
        , m_showChevron(showChevron)
    {
    }

    void ListTile::render(Renderer& renderer) const
    {
        const float radius = std::max(12.0f, m_bounds.h * 0.18f);
        const float iconBoxSize = m_bounds.h * 0.44f;
        const float iconBoxX = m_bounds.x + m_bounds.h * 0.28f;
        const float iconBoxY = m_bounds.y + (m_bounds.h - iconBoxSize) * 0.5f;
        const float textX = m_bounds.x + m_bounds.h * 0.92f;
        const float titleSize = std::max(16.0f, m_bounds.h * 0.24f);
        const float subtitleSize = std::max(12.0f, m_bounds.h * 0.18f);

        renderer.fillRoundedRect(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, radius, Colors::Card);
        renderer.drawRoundedRect(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, radius, Colors::GlassBorder);

        drawIcon(renderer, m_icon, iconBoxX, iconBoxY, iconBoxSize, m_iconColor);

        renderer.drawText(m_title, textX, m_bounds.y + m_bounds.h * 0.24f, Colors::TextPrimary, static_cast<int>(titleSize));
        renderer.drawText(m_subtitle, textX, m_bounds.y + m_bounds.h * 0.56f, Colors::TextSecondary, static_cast<int>(subtitleSize));

        if (m_showChevron)
        {
            drawIcon(renderer, Icon::ChevronRight, m_bounds.x + m_bounds.w - m_bounds.h * 0.42f, m_bounds.y + m_bounds.h * 0.32f, m_bounds.h * 0.16f, Colors::TextSecondary);
        }
        else if (m_trailingDot.a > 0)
        {
            renderer.fillCircle(m_bounds.x + m_bounds.w - m_bounds.h * 0.38f, m_bounds.y + m_bounds.h * 0.5f, m_bounds.h * 0.06f, m_trailingDot);
        }
    }
}
