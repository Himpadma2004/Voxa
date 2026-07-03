#include "StatusBar.h"

#include <utility>

#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Icons.h"
#include "../graphics/Renderer.h"

namespace VOXA
{
    StatusBar::StatusBar(std::string title, std::string batteryLabel)
        : m_title(std::move(title))
        , m_batteryLabel(std::move(batteryLabel))
    {
    }

    void StatusBar::render(Renderer& renderer) const
    {
        const float right = static_cast<float>(renderer.canvasWidth());
        renderer.drawText(m_title, 52.0f, 44.0f, Colors::TextPrimary, 18);
        drawIcon(renderer, Icon::Wifi, right - 170.0f, 42.0f, 18.0f, Colors::TextPrimary);
        renderer.drawText(m_batteryLabel, right - 138.0f, 44.0f, Colors::TextPrimary, 12);
        drawIcon(renderer, Icon::Battery, right - 96.0f, 39.0f, 22.0f, Colors::TextPrimary);
    }
}
