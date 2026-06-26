#include "ReminderScreen.h"

#include <array>

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Button.h"
#include "../widgets/Card.h"
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
            if (Rect { 44.0f, 34.0f, 56.0f, 56.0f }.contains(point.x, point.y))
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

        // Center glass card container
        Card container(Rect { 300.0f, 140.0f, 1000.0f, 640.0f }, Colors::Card, 32.0f);
        container.setShadow(Colors::Shadow, 8);
        container.setBorder(Colors::GlassBorder);
        container.render(renderer);

        // Category selection buttons inside the card container
        Button(Rect { 340.0f, 170.0f, 120.0f, 40.0f }, "All", true).render(renderer);
        Button(Rect { 480.0f, 170.0f, 130.0f, 40.0f }, "Today", false).render(renderer);
        Button(Rect { 630.0f, 170.0f, 150.0f, 40.0f }, "Upcoming", false).render(renderer);

        const std::array<ReminderItem, 4> items { {
            { "Call Sofia", "Today, 8:00 PM", Icon::Bell, SDL_Color { 130, 92, 255, 255 }, SDL_Color { 130, 92, 255, 255 } },
            { "Emily Birthday", "Jul 1, 2025", Icon::Calendar, SDL_Color { 255, 92, 146, 255 }, SDL_Color { 255, 92, 146, 255 } },
            { "Project Meeting", "Tomorrow, 10:00 AM", Icon::Bell, SDL_Color { 255, 168, 32, 255 }, SDL_Color { 255, 168, 32, 255 } },
            { "Submit Report", "Jun 5, 2026", Icon::Calendar, SDL_Color { 200, 92, 255, 255 }, SDL_Color { 150, 104, 255, 255 } },
        } };

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            ListTile tile(Rect { 340.0f, 238.0f + i * 114.0f, 920.0f, 84.0f }, items[i].icon, items[i].title, items[i].date, items[i].iconColor, items[i].dotColor, false);
            tile.render(renderer);
        }
    }
}
