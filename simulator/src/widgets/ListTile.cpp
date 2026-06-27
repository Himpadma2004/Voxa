#include "ListTile.h"

#include <utility>
#include <algorithm>

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
        // 1. Draw tile card background: clean white, rounded
        const float radius = 12.0f;
        
        // Fill white card background
        renderer.fillRoundedRect(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, radius, SDL_Color { 255, 255, 255, 255 });
        
        // Subtle grey border
        renderer.drawRoundedRect(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, radius, SDL_Color { 235, 235, 240, 255 });

        // 2. Draw colored circle background for icon
        const float cy = m_bounds.y + m_bounds.h * 0.5f;
        const float circleCx = m_bounds.x + 24.0f;
        const float circleR = 14.0f; // diameter 28
        
        renderer.fillCircle(circleCx, cy, circleR, m_iconColor);

        // Draw icon inside circle in white
        const float iconSize = 14.0f;
        drawIcon(renderer, m_icon, circleCx - iconSize * 0.5f, cy - iconSize * 0.5f, iconSize, Colors::White);

        // 3. Draw text (Title and Subtitle/Timestamp)
        const float textX = m_bounds.x + 48.0f;
        
        if (m_subtitle.empty())
        {
            // Center title vertically if no subtitle exists
            renderer.drawText(m_title, textX, m_bounds.y + m_bounds.h * 0.32f, Colors::TextPrimary, 13);
        }
        else
        {
            renderer.drawText(m_title, textX, m_bounds.y + 8.0f, Colors::TextPrimary, 12);
            renderer.drawText(m_subtitle, textX, m_bounds.y + 26.0f, Colors::TextSecondary, 10);
        }

        // 4. Draw right accessory (chevron or circular checkbox)
        const float accessoryCx = m_bounds.x + m_bounds.w - 20.0f;
        
        if (m_showChevron)
        {
            const float chevronSize = 12.0f;
            drawIcon(renderer, Icon::ChevronRight, accessoryCx - chevronSize * 0.5f, cy - chevronSize * 0.5f, chevronSize, SDL_Color { 180, 180, 185, 255 });
        }
        else
        {
            // Draw iOS/watchOS-style checkbox circle outline on the right
            renderer.drawCircle(accessoryCx, cy, 9.0f, SDL_Color { 200, 200, 205, 255 });
        }
    }
}
