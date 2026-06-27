// HomeScreen.cpp — Smartwatch Home OS for Waveshare 2.8" 320x240 display
#include "HomeScreen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <ctime>

#include "../core/Application.h"
#include "../core/Widget.h"
#include "../core/services/ReminderService.h"
#include "../core/services/IdeaService.h"
#include "../core/services/QuestionService.h"
#include "../core/services/MemoryService.h"
#include "../graphics/Colors.h"
#include "../graphics/Icons.h"
#include "../graphics/Renderer.h"
#include "ScreenCommon.h"

namespace
{
    constexpr float CW = 320.0f;
    constexpr float CH = 240.0f;

    struct MenuEntry
    {
        VOXA::Icon icon;
        const char* label;
        SDL_Color color;
        VOXA::ScreenId target;
        int badgeCount;
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
        m_isDragging = false;
        m_isScrollDragging = false;
        m_swipeOffset = 0.0f;
    }

    void HomeScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        // Get number of items in services for live badges
        int remCount = app.services().reminders ? static_cast<int>(app.services().reminders->getAll().size()) : 3;
        int ideaCount = app.services().ideas ? static_cast<int>(app.services().ideas->getAll().size()) : 4;
        int qCount = app.services().questions ? static_cast<int>(app.services().questions->getAll().size()) : 4;
        int memCount = app.services().memoryService ? static_cast<int>(app.services().memoryService->getAll().size()) : 6;

        const std::array<MenuEntry, 7> menuItems { {
            { Icon::Bell, "Reminders", SDL_Color { 145, 105, 255, 255 }, ScreenId::Reminders, remCount },
            { Icon::Lightbulb, "Ideas", SDL_Color { 255, 180, 40, 255 }, ScreenId::Ideas, ideaCount },
            { Icon::Question, "Questions", SDL_Color { 80, 185, 255, 255 }, ScreenId::Questions, qCount },
            { Icon::Search, "Search", SDL_Color { 75, 210, 140, 255 }, ScreenId::Search, 0 },
            { Icon::Mic, "Recordings", SDL_Color { 255, 105, 150, 255 }, ScreenId::Others, memCount },
            { Icon::Folder, "Others", SDL_Color { 150, 150, 160, 255 }, ScreenId::Others, 0 },
            { Icon::Settings, "Settings", SDL_Color { 85, 95, 110, 255 }, ScreenId::Settings, 0 }
        } };

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            const SDL_FPoint pt = app.windowToCanvas(event.button.x, event.button.y);
            m_dragStartX = pt.x;
            m_dragStartY = pt.y;
            m_isDragging = false;
            m_isScrollDragging = false;
            m_swipeOffset = 0.0f;
        }
        else if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            if (event.motion.state & SDL_BUTTON_LMASK)
            {
                const SDL_FPoint pt = app.windowToCanvas(event.motion.x, event.motion.y);
                float dx = pt.x - m_dragStartX;
                float dy = pt.y - m_dragStartY;

                if (!m_isDragging && !m_isScrollDragging)
                {
                    // Detect drag direction: horizontal swipe vs vertical scroll
                    if (std::abs(dx) > std::abs(dy) && std::abs(dx) > 5.0f)
                    {
                        m_isDragging = true;
                    }
                    else if (m_page == 1 && std::abs(dy) > 5.0f)
                    {
                        m_isScrollDragging = true;
                    }
                }

                if (m_isDragging)
                {
                    m_swipeOffset = dx;
                }
                else if (m_isScrollDragging)
                {
                    m_menuTargetScrollY -= dy * 1.2f;
                    m_dragStartY = pt.y; // reset for next delta
                }
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            const SDL_FPoint pt = app.windowToCanvas(event.button.x, event.button.y);
            float dx = pt.x - m_dragStartX;
            float dy = pt.y - m_dragStartY;

            if (m_isDragging)
            {
                // Page change gesture
                if (dx < -60.0f && m_page == 0)
                {
                    m_page = 1;
                    app.audio().playSoftConfirm();
                }
                else if (dx > 60.0f && m_page == 1)
                {
                    m_page = 0;
                    app.audio().playSoftConfirm();
                }
                m_isDragging = false;
                m_swipeOffset = 0.0f;
            }
            else if (m_isScrollDragging)
            {
                m_isScrollDragging = false;
            }
            else
            {
                // Simple tap (no swipe or scroll)
                if (m_page == 0)
                {
                    // Click Large Centered Microphone Button -> Record
                    // Center is (160, 126) with radius 36, plus "Tap to Record" text
                    if (Rect { 110.0f, 80.0f, 100.0f, 110.0f }.contains(pt.x, pt.y))
                    {
                        app.navigateTo(ScreenId::Record);
                        return;
                    }

                    // Click Right Chevron Button -> Go to Menu list
                    if (Rect { 275.0f, 106.0f, 45.0f, 40.0f }.contains(pt.x, pt.y))
                    {
                        m_page = 1;
                        app.audio().playSoftConfirm();
                        return;
                    }
                }
                else if (m_page == 1)
                {
                    // Back button click in menu header
                    if (Rect { 5.0f, 15.0f, 26.0f, 26.0f }.contains(pt.x, pt.y))
                    {
                        m_page = 0;
                        app.audio().playSoftConfirm();
                        return;
                    }

                    // Click on Menu List Items
                    for (std::size_t i = 0; i < menuItems.size(); ++i)
                    {
                        Rect itemRect { 10.0f, 50.0f + i * 50.0f - m_menuScrollY, 300.0f, 44.0f };
                        if (itemRect.contains(pt.x, pt.y) && pt.y >= 48.0f && pt.y <= 220.0f)
                        {
                            app.navigateTo(menuItems[i].target);
                            return;
                        }
                    }
                }
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            if (m_page == 1)
            {
                m_menuTargetScrollY -= event.wheel.y * 20.0f;
            }
        }
    }

    void HomeScreen::update(Application& app, float deltaSeconds)
    {
        m_elapsed += deltaSeconds;

        if (m_page == 1)
        {
            // Calculate scroll limits
            float contentHeight = 7.0f * 50.0f + 10.0f;
            float visibleHeight = 172.0f;
            float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            m_menuTargetScrollY = std::clamp(m_menuTargetScrollY, 0.0f, maxScrollY);
            m_menuScrollY += (m_menuTargetScrollY - m_menuScrollY) * 12.0f * deltaSeconds;
            if (std::abs(m_menuTargetScrollY - m_menuScrollY) < 0.1f)
            {
                m_menuScrollY = m_menuTargetScrollY;
            }
        }
    }

    void HomeScreen::render(Application& app, Renderer& renderer)
    {
        const float offset = m_isDragging ? m_swipeOffset : 0.0f;

        // Render Page 0 & Page 1 with horizontal sliding transitions
        for (int p = 0; p < kNumPages; ++p)
        {
            float drawX = static_cast<float>(p - m_page) * CW + offset;
            if (drawX <= -CW || drawX >= CW) continue;

            renderer.setLogicalOffset(drawX, 0.0f);

            if (p == 0)
            {
                renderPage0(app, renderer);
            }
            else
            {
                renderPage1(app, renderer);
            }

            renderer.resetLogicalOffset();
        }

        // Draw page dots always at the bottom
        ScreenCommon::renderPageDots(renderer, m_page, kNumPages);
    }

    // Page 0 — Voice Assistant Home screen (mockup style)
    void HomeScreen::renderPage0(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);

        // Header Title (Centered)
        renderer.drawTextCentered("VOXA", 160.0f, 30.0f, SDL_Color { 155, 120, 255, 255 }, 11);
        
        // Dynamic Greeting based on current local hour (resolves box glyph issue by removing 👋)
        std::string greeting = "Good Morning";
        std::time_t tNow = std::time(nullptr);
        std::tm local_tm;
#if defined(_MSC_VER)
        localtime_s(&local_tm, &tNow);
#else
        localtime_r(&tNow, &local_tm);
#endif
        int hour = local_tm.tm_hour;
        if (hour >= 12 && hour < 17) greeting = "Good Afternoon";
        else if (hour >= 17 && hour < 21) greeting = "Good Evening";
        else if (hour >= 21 || hour < 5) greeting = "Good Night";

        renderer.drawTextCentered(greeting, 160.0f, 46.0f, Colors::TextPrimary, 18);
        
        // Subtitle (Centered)
        renderer.drawTextCentered("How can I help today?", 160.0f, 68.0f, Colors::TextSecondary, 10);

        // --- 3D Pulsing Centered Mic Button ---
        const float micCx = 160.0f;
        const float micCy = 126.0f;
        const float micR = 36.0f;

        // Dynamic pulsing factor based on elapsed time (smooth sine wave)
        float pulse = std::sin(m_elapsed * 4.0f) * 0.5f + 0.5f; // 0.0 to 1.0

        // 1. Concentric pulsing halo rings
        renderer.drawCircle(micCx, micCy, micR + 6.0f + pulse * 4.0f, SDL_Color { 155, 120, 255, static_cast<Uint8>(35 + pulse * 20) });
        renderer.drawCircle(micCx, micCy, micR + 12.0f + pulse * 8.0f, SDL_Color { 155, 120, 255, static_cast<Uint8>(10 + pulse * 10) });

        // 2. Soft outer button shadow
        renderer.drawSoftShadow(micCx - micR, micCy - micR, micR * 2.0f, micR * 2.0f, micR, 4, SDL_Color { 0, 0, 0, 16 });
        
        // 3. Thick purple outer ring
        renderer.fillCircle(micCx, micCy, micR, Colors::Primary);
        renderer.drawCircle(micCx, micCy, micR, SDL_Color { 255, 255, 255, 60 });

        // 4. Inner white disc to create a premium 3D ring border
        renderer.fillCircle(micCx, micCy, micR - 2.5f, SDL_Color { 255, 255, 255, 255 });

        // 5. Very soft inner purple-to-white gradient for depth
        float innerR = micR - 5.0f;
        renderer.fillVerticalGradient(micCx - innerR, micCy - innerR, innerR * 2.0f, innerR * 2.0f, 
                                     SDL_Color { 246, 241, 255, 255 }, SDL_Color { 255, 255, 255, 255 });
        renderer.drawCircle(micCx, micCy, innerR, SDL_Color { 235, 230, 248, 255 });

        // 6. Mic icon in the center in theme purple
        const float micIconSz = 24.0f;
        drawIcon(renderer, Icon::Mic, micCx - micIconSz * 0.5f, micCy - micIconSz * 0.5f, micIconSz, Colors::Primary);

        // Tap to Record helper text centered below the button
        renderer.drawTextCentered("Tap to Record", 160.0f, 178.0f, Colors::Primary, 11);

        // --- Right Chevron Button ---
        const float btnCx = 295.0f;
        const float btnCy = 126.0f;
        const float btnR = 15.0f;

        // Circular background (semi-transparent white card)
        renderer.fillCircle(btnCx, btnCy, btnR, SDL_Color { 255, 255, 255, 200 });
        renderer.drawCircle(btnCx, btnCy, btnR, SDL_Color { 235, 235, 240, 255 });

        const float chevSize = 10.0f;
        drawIcon(renderer, Icon::ChevronRight, btnCx - chevSize * 0.5f, btnCy - chevSize * 0.5f, chevSize, Colors::Primary);
    }

    // Page 1 — Menu List
    void HomeScreen::renderPage1(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Menu", true, false, Icon::Plus);

        int remCount = app.services().reminders ? static_cast<int>(app.services().reminders->getAll().size()) : 3;
        int ideaCount = app.services().ideas ? static_cast<int>(app.services().ideas->getAll().size()) : 4;
        int qCount = app.services().questions ? static_cast<int>(app.services().questions->getAll().size()) : 4;
        int memCount = app.services().memoryService ? static_cast<int>(app.services().memoryService->getAll().size()) : 6;

        const std::array<MenuEntry, 7> menuItems { {
            { Icon::Bell, "Reminders", SDL_Color { 145, 105, 255, 255 }, ScreenId::Reminders, remCount },
            { Icon::Lightbulb, "Ideas", SDL_Color { 255, 180, 40, 255 }, ScreenId::Ideas, ideaCount },
            { Icon::Question, "Questions", SDL_Color { 80, 185, 255, 255 }, ScreenId::Questions, qCount },
            { Icon::Search, "Search", SDL_Color { 75, 210, 140, 255 }, ScreenId::Search, 0 },
            { Icon::Mic, "Recordings", SDL_Color { 255, 105, 150, 255 }, ScreenId::Others, memCount },
            { Icon::Folder, "Others", SDL_Color { 150, 150, 160, 255 }, ScreenId::Others, 0 },
            { Icon::Settings, "Settings", SDL_Color { 85, 95, 110, 255 }, ScreenId::Settings, 0 }
        } };

        // Clip region for scrollable menu items
        renderer.setClipRect(5.0f, 48.0f, 310.0f, 172.0f);

        for (std::size_t i = 0; i < menuItems.size(); ++i)
        {
            const auto& item = menuItems[i];
            const float itemY = 50.0f + i * 50.0f - m_menuScrollY;

            // Render tile rounded card: height 44.0f
            renderer.fillRoundedRect(10.0f, itemY, 300.0f, 44.0f, 10.0f, SDL_Color { 255, 255, 255, 255 });
            renderer.drawRoundedRect(10.0f, itemY, 300.0f, 44.0f, 10.0f, SDL_Color { 235, 235, 240, 255 });

            // Icon Circle: diameter 28 (radius 14.0f)
            const float cy = itemY + 22.0f;
            renderer.fillCircle(26.0f, cy, 14.0f, item.color);
            drawIcon(renderer, item.icon, 26.0f - 7.0f, cy - 7.0f, 14.0f, Colors::White);

            // Text Label: shifted right and font size 12
            renderer.drawText(item.label, 50.0f, itemY + 12.0f, Colors::TextPrimary, 12);

            // Badge Count (if > 0)
            if (item.badgeCount > 0)
            {
                char badgeStr[8];
                SDL_snprintf(badgeStr, sizeof(badgeStr), "%d", item.badgeCount);
                
                renderer.fillCircle(255.0f, cy, 10.0f, SDL_Color { 240, 235, 255, 255 });
                renderer.drawTextCentered(badgeStr, 255.0f, cy - 5.0f, Colors::Primary, 9);
            }

            // Right Chevron: centered and size 12
            drawIcon(renderer, Icon::ChevronRight, 280.0f, cy - 6.0f, 12.0f, SDL_Color { 180, 180, 185, 255 });
        }

        renderer.clearClipRect();
    }
}
