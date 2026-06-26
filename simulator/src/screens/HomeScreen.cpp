#include "HomeScreen.h"

#include <array>
#include <cmath>

#include "../core/Application.h"
#include "../core/services/ReminderService.h"
#include "../core/services/IdeaService.h"
#include "../core/services/QuestionService.h"
#include "../core/services/StorageService.h"
#include "../core/services/MemoryService.h"
#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Icons.h"
#include "../graphics/Renderer.h"
#include "../widgets/Card.h"
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

        const Rect recordCard { 366.0f, 218.0f, 784.0f, 180.0f };
        if (recordCard.contains(point.x, point.y))
        {
            app.navigateTo(ScreenId::Record);
            return;
        }

        // Quick nav sidebar items
        const std::array<std::pair<const char*, ScreenId>, 4> quickNav { {
            { "Search Memories", ScreenId::Search },
            { "Voice Capture", ScreenId::Record },
            { "Reminder Stack", ScreenId::Reminders },
            { "System Settings", ScreenId::Settings },
        } };
        for (std::size_t i = 0; i < quickNav.size(); ++i)
        {
            const float y = 390.0f + static_cast<float>(i) * 76.0f;
            if (Rect { 74.0f, y, 218.0f, 54.0f }.contains(point.x, point.y))
            {
                app.navigateTo(quickNav[i].second);
                return;
            }
        }

        // Sidebar mic button
        const float micDist = std::hypot(point.x - 183.0f, point.y - 744.0f);
        if (micDist <= 40.0f)
        {
            app.navigateTo(ScreenId::Record);
            return;
        }

        const std::array<HomeTile, 6> tiles { {
            { { 366.0f, 438.0f, 380.0f, 170.0f }, Icon::Bell, "Reminders", "3", ScreenId::Reminders },
            { { 770.0f, 438.0f, 380.0f, 170.0f }, Icon::Lightbulb, "Ideas", "7", ScreenId::Ideas },
            { { 1174.0f, 438.0f, 380.0f, 170.0f }, Icon::Question, "Questions", "4", ScreenId::Questions },
            { { 366.0f, 634.0f, 380.0f, 170.0f }, Icon::Search, "Search", "", ScreenId::Search },
            { { 770.0f, 634.0f, 380.0f, 170.0f }, Icon::Folder, "Others", "12", ScreenId::Others },
            { { 1174.0f, 634.0f, 380.0f, 170.0f }, Icon::Settings, "Settings", "", ScreenId::Settings },
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

    void HomeScreen::render(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);

        // Get mouse coordinates for hover state checks
        float mx = 0.0f, my = 0.0f;
        SDL_GetMouseState(&mx, &my);
        const SDL_FPoint mPt = app.windowToCanvas(mx, my);

        Card sidebar(Rect { 48.0f, 118.0f, 270.0f, 686.0f }, SDL_Color { 255, 255, 255, 120 }, 24.0f);
        sidebar.setShadow(Colors::Shadow, 8);
        sidebar.setBorder(Colors::GlassBorder);
        sidebar.render(renderer);

        renderer.drawText("V O X A", 86.0f, 170.0f, Colors::Primary, 26);
        renderer.drawText("PERSONAL AI ASSISTANT", 86.0f, 214.0f, Colors::TextSecondary, 12);
        renderer.drawText("Embedded Voice Workspace", 86.0f, 278.0f, Colors::TextPrimary, 18);
        renderer.drawText("Landscape simulator preview", 86.0f, 310.0f, Colors::TextSecondary, 13);

        const std::array<std::pair<const char*, ScreenId>, 4> quickNav { {
            { "Search Memories", ScreenId::Search },
            { "Voice Capture", ScreenId::Record },
            { "Reminder Stack", ScreenId::Reminders },
            { "System Settings", ScreenId::Settings },
        } };

        for (std::size_t i = 0; i < quickNav.size(); ++i)
        {
            const float y = 390.0f + static_cast<float>(i) * 76.0f;
            const Rect navRect { 74.0f, y, 218.0f, 54.0f };
            const bool hovered = navRect.contains(mPt.x, mPt.y);

            renderer.fillRoundedRect(74.0f, y, 218.0f, 54.0f, 18.0f, 
                hovered ? SDL_Color { 255, 255, 255, 200 } : SDL_Color { 255, 255, 255, static_cast<Uint8>(i == 1 ? 160 : 70) });
            renderer.drawRoundedRect(74.0f, y, 218.0f, 54.0f, 18.0f, hovered ? Colors::PrimaryLight : Colors::GlassBorder);
            renderer.drawText(quickNav[i].first, 98.0f, y + 17.0f, hovered ? Colors::Primary : Colors::TextPrimary, 14);
        }

        const float micDist = std::hypot(mPt.x - 183.0f, mPt.y - 744.0f);
        const bool micHovered = micDist <= 54.0f;
        // Clean minimal mic button - flat filled circle, no gradient spheres
        renderer.fillCircle(183.0f, 744.0f, micHovered ? 34.0f : 30.0f, micHovered ? Colors::Primary : SDL_Color { 186, 160, 255, 255 });
        renderer.drawCircle(183.0f, 744.0f, micHovered ? 34.0f : 30.0f, SDL_Color { 255, 255, 255, 60 });
        drawIcon(renderer, Icon::Mic, 161.0f, 722.0f, 44.0f, Colors::White);

        const float greetOffset = std::max(0.0f, 14.0f - m_elapsed * 26.0f);
        renderer.drawText("Good Morning", 366.0f, 126.0f + greetOffset, Colors::TextPrimary, 30);
        renderer.drawText("*", 592.0f, 118.0f + greetOffset, SDL_Color { 255, 184, 54, 255 }, 22);
        renderer.drawText("Ready to capture your thoughts in a clean HD landscape workspace.", 368.0f, 172.0f + greetOffset, Colors::TextSecondary, 14);

        const Rect recordCardRect { 366.0f, 218.0f, 784.0f, 180.0f };
        const bool recordHovered = recordCardRect.contains(mPt.x, mPt.y);
        Card recordCard(recordCardRect, recordHovered ? Colors::CardHover : Colors::Card, 24.0f);
        recordCard.setShadow(Colors::Shadow, 6);
        recordCard.setBorder(recordHovered ? Colors::PrimaryLight : Colors::GlassBorder);
        recordCard.render(renderer);

        // Mic circle icon on the left of the record card
        const float micCX = 456.0f;
        const float micCY = 308.0f;
        const float micR  = 48.0f;
        renderer.fillCircle(micCX, micCY, micR, Colors::Primary);
        renderer.drawCircle(micCX, micCY, micR, SDL_Color { 255, 255, 255, 60 });
        drawIcon(renderer, Icon::Mic, micCX - micR * 0.5f, micCY - micR * 0.5f, micR, Colors::White);

        // Label and subtitle to the right
        renderer.drawText("Record", 546.0f, 264.0f, Colors::TextPrimary, 22);
        renderer.drawText("Tap to start a voice session with waveform monitoring", 546.0f, 312.0f, Colors::TextSecondary, 13);

        const Rect insightCardRect { 1174.0f, 218.0f, 380.0f, 180.0f };
        const bool insightHovered = insightCardRect.contains(mPt.x, mPt.y);
        Card insightCard(insightCardRect, insightHovered ? Colors::CardHover : Colors::Card, 24.0f);
        insightCard.setShadow(Colors::Shadow, 6);
        insightCard.setBorder(insightHovered ? Colors::PrimaryLight : Colors::GlassBorder);
        insightCard.render(renderer);
        renderer.drawText("Today's Sync", 1204.0f, 256.0f, Colors::TextPrimary, 22);
        renderer.drawText("3 files waiting to sync", 1204.0f, 300.0f, Colors::TextSecondary, 14);
        renderer.drawText("Tap Settings to continue", 1204.0f, 332.0f, Colors::Primary, 13);

        std::string remindersCount = "0";
        if (app.services().reminders)
        {
            remindersCount = std::to_string(app.services().reminders->getAll().size());
        }

        std::string ideasCount = "0";
        if (app.services().ideas)
        {
            ideasCount = std::to_string(app.services().ideas->getAll().size());
        }

        std::string questionsCount = "0";
        if (app.services().questions)
        {
            questionsCount = std::to_string(app.services().questions->getAll().size());
        }

        std::string memoriesCount = "0";
        if (app.services().memoryService)
        {
            memoriesCount = std::to_string(app.services().memoryService->getAll().size());
        }

        static std::string remStr;
        static std::string ideaStr;
        static std::string questStr;
        static std::string memStr;
        remStr = remindersCount;
        ideaStr = ideasCount;
        questStr = questionsCount;
        memStr = memoriesCount;

        const std::array<HomeTile, 6> tiles { {
            { { 366.0f, 438.0f, 380.0f, 170.0f }, Icon::Bell, "Reminders", remStr.c_str(), ScreenId::Reminders },
            { { 770.0f, 438.0f, 380.0f, 170.0f }, Icon::Lightbulb, "Ideas", ideaStr.c_str(), ScreenId::Ideas },
            { { 1174.0f, 438.0f, 380.0f, 170.0f }, Icon::Question, "Questions", questStr.c_str(), ScreenId::Questions },
            { { 366.0f, 634.0f, 380.0f, 170.0f }, Icon::Search, "Search", "", ScreenId::Search },
            { { 770.0f, 634.0f, 380.0f, 170.0f }, Icon::Folder, "Others", memStr.c_str(), ScreenId::Others },
            { { 1174.0f, 634.0f, 380.0f, 170.0f }, Icon::Settings, "Settings", "", ScreenId::Settings },
        } };

        for (std::size_t i = 0; i < tiles.size(); ++i)
        {
            const HomeTile& tile = tiles[i];
            const bool tileHovered = tile.bounds.contains(mPt.x, mPt.y);
            
            float rise = std::max(0.0f, 16.0f - m_elapsed * (20.0f + static_cast<float>(i) * 3.0f));
            if (tileHovered)
            {
                rise -= 5.0f; // lift on hover
            }

            Card card(Rect { tile.bounds.x, tile.bounds.y + rise, tile.bounds.w, tile.bounds.h }, 
                tileHovered ? Colors::CardHover : Colors::Card, 24.0f);
            card.setShadow(Colors::Shadow, tileHovered ? 8 : 5);
            card.setBorder(tileHovered ? Colors::PrimaryLight : Colors::GlassBorder);
            card.render(renderer);

            drawIcon(renderer, tile.icon, tile.bounds.x + 28.0f, tile.bounds.y + 28.0f + rise, 28.0f, Colors::Primary);
            renderer.drawText(tile.title, tile.bounds.x + 28.0f, tile.bounds.y + 76.0f + rise, Colors::TextPrimary, 16);
            if (tile.value[0] != '\0')
            {
                renderer.drawText(tile.value, tile.bounds.x + 28.0f, tile.bounds.y + 108.0f + rise, Colors::TextSecondary, 13);
            }
        }

        ScreenCommon::renderPageDots(renderer, 0);
    }
}
