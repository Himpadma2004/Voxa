#include "ReminderScreen.h"

#include <array>

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Button.h"
#include "../widgets/ListTile.h"
#include "ScreenCommon.h"

namespace
{
    struct ReminderItem
    {
        const char* title;
        const char* date;
        VOXA::Icon icon;
        SDL_Color iconColor;
        SDL_Color dotColor;
    };
}

namespace VOXA
{
    ScreenId ReminderScreen::id() const
    {
        return ScreenId::Reminders;
    }

    void ReminderScreen::handleEvent(Application& app, const SDL_Event& event)
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

    void ReminderScreen::update(Application&, float)
    {
    }

    void ReminderScreen::render(Application&, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Reminders", true, true, Icon::Plus);

        Button(Rect { 18.0f, 42.0f, 50.0f, 20.0f }, "All", true).render(renderer);
        Button(Rect { 75.0f, 42.0f, 52.0f, 20.0f }, "Today", false).render(renderer);
        Button(Rect { 133.0f, 42.0f, 64.0f, 20.0f }, "Upcoming", false).render(renderer);

        const std::array<ReminderItem, 4> items { {
            { "Call Sofia", "Today, 8:00 PM", Icon::Bell, SDL_Color { 130, 92, 255, 255 }, SDL_Color { 130, 92, 255, 255 } },
            { "Emily Birthday", "Jul 1, 2025", Icon::Calendar, SDL_Color { 255, 92, 146, 255 }, SDL_Color { 255, 92, 146, 255 } },
            { "Project Meeting", "Tomorrow, 10:00 AM", Icon::Bell, SDL_Color { 255, 168, 32, 255 }, SDL_Color { 255, 168, 32, 255 } },
            { "Submit Report", "Jun 5, 2026", Icon::Calendar, SDL_Color { 200, 92, 255, 255 }, SDL_Color { 150, 104, 255, 255 } },
        } };

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            ListTile tile(Rect { 14.0f, 78.0f + i * 48.0f, 212.0f, 40.0f }, items[i].icon, items[i].title, items[i].date, items[i].iconColor, items[i].dotColor, false);
            tile.render(renderer);
        }
    }
}
