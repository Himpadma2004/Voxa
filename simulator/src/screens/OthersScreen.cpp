#include "OthersScreen.h"

#include <array>
#include <tuple>

#include "../core/Application.h"
#include "../graphics/Renderer.h"
#include "../widgets/ListTile.h"
#include "ScreenCommon.h"

namespace VOXA
{
    ScreenId OthersScreen::id() const
    {
        return ScreenId::Others;
    }

    void OthersScreen::handleEvent(Application& app, const SDL_Event& event)
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

    void OthersScreen::update(Application&, float)
    {
    }

    void OthersScreen::render(Application&, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Others", true, true, Icon::Plus);

        const std::array<std::tuple<Icon, const char*, const char*, SDL_Color>, 4> items { {
            { Icon::Note, "Meeting Notes", "May 28", SDL_Color { 222, 92, 255, 255 } },
            { Icon::Chat, "Random Thoughts", "May 27", SDL_Color { 56, 168, 255, 255 } },
            { Icon::Mic, "Voice Note - Idea", "May 26", SDL_Color { 68, 192, 122, 255 } },
            { Icon::Note, "Daily Log", "May 25", SDL_Color { 62, 152, 255, 255 } },
        } };

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            ListTile tile(Rect { 14.0f, 48.0f + i * 42.0f, 192.0f, 34.0f },
                          std::get<0>(items[i]), std::get<1>(items[i]), std::get<2>(items[i]), std::get<3>(items[i]),
                          SDL_Color { 184, 160, 210, 255 }, false);
            tile.render(renderer);
        }

        ScreenCommon::renderPageDots(renderer, 0);
    }
}
