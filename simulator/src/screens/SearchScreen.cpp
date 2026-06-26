#include "SearchScreen.h"

#include <array>
#include <vector>
#include <cctype>
#include <algorithm>
#include <cmath>

#include "../core/Application.h"
#include "../core/services/SearchService.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Card.h"
#include "../widgets/FloatingButton.h"
#include "../widgets/ListTile.h"
#include "../widgets/SearchBar.h"
#include "ScreenCommon.h"

namespace
{
    VOXA::Icon getCategoryIcon(const std::string& cat)
    {
        if (cat == "reminder") return VOXA::Icon::Bell;
        if (cat == "idea") return VOXA::Icon::Lightbulb;
        if (cat == "question") return VOXA::Icon::Question;
        return VOXA::Icon::Mic;
    }

    SDL_Color getCategoryColor(const std::string& cat)
    {
        if (cat == "reminder") return SDL_Color { 255, 170, 40, 255 };
        if (cat == "idea") return SDL_Color { 255, 178, 40, 255 };
        if (cat == "question") return SDL_Color { 176, 84, 255, 255 };
        return SDL_Color { 147, 108, 255, 255 };
    }

    struct KeyDefinition
    {
        const char* label;
        float x;
        float y;
        float w;
        float h;
        const char* action;
    };

    std::vector<KeyDefinition> getKeyboardKeys(float startX, float startY)
    {
        std::vector<KeyDefinition> keys;
        
        // Row 1
        const char* row1[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
        float r1X = startX + 46.0f;
        for (int i = 0; i < 10; ++i) {
            keys.push_back({row1[i], r1X + i * 92.0f, startY + 20.0f, 80.0f, 54.0f, row1[i]});
        }
        
        // Row 2
        const char* row2[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};
        float r2X = startX + 92.0f;
        for (int i = 0; i < 9; ++i) {
            keys.push_back({row2[i], r2X + i * 92.0f, startY + 90.0f, 80.0f, 54.0f, row2[i]});
        }
        
        // Row 3
        keys.push_back({"Shift", startX + 62.0f, startY + 160.0f, 100.0f, 54.0f, "shift"});
        const char* row3[] = {"Z", "X", "C", "V", "B", "N", "M"};
        float r3X = startX + 62.0f + 112.0f;
        for (int i = 0; i < 7; ++i) {
            keys.push_back({row3[i], r3X + i * 92.0f, startY + 160.0f, 80.0f, 54.0f, row3[i]});
        }
        keys.push_back({"Delete", startX + 62.0f + 112.0f + 7 * 92.0f - 12.0f, startY + 160.0f, 120.0f, 54.0f, "backspace"});
        
        // Row 4
        keys.push_back({"Close", startX + 88.0f, startY + 230.0f, 140.0f, 54.0f, "close"});
        keys.push_back({"Space", startX + 88.0f + 152.0f, startY + 230.0f, 500.0f, 54.0f, "space"});
        keys.push_back({"Search", startX + 88.0f + 152.0f + 512.0f, startY + 230.0f, 160.0f, 54.0f, "search"});
        
        return keys;
    }
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
        m_keyboardOpen = false;
        m_keyboardAnim = 0.0f;
        m_searchQuery = "";
        m_voiceSearchOpen = false;
        m_voiceSearchAnim = 0.0f;
        m_voiceElapsed = 0.0f;
        m_scrollY = 0.0f;
        m_targetScrollY = 0.0f;
        m_isDragging = false;
    }

    void SearchScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        // 1. Handle hardware keyboard input when virtual keyboard is open
        if (m_keyboardOpen && event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key >= SDLK_A && event.key.key <= SDLK_Z)
            {
                app.audio().playClick();
                char c = 'a' + (event.key.key - SDLK_A);
                // Handle shift
                if (event.key.mod & SDL_KMOD_SHIFT) c = std::toupper(c);
                m_searchQuery += c;
                return;
            }
            else if (event.key.key == SDLK_SPACE)
            {
                app.audio().playClick();
                m_searchQuery += " ";
                return;
            }
            else if (event.key.key == SDLK_BACKSPACE)
            {
                app.audio().playClick();
                if (!m_searchQuery.empty()) m_searchQuery.pop_back();
                return;
            }
            else if (event.key.key == SDLK_RETURN)
            {
                app.audio().playClick();
                m_keyboardOpen = false;
                return;
            }
        }

        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            float mx = 0.0f, my = 0.0f;
            SDL_GetMouseState(&mx, &my);
            const SDL_FPoint mPt = app.windowToCanvas(mx, my);
            if (Rect { 86.0f, 278.0f, 930.0f, 612.0f }.contains(mPt.x, mPt.y))
            {
                m_targetScrollY -= event.wheel.y * 38.0f;
            }
            return;
        }

        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            if (m_isDragging)
            {
                const SDL_FPoint point = app.windowToCanvas(event.motion.x, event.motion.y);
                float diffY = point.y - m_dragStartY;
                m_targetScrollY = m_dragStartScrollY - diffY;
            }
            return;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            m_isDragging = false;
            return;
        }

        if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            return;
        }

        const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
        
        // Circular Back Button
        if (Rect { 44.0f, 34.0f, 56.0f, 56.0f }.contains(point.x, point.y))
        {
            app.navigateTo(ScreenId::Home);
            return;
        }

        // List drag-scrolling click detection
        if (Rect { 86.0f, 278.0f, 930.0f, 612.0f }.contains(point.x, point.y))
        {
            m_isDragging = true;
            m_dragStartY = point.y;
            m_dragStartScrollY = m_targetScrollY;
        }

        // Search Bar click toggles keyboard
        const Rect searchBarRect { 86.0f, 128.0f, 820.0f, 74.0f };
        if (searchBarRect.contains(point.x, point.y))
        {
            m_keyboardOpen = true;
            m_voiceSearchOpen = false;
            return;
        }

        // Floating Mic Button click triggers inline voice search overlay
        const Rect micBtnRect { 1412.0f, 750.0f, 96.0f, 96.0f };
        if (micBtnRect.contains(point.x, point.y))
        {
            m_voiceSearchOpen = true;
            m_keyboardOpen = false;
            app.audio().playSoftConfirm();
            return;
        }

        // Handle inline voice search "Stop" button click
        if (m_voiceSearchOpen && m_voiceSearchAnim > 0.8f)
        {
            const float cx = 800.0f;
            const float cardY = 900.0f - m_voiceSearchAnim * 300.0f;
            // Stop button: x = cx - 100.0f, y = cardY + 172.0f, w = 200.0f, h = 44.0f
            const Rect stopBtn { cx - 100.0f, cardY + 172.0f, 200.0f, 44.0f };
            if (stopBtn.contains(point.x, point.y))
            {
                app.audio().playSoftConfirm();
                m_searchQuery = "YouTube Integration"; // mock voice query results
                m_voiceSearchOpen = false;
                return;
            }
        }

        // Handle virtual keyboard keys click
        if (m_keyboardOpen && m_keyboardAnim > 0.8f)
        {
            const float kbdY = 900.0f - m_keyboardAnim * 330.0f;
            auto keys = getKeyboardKeys(300.0f, kbdY);
            for (const auto& key : keys)
            {
                if (Rect{ key.x, key.y, key.w, key.h }.contains(point.x, point.y))
                {
                    app.audio().playClick();
                    std::string act = key.action;
                    if (act == "backspace")
                    {
                        if (!m_searchQuery.empty()) m_searchQuery.pop_back();
                    }
                    else if (act == "space")
                    {
                        m_searchQuery += " ";
                    }
                    else if (act == "close" || act == "search")
                    {
                        m_keyboardOpen = false;
                    }
                    else if (act == "shift")
                    {
                        // Shift is visual only for simulation
                    }
                    else
                    {
                        m_searchQuery += act;
                    }
                    return;
                }
            }

            // Clicking inside keyboard panel but not on keys absorbs the click
            if (Rect{ 300.0f, kbdY, 1000.0f, 330.0f }.contains(point.x, point.y))
            {
                return;
            }
        }

        // Clicking outside search bar / keyboard closes it
        m_keyboardOpen = false;
    }

    void SearchScreen::update(Application& app, float deltaSeconds)
    {
        m_elapsed += deltaSeconds;

        if (m_keyboardOpen)
        {
            m_keyboardAnim = std::min(1.0f, m_keyboardAnim + deltaSeconds * 6.0f);
        }
        else
        {
            m_keyboardAnim = std::max(0.0f, m_keyboardAnim - deltaSeconds * 6.0f);
        }

        if (m_voiceSearchOpen)
        {
            m_voiceSearchAnim = std::min(1.0f, m_voiceSearchAnim + deltaSeconds * 6.0f);
            m_voiceElapsed += deltaSeconds;
        }
        else
        {
            m_voiceSearchAnim = std::max(0.0f, m_voiceSearchAnim - deltaSeconds * 6.0f);
            m_voiceElapsed = 0.0f;
        }
    }

    void SearchScreen::render(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Search", true, true, Icon::Filter);

        // Get mouse coordinates for hover state checks
        float mx = 0.0f, my = 0.0f;
        SDL_GetMouseState(&mx, &my);
        const SDL_FPoint mPt = app.windowToCanvas(mx, my);

        // Draw SearchBar with typed text (or placeholder if query is empty)
        // If query is not empty, draw it with active cursor
        std::string searchDisplay = m_searchQuery;
        if (m_keyboardOpen && static_cast<int>(m_elapsed * 2.0f) % 2 == 0)
        {
            searchDisplay += "|"; // Blinking typewriter cursor
        }

        // Draw Search Bar
        const Rect searchBarRect { 86.0f, 128.0f, 820.0f, 74.0f };
        const bool searchHovered = searchBarRect.contains(mPt.x, mPt.y) || m_keyboardOpen;
        
        renderer.fillRoundedRect(searchBarRect.x, searchBarRect.y, searchBarRect.w, searchBarRect.h, searchBarRect.h * 0.5f, 
            searchHovered ? SDL_Color { 255, 255, 255, 180 } : SDL_Color { 255, 255, 255, 120 });
        renderer.drawRoundedRect(searchBarRect.x, searchBarRect.y, searchBarRect.w, searchBarRect.h, searchBarRect.h * 0.5f, 
            m_keyboardOpen ? Colors::PrimaryLight : Colors::GlassBorder);
        
        if (m_searchQuery.empty())
        {
            renderer.drawText("Search memories, notes, recordings, and ideas...", searchBarRect.x + searchBarRect.h * 0.45f, searchBarRect.y + searchBarRect.h * 0.3f, SDL_Color { 140, 140, 145, 255 }, static_cast<int>(searchBarRect.h * 0.34f));
        }
        else
        {
            renderer.drawText(searchDisplay, searchBarRect.x + searchBarRect.h * 0.45f, searchBarRect.y + searchBarRect.h * 0.3f, Colors::TextPrimary, static_cast<int>(searchBarRect.h * 0.34f));
        }
        drawIcon(renderer, Icon::Search, searchBarRect.x + searchBarRect.w - searchBarRect.h * 0.8f, searchBarRect.y + searchBarRect.h * 0.25f, searchBarRect.h * 0.45f, Colors::TextSecondary);

        if (m_searchQuery.empty())
        {
            renderer.drawText("Recent Searches", 88.0f, 236.0f, Colors::TextPrimary, 18);
        }
        else
        {
            renderer.drawText("Search Results", 88.0f, 236.0f, Colors::TextPrimary, 18);
        }

        // Retrieve search results dynamically from the SearchService
        std::vector<SearchResult> results;
        if (app.services().search)
        {
            if (m_searchQuery.empty())
            {
                results = app.services().search->getRecent(10);
            }
            else
            {
                results = app.services().search->search(m_searchQuery);
            }
        }

        // Set clipping region to prevent scroll overlap with search bar or screen bottom
        renderer.setClipRect(80.0f, 270.0f, 960.0f, 620.0f);

        for (std::size_t i = 0; i < results.size(); ++i)
        {
            const auto& res = results[i];
            Icon icon = getCategoryIcon(res.category);
            SDL_Color color = getCategoryColor(res.category);

            const float slide = std::max(0.0f, 54.0f - m_elapsed * (72.0f + static_cast<float>(i) * 6.0f));
            const Rect tileRect { 86.0f, 278.0f + i * 122.0f + slide - m_scrollY, 930.0f, 96.0f };
            const bool tileHovered = tileRect.contains(mPt.x, mPt.y);

            ListTile tile(tileRect, icon, res.title.c_str(), res.timestamp.c_str(), color, 
                tileHovered ? Colors::CardHover : SDL_Color { 0, 0, 0, 0 }, true);
            tile.render(renderer);
        }

        renderer.clearClipRect();

        // Spotlight focus card on the right
        const Rect spotRect { 1070.0f, 128.0f, 424.0f, 570.0f };
        const bool spotHovered = spotRect.contains(mPt.x, mPt.y);
        Card spotlight(spotRect, spotHovered ? Colors::CardHover : SDL_Color { 255, 255, 255, 120 }, 32.0f);
        spotlight.setShadow(Colors::Shadow, 8);
        spotlight.setBorder(spotHovered ? Colors::PrimaryLight : Colors::GlassBorder);
        spotlight.render(renderer);
        renderer.drawText("Focus", 1116.0f, 178.0f, Colors::TextPrimary, 22);
        renderer.drawText("Memories are organized like a premium canvas.", 1116.0f, 228.0f, Colors::TextSecondary, 13);
        renderer.drawText("Tap the mic to open voice search instantly.", 1116.0f, 260.0f, Colors::TextSecondary, 13);
        
        const float glowRad = 112.0f + std::sin(m_elapsed * 2.0f) * 6.0f;
        renderer.drawGlowCircle(1282.0f, 470.0f, glowRad, SDL_Color { 124, 92, 255, 22 }, 10);
        renderer.fillCircleGradient(1282.0f, 470.0f, 88.0f, SDL_Color { 244, 238, 255, 255 }, SDL_Color { 162, 123, 250, 255 });
        drawIcon(renderer, Icon::Search, 1238.0f, 426.0f, 88.0f, Colors::White);

        // Floating Mic Button on bottom right
        const Rect micBtnRect { 1412.0f, 750.0f, 96.0f, 96.0f };
        const bool micHovered = micBtnRect.contains(mPt.x, mPt.y);
        FloatingButton button(1460.0f, 798.0f, micHovered ? 52.0f : 48.0f, Icon::Mic, Colors::Primary, Colors::White);
        button.render(renderer);

        // Render Sliding QWERTY Virtual Keyboard
        if (m_keyboardAnim > 0.0f)
        {
            const float kbdX = 300.0f;
            const float kbdY = 900.0f - m_keyboardAnim * 330.0f;
            const float kbdW = 1000.0f;
            const float kbdH = 330.0f;

            // Sleek frosted glass panel
            Card kbdPanel(Rect{ kbdX, kbdY, kbdW, kbdH }, SDL_Color { 255, 255, 255, 225 }, 28.0f);
            kbdPanel.setShadow(Colors::Shadow, 12);
            kbdPanel.setBorder(Colors::PrimaryLight);
            kbdPanel.render(renderer);

            // Draw keys
            auto keys = getKeyboardKeys(kbdX, kbdY);
            for (const auto& key : keys)
            {
                const bool keyHovered = Rect{ key.x, key.y, key.w, key.h }.contains(mPt.x, mPt.y);
                
                // Frosted key caps
                SDL_Color keyFill = keyHovered ? SDL_Color { 235, 230, 250, 255 } : SDL_Color { 248, 248, 250, 200 };
                SDL_Color keyBorder = keyHovered ? Colors::Primary : Colors::GlassBorder;
                
                std::string act = key.action;
                if (act == "space" || act == "backspace" || act == "close" || act == "search" || act == "shift")
                {
                    keyFill = keyHovered ? SDL_Color { 210, 195, 255, 255 } : SDL_Color { 230, 226, 240, 255 };
                }

                renderer.fillRoundedRect(key.x, key.y, key.w, key.h, 14.0f, keyFill);
                renderer.drawRoundedRect(key.x, key.y, key.w, key.h, 14.0f, keyBorder);

                // Draw label
                SDL_Color textColor = (keyHovered && (act == "space" || act == "backspace" || act == "close" || act == "search")) ? Colors::White : Colors::TextPrimary;
                renderer.drawTextCentered(key.label, key.x + key.w * 0.5f, key.y + key.h * 0.32f, textColor, 15);
            }
        }

        // Render Dedicated Voice Search Overlay
        if (m_voiceSearchAnim > 0.0f)
        {
            const float cardW = 700.0f;
            const float cardH = 240.0f;
            const float cardX = 800.0f - cardW * 0.5f; // centered
            const float cardY = 900.0f - m_voiceSearchAnim * 300.0f;

            Card voiceCard(Rect{ cardX, cardY, cardW, cardH }, SDL_Color { 255, 255, 255, 240 }, 32.0f);
            voiceCard.setShadow(Colors::Shadow, 16);
            voiceCard.setBorder(Colors::PrimaryLight);
            voiceCard.render(renderer);

            // Pulse center circle
            const float cx = cardX + 120.0f;
            const float cy = cardY + 120.0f;
            const float pulse = std::sin(m_voiceElapsed * 4.0f) * 8.0f;

            renderer.drawGlowCircle(cx, cy, 60.0f + pulse, SDL_Color { 124, 92, 255, 30 }, 6);
            renderer.fillCircleGradient(cx, cy, 48.0f + pulse * 0.2f, SDL_Color { 255, 255, 255, 255 }, SDL_Color { 242, 236, 252, 255 });
            renderer.drawCircle(cx, cy, 48.0f + pulse * 0.2f, Colors::Primary);
            drawIcon(renderer, Icon::Mic, cx - 18.0f, cy - 18.0f, 36.0f, Colors::Primary);

            // Listening text
            renderer.drawText("Voice Search", cardX + 220.0f, cardY + 54.0f, Colors::TextPrimary, 22);
            renderer.drawText("Listening for search keywords...", cardX + 220.0f, cardY + 96.0f, Colors::TextSecondary, 14);

            // Dynamic voice lines
            for (int i = 0; i < 15; ++i)
            {
                const float wx = cardX + 220.0f + i * 14.0f;
                const float amp = 8.0f + std::sin(m_voiceElapsed * 6.0f + static_cast<float>(i) * 0.5f) * 28.0f;
                renderer.drawLine(wx, cardY + 140.0f - amp * 0.5f, wx, cardY + 140.0f + amp * 0.5f, SDL_Color { 166, 123, 250, 200 });
            }

            // Stop button
            const float btnX = 800.0f - 100.0f;
            const float btnY = cardY + 172.0f;
            const Rect stopBtnRect { btnX, btnY, 200.0f, 44.0f };
            const bool stopHovered = stopBtnRect.contains(mPt.x, mPt.y);

            renderer.fillRoundedRect(btnX, btnY, 200.0f, 44.0f, 22.0f, stopHovered ? Colors::PrimaryDark : Colors::Primary);
            renderer.drawTextCentered("Stop & Search", 800.0f, btnY + 12.0f, Colors::White, 14);
        }
    }
}
