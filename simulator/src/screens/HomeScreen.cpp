#include "HomeScreen.h"

#include <array>
#include <cmath>
#include <chrono>
#include <ctime>
#include <string>

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Icons.h"
#include "../graphics/Renderer.h"
#include "../widgets/Card.h"
#include "ScreenCommon.h"

namespace
{
    std::string getCurrentDateText()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm;
#if defined(_MSC_VER)
        localtime_s(&local_tm, &now_time);
#else
        localtime_r(&now_time, &local_tm);
#endif
        char buffer[128];
        std::strftime(buffer, sizeof(buffer), "%A, %b %d", &local_tm);
        return std::string(buffer);
    }

    std::string getCurrentTimeText()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm;
#if defined(_MSC_VER)
        localtime_s(&local_tm, &now_time);
#else
        localtime_r(&now_time, &local_tm);
#endif
        char buffer[128];
        std::strftime(buffer, sizeof(buffer), "%I:%M", &local_tm);
        std::string s(buffer);
        if (!s.empty() && s[0] == '0') s = s.substr(1);
        return s;
    }

    struct HomeAppCell
    {
        VOXA::Icon icon;
        const char* title;
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
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
            m_isDragging = true;
            m_dragStartX = point.x;
            m_dragStartY = point.y;
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            if (m_isDragging)
            {
                const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
                float dx = point.x - m_dragStartX;
                float dy = point.y - m_dragStartY;
                m_isDragging = false;

                // Watch-like swipe detection
                if (std::abs(dx) > 30.0f && std::abs(dy) < 50.0f)
                {
                    if (dx < 0.0f && m_page == 0) // swiped left -> next page
                    {
                        m_page = 1;
                        app.audio().playSoftConfirm();
                        return;
                    }
                    else if (dx > 0.0f && m_page == 1) // swiped right -> prev page
                    {
                        m_page = 0;
                        app.audio().playSoftConfirm();
                        return;
                    }
                }

                // Standard touch click
                if (std::abs(dx) < 6.0f && std::abs(dy) < 6.0f)
                {
                    // Bottom page dots trigger clicks to ease testing
                    if (point.y >= 215.0f && point.y <= 235.0f)
                    {
                        if (point.x >= 140.0f && point.x <= 160.0f)
                        {
                            m_page = 0;
                            app.audio().playClick();
                            return;
                        }
                        else if (point.x >= 160.0f && point.x <= 180.0f)
                        {
                            m_page = 1;
                            app.audio().playClick();
                            return;
                        }
                    }

                    if (m_page == 0)
                    {
                        // Mic button: centered at (160, 168), radius 28
                        float dist = std::hypot(point.x - 160.0f, point.y - 168.0f);
                        if (dist <= 28.0f)
                        {
                            app.navigateTo(ScreenId::Record);
                            return;
                        }
                    }
                    else if (m_page == 1)
                    {
                        const std::array<HomeAppCell, 6> cells { {
                            { Icon::Mic, "Capture", ScreenId::Record },
                            { Icon::Search, "Search", ScreenId::Search },
                            { Icon::Bell, "Reminders", ScreenId::Reminders },
                            { Icon::Lightbulb, "Ideas", ScreenId::Ideas },
                            { Icon::Question, "Questions", ScreenId::Questions },
                            { Icon::Settings, "Settings", ScreenId::Settings }
                        } };

                        const float cellW = 80.0f;
                        const float cellH = 60.0f;

                        for (std::size_t i = 0; i < cells.size(); ++i)
                        {
                            int row = static_cast<int>(i) / 3;
                            int col = static_cast<int>(i) % 3;
                            float cx = 54.0f + col * 106.0f;
                            float cy = 85.0f + row * 85.0f;
                            Rect cellRect { cx - cellW * 0.5f, cy - cellH * 0.5f, cellW, cellH };

                            if (cellRect.contains(point.x, point.y))
                            {
                                app.navigateTo(cells[i].target);
                                return;
                            }
                        }
                    }
                }
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

        const float width = static_cast<float>(renderer.canvasWidth());
        
        float mx = 0.0f, my = 0.0f;
        SDL_GetMouseState(&mx, &my);
        const SDL_FPoint mPt = app.windowToCanvas(mx, my);

        if (m_page == 0)
        {
            // PAGE 0: Watch Face & Main Capture
            // Large digital watch clock
            renderer.drawTextCentered(getCurrentDateText(), width * 0.5f, 48.0f, Colors::TextSecondary, 12);
            renderer.drawTextCentered(getCurrentTimeText(), width * 0.5f, 62.0f, Colors::TextPrimary, 52);

            // Large watch microphone capture button
            const float micX = 160.0f;
            const float micY = 168.0f;
            const float micR = 28.0f;
            const float dist = std::hypot(mPt.x - micX, mPt.y - micY);
            const bool hovered = dist <= micR;

            renderer.drawGlowCircle(micX, micY, micR + 4.0f, SDL_Color { 124, 92, 255, 20 }, 8);
            renderer.fillCircle(micX, micY, hovered ? micR + 2.0f : micR, Colors::Primary);
            renderer.drawCircle(micX, micY, hovered ? micR + 2.0f : micR, SDL_Color { 255, 255, 255, 90 });
            drawIcon(renderer, Icon::Mic, micX - 14.0f, micY - 14.0f, 28.0f, Colors::White);

            ScreenCommon::renderPageDots(renderer, 0, 2);
        }
        else
        {
            // PAGE 1: Option Grid (Icon First Watch Apps)
            const std::array<HomeAppCell, 6> cells { {
                { Icon::Mic, "Capture", ScreenId::Record },
                { Icon::Search, "Search", ScreenId::Search },
                { Icon::Bell, "Reminders", ScreenId::Reminders },
                { Icon::Lightbulb, "Ideas", ScreenId::Ideas },
                { Icon::Question, "Questions", ScreenId::Questions },
                { Icon::Settings, "Settings", ScreenId::Settings }
            } };

            const float cellW = 80.0f;
            const float cellH = 60.0f;

            for (std::size_t i = 0; i < cells.size(); ++i)
            {
                int row = static_cast<int>(i) / 3;
                int col = static_cast<int>(i) % 3;
                float cx = 54.0f + col * 106.0f;
                float cy = 85.0f + row * 85.0f;
                Rect cellRect { cx - cellW * 0.5f, cy - cellH * 0.5f, cellW, cellH };

                const bool hovered = cellRect.contains(mPt.x, mPt.y);
                Card card(cellRect, hovered ? Colors::CardHover : Colors::Card, 12.0f);
                card.setBorder(hovered ? Colors::PrimaryLight : Colors::GlassBorder);
                card.render(renderer);

                drawIcon(renderer, cells[i].icon, cx - 11.0f, cy - 20.0f, 22.0f, Colors::Primary);
                renderer.drawTextCentered(cells[i].title, cx, cy + 12.0f, Colors::TextPrimary, 10);
            }

            ScreenCommon::renderPageDots(renderer, 1, 2);
        }
    }
}
