#include "IdeasScreen.h"

#include <array>

#include "../core/Application.h"
#include "../graphics/Renderer.h"
#include "../widgets/ListTile.h"
#include "ScreenCommon.h"

namespace VOXA
{
    ScreenId IdeasScreen::id() const
    {
        return ScreenId::Ideas;
    }

    void IdeasScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
            if (Rect { 12.0f, 10.0f, 20.0f, 20.0f }.contains(point.x, point.y))
            {
                app.navigateTo(ScreenId::Home);
            }
        }
    }

    void IdeasScreen::update(Application&, float)
    {
    }

    void IdeasScreen::render(Application&, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Ideas", true, true, Icon::Plus);

        const std::array<std::pair<const char*, const char*>, 4> items { {
            { "YouTube Integration", "May 28" },
            { "Offline AI Mode", "May 27" },
            { "Smart Dashboard", "May 26" },
            { "Auto Summary Feature", "May 25" },
        } };

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            ListTile tile(Rect { 14.0f, 48.0f + i * 42.0f, 192.0f, 34.0f }, Icon::Lightbulb, items[i].first, items[i].second, SDL_Color { 255, 178, 40, 255 }, SDL_Color { 255, 178, 40, 255 }, false);
            tile.render(renderer);
        }

        ScreenCommon::renderPageDots(renderer, 0);
    }
}
