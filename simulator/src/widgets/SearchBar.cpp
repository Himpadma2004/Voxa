#include "SearchBar.h"

#include <utility>

#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Icons.h"
#include "../graphics/Renderer.h"

namespace VOXA
{
    SearchBar::SearchBar(Rect bounds, std::string placeholder)
        : Widget(bounds)
        , m_placeholder(std::move(placeholder))
    {
    }

    void SearchBar::render(Renderer& renderer) const
    {
        renderer.fillRoundedRect(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, m_bounds.h * 0.5f, SDL_Color { 248, 244, 240, 255 });
        renderer.drawText(m_placeholder, m_bounds.x + m_bounds.h * 0.45f, m_bounds.y + m_bounds.h * 0.3f, SDL_Color { 168, 162, 157, 255 }, static_cast<int>(m_bounds.h * 0.34f));
        drawIcon(renderer, Icon::Search, m_bounds.x + m_bounds.w - m_bounds.h * 0.8f, m_bounds.y + m_bounds.h * 0.25f, m_bounds.h * 0.45f, Colors::TextSecondary);
    }
}
