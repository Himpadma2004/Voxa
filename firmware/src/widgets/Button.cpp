#include "Button.h"

#include <utility>

#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Renderer.h"

namespace VOXA
{
    Button::Button(Rect bounds, std::string label, bool active)
        : Widget(bounds)
        , m_label(std::move(label))
        , m_active(active)
    {
    }

    void Button::render(Renderer& renderer) const
    {
        const SDL_Color background = m_active ? Colors::Primary : SDL_Color { 245, 240, 236, 255 };
        const SDL_Color text = m_active ? Colors::White : Colors::TextSecondary;
        renderer.drawSoftShadow(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, m_bounds.h * 0.5f, 4, SDL_Color { 12, 12, 18, 10 });
        renderer.fillRoundedRect(m_bounds.x, m_bounds.y, m_bounds.w, m_bounds.h, m_bounds.h * 0.5f, background);
        renderer.drawTextCentered(m_label, m_bounds.x + m_bounds.w * 0.5f, m_bounds.y + m_bounds.h * 0.28f, text, static_cast<int>(m_bounds.h * 0.42f));
    }
}
