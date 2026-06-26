#include "HomeScreen.h"

#include <array>
#include <cmath>

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Icons.h"
#include "../graphics/Renderer.h"
#include "../widgets/Card.h"
#include "../widgets/CircleButton.h"
#include "../widgets/StatusBar.h"
#include "ScreenCommon.h"

namespace
{
    struct HomeTile
    {
        VOXA::Rect bounds;
        VOXA::Icon icon;
        const char* title;
        const char* value;
        VOXA::ScreenId target;
    };
}

namespace VOXA
{
    ScreenId HomeScreen::id() const
    {
        return ScreenId::Home;
    }

    void HomeScreen::onEnter(Application&)
    {
        m_elapsed = 0.0f;
    }

    void HomeScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            return;
        }

        const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);

        const Rect recordCard { 380.0f, 218.0f, 520.0f, 180.0f };
        if (recordCard.contains(point.x, point.y))
        {
            app.navigateTo(ScreenId::Record);
            return;
        }

        const std::array<HomeTile, 6> tiles { {
            { { 380.0f, 438.0f, 240.0f, 170.0f }, Icon::Bell, "Reminders", "3", ScreenId::Reminders },
            { { 642.0f, 438.0f, 240.0f, 170.0f }, Icon::Lightbulb, "Ideas", "7", ScreenId::Ideas },
            { { 904.0f, 438.0f, 240.0f, 170.0f }, Icon::Question, "Questions", "4", ScreenId::Questions },
            { { 380.0f, 634.0f, 240.0f, 170.0f }, Icon::Search, "Search", "", ScreenId::Search },
            { { 642.0f, 634.0f, 240.0f, 170.0f }, Icon::Folder, "Others", "12", ScreenId::Others },
            { { 904.0f, 634.0f, 240.0f, 170.0f }, Icon::Settings, "Settings", "", ScreenId::Settings },
        } };

        for (const HomeTile& tile : tiles)
        {
            if (tile.bounds.contains(point.x, point.y))
            {
                app.navigateTo(tile.target);
                return;
            }
        }
    }

    void HomeScreen::update(Application&, float deltaSeconds)
    {
        m_elapsed += deltaSeconds;
    }

    void HomeScreen::render(Application&, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        StatusBar statusBar("VOXA", "92%");
        statusBar.render(renderer);

        Card sidebar(Rect { 48.0f, 118.0f, 270.0f, 686.0f }, SDL_Color { 18, 20, 28, 228 }, 34.0f);
        sidebar.setShadow(SDL_Color { 4, 4, 8, 18 }, 8);
        sidebar.render(renderer);

        renderer.drawText("V O X A", 86.0f, 170.0f, SDL_Color { 242, 242, 248, 255 }, 36);
        renderer.drawText("PERSONAL AI ASSISTANT", 86.0f, 224.0f, SDL_Color { 176, 178, 192, 255 }, 14);
        renderer.drawText("Embedded voice workspace", 86.0f, 288.0f, SDL_Color { 208, 210, 224, 255 }, 20);
        renderer.drawText("Landscape simulator preview", 86.0f, 320.0f, SDL_Color { 138, 140, 156, 255 }, 14);

        const std::array<std::pair<const char*, ScreenId>, 4> quickNav { {
            { "Search Memories", ScreenId::Search },
            { "Voice Capture", ScreenId::Record },
            { "Reminder Stack", ScreenId::Reminders },
            { "System Settings", ScreenId::Settings },
        } };

        for (std::size_t i = 0; i < quickNav.size(); ++i)
        {
            const float y = 410.0f + static_cast<float>(i) * 82.0f;
            renderer.fillRoundedRect(78.0f, y, 210.0f, 60.0f, 22.0f, SDL_Color { 255, 255, 255, i == 1 ? 24 : 12 });
            renderer.drawText(quickNav[i].first, 102.0f, y + 20.0f, SDL_Color { 238, 239, 246, 255 }, 16);
        }

        renderer.drawGlowCircle(214.0f, 744.0f, 54.0f, SDL_Color { 124, 92, 255, 20 }, 8);
        renderer.fillCircleGradient(214.0f, 744.0f, 42.0f, SDL_Color { 186, 160, 255, 255 }, Colors::Primary);
        drawIcon(renderer, Icon::Mic, 192.0f, 722.0f, 44.0f, Colors::White);

        const float greetOffset = std::max(0.0f, 14.0f - m_elapsed * 26.0f);
        renderer.drawText("Good Morning", 380.0f, 126.0f + greetOffset, Colors::TextPrimary, 46);
        renderer.drawText("*", 696.0f, 118.0f + greetOffset, SDL_Color { 255, 184, 54, 255 }, 34);
        renderer.drawText("Ready to capture your thoughts in a clean HD landscape workspace.", 382.0f, 180.0f + greetOffset, Colors::TextSecondary, 18);

        Card recordCard(Rect { 380.0f, 218.0f, 520.0f, 180.0f }, Colors::Card, 28.0f);
        recordCard.setShadow(SDL_Color { 18, 18, 20, 10 }, 6);
        recordCard.render(renderer);

        CircleButton mic(476.0f, 308.0f, 56.0f, Icon::Mic, Colors::Primary, Colors::White);
        mic.render(renderer);
        renderer.drawText("Record", 580.0f, 272.0f, Colors::TextPrimary, 34);
        renderer.drawText("Tap to start an elegant voice session with waveform monitoring", 580.0f, 324.0f, Colors::TextSecondary, 18);

        Card insightCard(Rect { 930.0f, 218.0f, 360.0f, 180.0f }, SDL_Color { 248, 245, 252, 255 }, 28.0f);
        insightCard.setShadow(SDL_Color { 18, 18, 20, 10 }, 6);
        insightCard.render(renderer);
        renderer.drawText("Today's Sync", 970.0f, 258.0f, Colors::TextPrimary, 26);
        renderer.drawText("3 files waiting to sync", 970.0f, 306.0f, Colors::TextSecondary, 18);
        renderer.drawText("Tap Settings to continue", 970.0f, 336.0f, Colors::Primary, 16);

        const std::array<HomeTile, 6> tiles { {
            { { 380.0f, 438.0f, 240.0f, 170.0f }, Icon::Bell, "Reminders", "3", ScreenId::Reminders },
            { { 642.0f, 438.0f, 240.0f, 170.0f }, Icon::Lightbulb, "Ideas", "7", ScreenId::Ideas },
            { { 904.0f, 438.0f, 240.0f, 170.0f }, Icon::Question, "Questions", "4", ScreenId::Questions },
            { { 380.0f, 634.0f, 240.0f, 170.0f }, Icon::Search, "Search", "", ScreenId::Search },
            { { 642.0f, 634.0f, 240.0f, 170.0f }, Icon::Folder, "Others", "12", ScreenId::Others },
            { { 904.0f, 634.0f, 240.0f, 170.0f }, Icon::Settings, "Settings", "", ScreenId::Settings },
        } };

        for (std::size_t i = 0; i < tiles.size(); ++i)
        {
            const HomeTile& tile = tiles[i];
            const float rise = std::max(0.0f, 16.0f - m_elapsed * (20.0f + static_cast<float>(i) * 3.0f));
            Card card(Rect { tile.bounds.x, tile.bounds.y + rise, tile.bounds.w, tile.bounds.h }, Colors::Card, 26.0f);
            card.setShadow(SDL_Color { 18, 18, 20, 8 }, 4);
            card.render(renderer);

            drawIcon(renderer, tile.icon, tile.bounds.x + 32.0f, tile.bounds.y + 26.0f + rise, 36.0f, Colors::TextPrimary);
            renderer.drawText(tile.title, tile.bounds.x + 34.0f, tile.bounds.y + 92.0f + rise, Colors::TextPrimary, 24);
            if (tile.value[0] != '\0')
            {
                renderer.drawText(tile.value, tile.bounds.x + 34.0f, tile.bounds.y + 126.0f + rise, Colors::TextSecondary, 16);
            }
        }

        ScreenCommon::renderPageDots(renderer, 0);
    }
}
