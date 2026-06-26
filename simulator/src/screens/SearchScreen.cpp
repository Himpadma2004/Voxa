#include "SearchScreen.h"

#include <array>

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/FloatingButton.h"
#include "../widgets/ListTile.h"
#include "../widgets/SearchBar.h"
#include "ScreenCommon.h"

namespace
{
    struct SearchEntry
    {
        VOXA::Icon icon;
        const char* title;
        const char* age;
        SDL_Color color;
    };
}

namespace VOXA
{
    ScreenId SearchScreen::id() const
    {
        return ScreenId::Search;
    }

    void SearchScreen::onEnter(Application&)
    {
        m_elapsed = 0.0f;
    }

    void SearchScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            return;
        }

        const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
        if (Rect { 44.0f, 34.0f, 56.0f, 56.0f }.contains(point.x, point.y))
        {
            app.navigateTo(ScreenId::Home);
            return;
        }

        if (Rect { 1410.0f, 744.0f, 110.0f, 110.0f }.contains(point.x, point.y))
        {
            app.navigateTo(ScreenId::Record);
        }
    }

    void SearchScreen::update(Application&, float deltaSeconds)
    {
        m_elapsed += deltaSeconds;
    }

    void SearchScreen::render(Application&, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Search", true, true, Icon::Filter);

        SearchBar searchBar(Rect { 86.0f, 128.0f, 820.0f, 74.0f }, "Search memories, notes, recordings, and ideas...");
        searchBar.render(renderer);
        renderer.drawText("Recent Searches", 88.0f, 236.0f, Colors::TextPrimary, 18);

        const std::array<SearchEntry, 4> entries { {
            { Icon::Mic, "YouTube Integration", "2 days ago", SDL_Color { 147, 108, 255, 255 } },
            { Icon::Lightbulb, "AI Future Notes", "3 days ago", SDL_Color { 255, 178, 40, 255 } },
            { Icon::Bell, "Project Ideas", "5 days ago", SDL_Color { 255, 170, 40, 255 } },
            { Icon::Calendar, "Study Plan", "1 week ago", SDL_Color { 176, 84, 255, 255 } },
        } };

        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            const float slide = std::max(0.0f, 54.0f - m_elapsed * (72.0f + static_cast<float>(i) * 6.0f));
            ListTile tile(Rect { 86.0f, 278.0f + i * 122.0f + slide, 930.0f, 96.0f }, entries[i].icon, entries[i].title, entries[i].age, entries[i].color, SDL_Color { 0, 0, 0, 0 }, true);
            tile.render(renderer);
        }

        Card spotlight(Rect { 1070.0f, 128.0f, 424.0f, 570.0f }, SDL_Color { 255, 255, 255, 214 }, 32.0f);
        spotlight.setShadow(SDL_Color { 18, 18, 20, 12 }, 8);
        spotlight.render(renderer);
        renderer.drawText("Focus", 1116.0f, 178.0f, Colors::TextPrimary, 28);
        renderer.drawText("Memories are organized like a premium canvas.", 1116.0f, 228.0f, Colors::TextSecondary, 18);
        renderer.drawText("Tap the mic to open voice search instantly.", 1116.0f, 266.0f, Colors::TextSecondary, 18);
        renderer.drawGlowCircle(1282.0f, 470.0f, 112.0f, SDL_Color { 124, 92, 255, 22 }, 10);
        renderer.fillCircleGradient(1282.0f, 470.0f, 88.0f, SDL_Color { 244, 238, 255, 255 }, SDL_Color { 162, 123, 250, 255 });
        drawIcon(renderer, Icon::Search, 1238.0f, 426.0f, 88.0f, Colors::White);

        FloatingButton button(1460.0f, 798.0f, 48.0f, Icon::Mic, Colors::Primary, Colors::White);
        button.render(renderer);
    }
}
