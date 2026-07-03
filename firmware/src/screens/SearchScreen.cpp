// SearchScreen.cpp — smartwatch search screen for Waveshare 2.8" 320x240 display
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

    std::vector<std::string> getWordSuggestions(const std::string& currentText)
    {
        static const std::vector<std::string> vocab = {
            "YouTube", "Voxa", "Birthday", "Party", "Family", "Meeting", "Reminder", "Schedule", 
            "Tomorrow", "Integration", "Workspace", "Shopping", "Grocery", "Project", "Presentation", 
            "AI", "Personal", "Assistant", "Voice", "Note", "Question", "Answer", "Search", "Settings", 
            "Google", "Deepmind", "Idea", "Call", "Sofia", "Emily", "Gift", "Dinner", "Today", "Week"
        };

        if (currentText.empty())
        {
            return {"YouTube", "Birthday", "Meeting"};
        }

        std::string lastWord;
        auto lastSpace = currentText.find_last_of(" \t\n\r");
        if (lastSpace == std::string::npos)
        {
            lastWord = currentText;
        }
        else
        {
            lastWord = currentText.substr(lastSpace + 1);
        }

        if (lastWord.empty())
        {
            return {"YouTube", "Birthday", "Meeting"};
        }

        std::string lowerWord = lastWord;
        std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), [](unsigned char c){ return std::tolower(c); });

        std::vector<std::string> matches;
        for (const auto& w : vocab)
        {
            std::string lowerW = w;
            std::transform(lowerW.begin(), lowerW.end(), lowerW.begin(), [](unsigned char c){ return std::tolower(c); });
            if (lowerW.rfind(lowerWord, 0) == 0)
            {
                matches.push_back(w);
                if (matches.size() >= 3) break;
            }
        }

        if (matches.size() < 3)
        {
            std::vector<std::string> defaults = {"the", "a", "is"};
            for (const auto& d : defaults)
            {
                if (std::find(matches.begin(), matches.end(), d) == matches.end())
                {
                    matches.push_back(d);
                    if (matches.size() >= 3) break;
                }
            }
        }

        return matches;
    }

    void applySuggestion(std::string& currentText, const std::string& suggestion)
    {
        auto lastSpace = currentText.find_last_of(" \t\n\r");
        if (lastSpace == std::string::npos)
        {
            currentText = suggestion + " ";
        }
        else
        {
            currentText = currentText.substr(0, lastSpace + 1) + suggestion + " ";
        }
    }

    std::vector<KeyDefinition> getKeyboardKeys(float startX, float startY, int mode)
    {
        std::vector<KeyDefinition> keys;
        if (mode == 0) // Alphabet
        {
            // Row 1 (Q-P) - 10 keys: 26x20 size, spacing 4
            const char* row1[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
            for (int i = 0; i < 10; ++i) {
                keys.push_back({row1[i], startX + 5.0f + i * 30.0f, startY + 24.0f, 26.0f, 20.0f, row1[i]});
            }
            
            // Row 2 (A-L) - 9 keys
            const char* row2[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};
            for (int i = 0; i < 9; ++i) {
                keys.push_back({row2[i], startX + 20.0f + i * 30.0f, startY + 48.0f, 26.0f, 20.0f, row2[i]});
            }
            
            // Row 3
            keys.push_back({"Shift", startX + 5.0f, startY + 72.0f, 32.0f, 20.0f, "shift"});
            const char* row3[] = {"Z", "X", "C", "V", "B", "N", "M"};
            for (int i = 0; i < 7; ++i) {
                keys.push_back({row3[i], startX + 41.0f + i * 30.0f, startY + 72.0f, 26.0f, 20.0f, row3[i]});
            }
            keys.push_back({"Del", startX + 255.0f, startY + 72.0f, 50.0f, 20.0f, "backspace"});
            
            // Row 4
            keys.push_back({"?12", startX + 5.0f, startY + 96.0f, 45.0f, 20.0f, "mode_sym"});
            keys.push_back({"Space", startX + 54.0f, startY + 96.0f, 182.0f, 20.0f, "space"});
            keys.push_back({"Done", startX + 240.0f, startY + 96.0f, 65.0f, 20.0f, "search"});
        }
        else if (mode == 1) // Numbers & Punctuation
        {
            // Row 1
            const char* row1[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
            for (int i = 0; i < 10; ++i) {
                keys.push_back({row1[i], startX + 5.0f + i * 30.0f, startY + 24.0f, 26.0f, 20.0f, row1[i]});
            }
            
            // Row 2
            const char* row2[] = {"-", "/", ":", ";", "(", ")", "$", "&", "@", "\""};
            for (int i = 0; i < 10; ++i) {
                keys.push_back({row2[i], startX + 5.0f + i * 30.0f, startY + 48.0f, 26.0f, 20.0f, row2[i]});
            }
            
            // Row 3
            keys.push_back({"#+=", startX + 5.0f, startY + 72.0f, 40.0f, 20.0f, "mode_extra"});
            const char* row3[] = {".", ",", "?", "!", "'", "_"};
            for (int i = 0; i < 6; ++i) {
                keys.push_back({row3[i], startX + 49.0f + i * 30.0f, startY + 72.0f, 26.0f, 20.0f, row3[i]});
            }
            keys.push_back({"Del", startX + 255.0f, startY + 72.0f, 50.0f, 20.0f, "backspace"});
            
            // Row 4
            keys.push_back({"ABC", startX + 5.0f, startY + 96.0f, 45.0f, 20.0f, "mode_abc"});
            keys.push_back({"Space", startX + 54.0f, startY + 96.0f, 182.0f, 20.0f, "space"});
            keys.push_back({"Done", startX + 240.0f, startY + 96.0f, 65.0f, 20.0f, "search"});
        }
        else // Extra symbols
        {
            // Row 1
            const char* row1[] = {"[", "]", "{", "}", "#", "%", "^", "*", "+", "="};
            for (int i = 0; i < 10; ++i) {
                keys.push_back({row1[i], startX + 5.0f + i * 30.0f, startY + 24.0f, 26.0f, 20.0f, row1[i]});
            }
            
            // Row 2
            const char* row2[] = {"_", "\\", "|", "~", "<", ">", "$", "P", "Y", "."};
            for (int i = 0; i < 10; ++i) {
                keys.push_back({row2[i], startX + 5.0f + i * 30.0f, startY + 48.0f, 26.0f, 20.0f, row2[i]});
            }
            
            // Row 3
            keys.push_back({"?12", startX + 5.0f, startY + 72.0f, 40.0f, 20.0f, "mode_sym"});
            const char* row3[] = {".", ",", "?", "!", "'", "_"};
            for (int i = 0; i < 6; ++i) {
                keys.push_back({row3[i], startX + 49.0f + i * 30.0f, startY + 72.0f, 26.0f, 20.0f, row3[i]});
            }
            keys.push_back({"Del", startX + 255.0f, startY + 72.0f, 50.0f, 20.0f, "backspace"});
            
            // Row 4
            keys.push_back({"ABC", startX + 5.0f, startY + 96.0f, 45.0f, 20.0f, "mode_abc"});
            keys.push_back({"Space", startX + 54.0f, startY + 96.0f, 182.0f, 20.0f, "space"});
            keys.push_back({"Done", startX + 240.0f, startY + 96.0f, 65.0f, 20.0f, "search"});
        }
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
        // 1. Handle physical keyboard inputs
        if (m_keyboardOpen && event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key >= SDLK_A && event.key.key <= SDLK_Z)
            {
                app.audio().playClick();
                char c = 'a' + (event.key.key - SDLK_A);
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
            if (Rect { 10.0f, 110.0f, 300.0f, 120.0f }.contains(mPt.x, mPt.y))
            {
                m_targetScrollY -= event.wheel.y * 20.0f;
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
            if (m_isDragging)
            {
                const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
                float diffY = std::abs(point.y - m_dragStartY);
                if (diffY < 6.0f)
                {
                    // Tapped a search result tile!
                    std::vector<SearchResult> results;
                    if (app.services().search)
                    {
                        if (m_searchQuery.empty())
                        {
                            results = app.services().search->getRecent(5);
                        }
                        else
                        {
                            results = app.services().search->search(m_searchQuery);
                        }
                    }

                    for (std::size_t i = 0; i < results.size(); ++i)
                    {
                        Rect tileRect { 10.0f, 110.0f + i * 54.0f - m_scrollY, 300.0f, 48.0f };
                        if (tileRect.contains(point.x, point.y) && point.y >= 110.0f && point.y <= 230.0f)
                        {
                            std::string detailCat = results[i].category + "s";
                            if (results[i].category == "memory") detailCat = "others";
                            
                            app.setSelectedItem(detailCat, results[i].sourceId);
                            app.navigateTo(ScreenId::Detail);
                            break;
                        }
                    }
                }
                m_isDragging = false;
            }
            return;
        }

        if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            return;
        }

        const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
        
        // Circular Back Button (Large 40x40 tap target in top-left)
        if (Rect { 0.0f, 0.0f, 40.0f, 40.0f }.contains(point.x, point.y))
        {
            app.navigateBack();
            return;
        }

        // Clear button tap
        if (Rect { 260.0f, 85.0f, 50.0f, 20.0f }.contains(point.x, point.y))
        {
            app.audio().playSoftConfirm();
            m_searchQuery = "";
            return;
        }

        // List drag-scrolling
        if (Rect { 10.0f, 110.0f, 300.0f, 120.0f }.contains(point.x, point.y))
        {
            m_isDragging = true;
            m_dragStartY = point.y;
            m_dragStartScrollY = m_targetScrollY;
        }

        // Search Bar click toggles keyboard or voice search
        const Rect searchBarRect { 10.0f, 50.0f, 300.0f, 32.0f };
        if (searchBarRect.contains(point.x, point.y))
        {
            if (point.x >= 270.0f)
            {
                m_voiceSearchOpen = true;
                m_keyboardOpen = false;
                app.audio().playSoftConfirm();
            }
            else
            {
                m_keyboardOpen = true;
                m_voiceSearchOpen = false;
            }
            return;
        }

        // Handle inline voice search "Stop" button click
        if (m_voiceSearchOpen && m_voiceSearchAnim > 0.8f)
        {
            const float cx = 160.0f;
            const float cardY = 240.0f - m_voiceSearchAnim * 170.0f;
            const Rect stopBtn { cx - 60.0f, cardY + 110.0f, 120.0f, 24.0f };
            if (stopBtn.contains(point.x, point.y))
            {
                app.audio().playSoftConfirm();
                m_searchQuery = "YouTube Integration"; // mock query results
                m_voiceSearchOpen = false;
                return;
            }
        }

        // Handle virtual keyboard clicks
        if (m_keyboardOpen && m_keyboardAnim > 0.8f)
        {
            const float kbdY = 240.0f - m_keyboardAnim * 135.0f;

            std::string* activeStr = &m_searchQuery;

            // Check suggestion bar click: kbdY to kbdY + 20.0f
            if (point.y >= kbdY && point.y <= kbdY + 20.0f && point.x >= 5.0f && point.x <= 315.0f)
            {
                auto suggestions = getWordSuggestions(*activeStr);
                if (point.x >= 10.0f && point.x <= 100.0f && suggestions.size() > 0)
                {
                    app.audio().playClick();
                    applySuggestion(*activeStr, suggestions[0]);
                }
                else if (point.x >= 110.0f && point.x <= 200.0f && suggestions.size() > 1)
                {
                    app.audio().playClick();
                    applySuggestion(*activeStr, suggestions[1]);
                }
                else if (point.x >= 210.0f && point.x <= 300.0f && suggestions.size() > 2)
                {
                    app.audio().playClick();
                    applySuggestion(*activeStr, suggestions[2]);
                }
                return;
            }

            auto keys = getKeyboardKeys(5.0f, kbdY, m_keyboardMode);
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
                        m_keyboardMode = 0;
                    }
                    else if (act == "shift")
                    {
                        m_keyboardShift = !m_keyboardShift;
                    }
                    else if (act == "mode_sym")
                    {
                        m_keyboardMode = 1;
                    }
                    else if (act == "mode_abc")
                    {
                        m_keyboardMode = 0;
                    }
                    else if (act == "mode_extra")
                    {
                        m_keyboardMode = 2;
                    }
                    else
                    {
                        std::string charToInsert = act;
                        if (charToInsert.size() == 1 && std::isalpha(charToInsert[0]))
                        {
                            if (!m_keyboardShift) charToInsert[0] = std::tolower(charToInsert[0]);
                        }
                        m_searchQuery += charToInsert;
                    }
                    return;
                }
            }

            // Click inside keyboard eats the event
            if (Rect{ 5.0f, kbdY, 310.0f, 135.0f }.contains(point.x, point.y))
            {
                return;
            }
        }

        // Click completely outside closes keyboard
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

        // Calculate scroll limits
        std::vector<SearchResult> results;
        if (app.services().search)
        {
            if (m_searchQuery.empty())
            {
                results = app.services().search->getRecent(5);
            }
            else
            {
                results = app.services().search->search(m_searchQuery);
            }
        }

        float contentHeight = std::max(0.0f, static_cast<float>(results.size()) * 54.0f);
        float visibleHeight = 120.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        m_targetScrollY = std::clamp(m_targetScrollY, 0.0f, maxScrollY);
        m_scrollY += (m_targetScrollY - m_scrollY) * 12.0f * deltaSeconds;
        if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
        {
            m_scrollY = m_targetScrollY;
        }
    }

    void SearchScreen::render(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Search", true, false, Icon::Plus);

        float mx = 0.0f, my = 0.0f;
        SDL_GetMouseState(&mx, &my);
        const SDL_FPoint mPt = app.windowToCanvas(mx, my);

        std::string searchDisplay = m_searchQuery;
        if (m_keyboardOpen && static_cast<int>(m_elapsed * 2.0f) % 2 == 0)
        {
            searchDisplay += "|";
        }

        // Draw watch Search Bar Capsule
        const Rect searchBarRect { 10.0f, 50.0f, 300.0f, 32.0f };
        
        renderer.fillRoundedRect(searchBarRect.x, searchBarRect.y, searchBarRect.w, searchBarRect.h, 16.0f, 
            SDL_Color { 255, 255, 255, 255 });
        renderer.drawRoundedRect(searchBarRect.x, searchBarRect.y, searchBarRect.w, searchBarRect.h, 16.0f, 
            m_keyboardOpen ? Colors::PrimaryLight : SDL_Color { 235, 235, 240, 255 });
        
        // Search icon on left
        drawIcon(renderer, Icon::Search, searchBarRect.x + 10.0f, searchBarRect.y + 10.0f, 12.0f, Colors::TextSecondary);

        // Input text
        if (m_searchQuery.empty())
        {
            renderer.drawText("Search anything...", searchBarRect.x + 28.0f, searchBarRect.y + 9.0f, SDL_Color { 160, 160, 165, 255 }, 11);
        }
        else
        {
            renderer.drawText(searchDisplay, searchBarRect.x + 28.0f, searchBarRect.y + 9.0f, Colors::TextPrimary, 11);
        }

        // Purple mic icon on right
        drawIcon(renderer, Icon::Mic, searchBarRect.x + searchBarRect.w - 22.0f, searchBarRect.y + 9.0f, 13.0f, Colors::Primary);

        // Section header
        if (m_searchQuery.empty())
        {
            renderer.drawText("Recent Searches", 12.0f, 94.0f, Colors::TextPrimary, 11);
        }
        else
        {
            renderer.drawText("Search Results", 12.0f, 94.0f, Colors::TextPrimary, 11);
        }

        // Clear button
        renderer.drawTextCentered("Clear", 285.0f, 94.0f, Colors::Primary, 10);

        std::vector<SearchResult> results;
        if (app.services().search)
        {
            if (m_searchQuery.empty())
            {
                results = app.services().search->getRecent(5);
            }
            else
            {
                results = app.services().search->search(m_searchQuery);
            }
        }

        // Set clipping region for scrolling search items
        renderer.setClipRect(5.0f, 110.0f, 310.0f, 120.0f);

        for (std::size_t i = 0; i < results.size(); ++i)
        {
            const auto& res = results[i];
            Icon icon = getCategoryIcon(res.category);
            
            // If recent search (query is empty), show a Clock/History icon instead!
            if (m_searchQuery.empty())
            {
                icon = Icon::Calendar; // maps to clock/history look
            }

            SDL_Color color = getCategoryColor(res.category);
            if (m_searchQuery.empty())
            {
                color = SDL_Color { 150, 150, 160, 255 }; // generic grey for recents
            }

            const Rect tileRect { 10.0f, 110.0f + i * 54.0f - m_scrollY, 300.0f, 48.0f };

            ListTile tile(tileRect, icon, res.title.c_str(), res.timestamp.c_str(), color, 
                SDL_Color { 0, 0, 0, 0 }, true);
            tile.render(renderer);
        }

        renderer.clearClipRect();

        // Render sliding watch QWERTY Virtual Keyboard
        if (m_keyboardAnim > 0.0f)
        {
            const float kbdX = 5.0f;
            const float kbdY = 240.0f - m_keyboardAnim * 135.0f;
            const float kbdW = 310.0f;
            const float kbdH = 135.0f;

            Card kbdPanel(Rect{ kbdX, kbdY, kbdW, kbdH }, SDL_Color { 255, 255, 255, 240 }, 12.0f);
            kbdPanel.setShadow(Colors::Shadow, 6);
            kbdPanel.setBorder(Colors::PrimaryLight);
            kbdPanel.render(renderer);

            // Suggestion bar
            renderer.drawLine(kbdX, kbdY + 20.0f, kbdX + kbdW, kbdY + 20.0f, Colors::GlassBorder);
            auto suggestions = getWordSuggestions(m_searchQuery);
            for (std::size_t i = 0; i < suggestions.size(); ++i)
            {
                float sugX = kbdX + 10.0f + i * 100.0f;
                Rect sugRect{ sugX, kbdY + 3.0f, 90.0f, 14.0f };
                const bool sugHovered = sugRect.contains(mPt.x, mPt.y);
                
                renderer.fillRoundedRect(sugRect.x, sugRect.y, sugRect.w, sugRect.h, 7.0f, 
                    sugHovered ? SDL_Color{ 235, 230, 250, 255 } : SDL_Color{ 245, 243, 248, 200 });
                renderer.drawRoundedRect(sugRect.x, sugRect.y, sugRect.w, sugRect.h, 7.0f, Colors::GlassBorder);
                
                renderer.drawTextCentered(suggestions[i], sugRect.x + sugRect.w * 0.5f, sugRect.y + 2.0f, Colors::Primary, 9);
            }

            // Draw QWERTY keys
            auto keys = getKeyboardKeys(kbdX, kbdY, m_keyboardMode);
            for (const auto& key : keys)
            {
                const bool keyHovered = Rect{ key.x, key.y, key.w, key.h }.contains(mPt.x, mPt.y);
                SDL_Color keyFill = keyHovered ? SDL_Color { 235, 230, 250, 255 } : SDL_Color { 248, 248, 250, 200 };
                SDL_Color keyBorder = keyHovered ? Colors::Primary : Colors::GlassBorder;
                
                std::string act = key.action;
                if (act == "space" || act == "backspace" || act == "close" || act == "search" || act == "shift" || act == "mode_sym" || act == "mode_abc" || act == "mode_extra")
                {
                    keyFill = keyHovered ? SDL_Color { 210, 195, 255, 255 } : SDL_Color { 230, 226, 240, 255 };
                }

                if (act == "shift" && m_keyboardShift)
                {
                    keyFill = Colors::Primary;
                }

                renderer.fillRoundedRect(key.x, key.y, key.w, key.h, 6.0f, keyFill);
                renderer.drawRoundedRect(key.x, key.y, key.w, key.h, 6.0f, keyBorder);

                SDL_Color textColor = (keyHovered && (act == "space" || act == "backspace" || act == "close" || act == "search")) ? Colors::White : Colors::TextPrimary;
                if (act == "shift" && m_keyboardShift) textColor = Colors::White;

                std::string keyLabel = key.label;
                if (m_keyboardMode == 0 && !m_keyboardShift && keyLabel.size() == 1 && std::isalpha(keyLabel[0]))
                {
                    keyLabel[0] = std::tolower(keyLabel[0]);
                }
                renderer.drawTextCentered(keyLabel, key.x + key.w * 0.5f, key.y + key.h * 0.25f, textColor, 10);
            }
        }

        // Render dedicated voice search overlay
        if (m_voiceSearchAnim > 0.0f)
        {
            const float cardW = 260.0f;
            const float cardH = 150.0f;
            const float cardX = 160.0f - cardW * 0.5f;
            const float cardY = 240.0f - m_voiceSearchAnim * 170.0f;

            Card voiceCard(Rect{ cardX, cardY, cardW, cardH }, SDL_Color { 255, 255, 255, 240 }, 16.0f);
            voiceCard.setShadow(Colors::Shadow, 8);
            voiceCard.setBorder(Colors::PrimaryLight);
            voiceCard.render(renderer);

            const float cx = cardX + 45.0f;
            const float cy = cardY + 55.0f;
            const float pulse = std::sin(m_voiceElapsed * 4.0f) * 3.0f;

            renderer.drawGlowCircle(cx, cy, 28.0f + pulse, SDL_Color { 124, 92, 255, 30 }, 4);
            renderer.fillCircleGradient(cx, cy, 20.0f + pulse * 0.2f, SDL_Color { 255, 255, 255, 255 }, SDL_Color { 242, 236, 252, 255 });
            renderer.drawCircle(cx, cy, 20.0f + pulse * 0.2f, Colors::Primary);
            drawIcon(renderer, Icon::Mic, cx - 10.0f, cy - 10.0f, 20.0f, Colors::Primary);

            renderer.drawText("Voice Search", cardX + 85.0f, cardY + 28.0f, Colors::TextPrimary, 13);
            renderer.drawText("Listening...", cardX + 85.0f, cardY + 48.0f, Colors::TextSecondary, 10);

            // Dynamic voice lines
            for (int i = 0; i < 8; ++i)
            {
                const float wx = cardX + 85.0f + i * 8.0f;
                const float amp = 4.0f + std::sin(m_voiceElapsed * 6.0f + static_cast<float>(i) * 0.5f) * 12.0f;
                renderer.drawLine(wx, cardY + 75.0f - amp * 0.5f, wx, cardY + 75.0f + amp * 0.5f, SDL_Color { 166, 123, 250, 200 });
            }

            // Stop button
            const float btnX = 160.0f - 60.0f;
            const float btnY = cardY + 110.0f;
            const Rect stopBtnRect { btnX, btnY, 120.0f, 24.0f };
            const bool stopHovered = stopBtnRect.contains(mPt.x, mPt.y);

            renderer.fillRoundedRect(btnX, btnY, 120.0f, 24.0f, 12.0f, stopHovered ? Colors::PrimaryDark : Colors::Primary);
            renderer.drawTextCentered("Stop & Search", 160.0f, btnY + 5.0f, Colors::White, 9);
        }
    }
}
