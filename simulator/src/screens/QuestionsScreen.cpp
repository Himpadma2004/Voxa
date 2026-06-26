#include "QuestionsScreen.h"

#include <array>

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/ListTile.h"
#include "ScreenCommon.h"

namespace VOXA
{
    ScreenId QuestionsScreen::id() const
    {
        return ScreenId::Questions;
    }

    void QuestionsScreen::handleEvent(Application& app, const SDL_Event& event)
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

    void QuestionsScreen::update(Application&, float)
    {
    }

    void QuestionsScreen::render(Application&, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Questions", true, true, Icon::Plus);

        const std::array<std::pair<const char*, const char*>, 4> items { {
            { "What is ChromaDB?", "May 28" },
            { "Explain LLMs simply", "May 27" },
            { "AI future predictions", "May 26" },
            { "How does memory work?", "May 25" },
        } };

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            ListTile tile(Rect { 14.0f, 48.0f + i * 42.0f, 192.0f, 34.0f }, Icon::Question, items[i].first, items[i].second, SDL_Color { 126, 104, 180, 255 }, SDL_Color { 150, 126, 194, 255 }, true);
            tile.render(renderer);
        }

        ScreenCommon::renderPageDots(renderer, 0);
    }
}
