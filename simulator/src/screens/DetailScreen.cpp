#include "DetailScreen.h"

#include <array>
#include <cmath>
#include <algorithm>

#include "../core/Application.h"
#include "../core/services/StorageService.h"
#include "../core/services/MemoryService.h"
#include "../core/services/TimeService.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Card.h"
#include "../widgets/Button.h"
#include "../widgets/ListTile.h"
#include "ScreenCommon.h"

namespace
{
    struct KeyDef
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

    std::vector<KeyDef> getKeyboardKeys(float startX, float startY, int mode)
    {
        std::vector<KeyDef> keys;
        if (mode == 0) // Alphabet
        {
            // Row 1
            const char* row1[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
            for (int i = 0; i < 10; ++i) {
                keys.push_back({row1[i], startX + 44.0f + i * 92.0f, startY + 54.0f, 84.0f, 54.0f, row1[i]});
            }
            
            // Row 2
            const char* row2[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};
            for (int i = 0; i < 9; ++i) {
                keys.push_back({row2[i], startX + 90.0f + i * 92.0f, startY + 120.0f, 84.0f, 54.0f, row2[i]});
            }
            
            // Row 3
            keys.push_back({"Shift", startX + 44.0f, startY + 186.0f, 100.0f, 54.0f, "shift"});
            const char* row3[] = {"Z", "X", "C", "V", "B", "N", "M"};
            for (int i = 0; i < 7; ++i) {
                keys.push_back({row3[i], startX + 152.0f + i * 92.0f, startY + 186.0f, 84.0f, 54.0f, row3[i]});
            }
            keys.push_back({"Delete", startX + 796.0f, startY + 186.0f, 160.0f, 54.0f, "backspace"});
            
            // Row 4
            keys.push_back({"?123", startX + 44.0f, startY + 252.0f, 140.0f, 54.0f, "mode_sym"});
            keys.push_back({"Space", startX + 192.0f, startY + 252.0f, 612.0f, 54.0f, "space"});
            keys.push_back({"Done", startX + 812.0f, startY + 252.0f, 144.0f, 54.0f, "close"});
        }
        else if (mode == 1) // Numbers & Punctuation
        {
            // Row 1
            const char* row1[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
            for (int i = 0; i < 10; ++i) {
                keys.push_back({row1[i], startX + 44.0f + i * 92.0f, startY + 54.0f, 84.0f, 54.0f, row1[i]});
            }
            
            // Row 2
            const char* row2[] = {"-", "/", ":", ";", "(", ")", "$", "&", "@", "\""};
            for (int i = 0; i < 10; ++i) {
                keys.push_back({row2[i], startX + 44.0f + i * 92.0f, startY + 120.0f, 84.0f, 54.0f, row2[i]});
            }
            
            // Row 3
            keys.push_back({"#+=", startX + 44.0f, startY + 186.0f, 100.0f, 54.0f, "mode_extra"});
            const char* row3[] = {".", ",", "?", "!", "'", "_"};
            for (int i = 0; i < 6; ++i) {
                keys.push_back({row3[i], startX + 152.0f + i * 92.0f, startY + 186.0f, 84.0f, 54.0f, row3[i]});
            }
            keys.push_back({"Delete", startX + 796.0f, startY + 186.0f, 160.0f, 54.0f, "backspace"});
            
            // Row 4
            keys.push_back({"ABC", startX + 44.0f, startY + 252.0f, 140.0f, 54.0f, "mode_abc"});
            keys.push_back({"Space", startX + 192.0f, startY + 252.0f, 612.0f, 54.0f, "space"});
            keys.push_back({"Done", startX + 812.0f, startY + 252.0f, 144.0f, 54.0f, "close"});
        }
        else // Extra symbols
        {
            // Row 1
            const char* row1[] = {"[", "]", "{", "}", "#", "%", "^", "*", "+", "="};
            for (int i = 0; i < 10; ++i) {
                keys.push_back({row1[i], startX + 44.0f + i * 92.0f, startY + 54.0f, 84.0f, 54.0f, row1[i]});
            }
            
            // Row 2
            const char* row2[] = {"_", "\\", "|", "~", "<", ">", "\u20AC", "\u00A3", "\u00A5", "\u2022"};
            for (int i = 0; i < 10; ++i) {
                keys.push_back({row2[i], startX + 44.0f + i * 92.0f, startY + 120.0f, 84.0f, 54.0f, row2[i]});
            }
            
            // Row 3
            keys.push_back({"?123", startX + 44.0f, startY + 186.0f, 100.0f, 54.0f, "mode_sym"});
            const char* row3[] = {".", ",", "?", "!", "'", "_"};
            for (int i = 0; i < 6; ++i) {
                keys.push_back({row3[i], startX + 152.0f + i * 92.0f, startY + 186.0f, 84.0f, 54.0f, row3[i]});
            }
            keys.push_back({"Delete", startX + 796.0f, startY + 186.0f, 160.0f, 54.0f, "backspace"});
            
            // Row 4
            keys.push_back({"ABC", startX + 44.0f, startY + 252.0f, 140.0f, 54.0f, "mode_abc"});
            keys.push_back({"Space", startX + 192.0f, startY + 252.0f, 612.0f, 54.0f, "space"});
            keys.push_back({"Done", startX + 812.0f, startY + 252.0f, 144.0f, 54.0f, "close"});
        }
        return keys;
    }
}

namespace VOXA
{
    ScreenId DetailScreen::id() const
    {
        return ScreenId::Detail;
    }

    void DetailScreen::onEnter(Application& app)
    {
        m_elapsed = 0.0f;
        m_focusedField = 0;
        m_keyboardOpen = false;
        m_keyboardAnim = 0.0f;
        m_keyboardShift = false;
        m_editTitle = "";
        m_editContent = "";
        m_editComment = "";
        m_commentsList.clear();
        m_scrollY = 0.0f;
        m_targetScrollY = 0.0f;
        m_isDragging = false;

        const auto& selected = app.getSelectedItem();
        m_category = selected.category;
        m_itemId = selected.id;

        loadItem(app);
    }

    void DetailScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        // 1. Hardware keyboard support
        if (m_keyboardOpen && event.type == SDL_EVENT_KEY_DOWN)
        {
            std::string* activeStr = nullptr;
            if (m_focusedField == 1) activeStr = &m_editTitle;
            else if (m_focusedField == 2) activeStr = &m_editContent;
            else if (m_focusedField == 3) activeStr = &m_editComment;

            if (activeStr)
            {
                if (event.key.key >= SDLK_A && event.key.key <= SDLK_Z)
                {
                    app.audio().playClick();
                    char c = 'a' + (event.key.key - SDLK_A);
                    if (event.key.mod & SDL_KMOD_SHIFT || m_keyboardShift) c = std::toupper(c);
                    *activeStr += c;
                    return;
                }
                else if (event.key.key == SDLK_SPACE)
                {
                    app.audio().playClick();
                    *activeStr += " ";
                    return;
                }
                else if (event.key.key == SDLK_BACKSPACE)
                {
                    app.audio().playClick();
                    if (!activeStr->empty()) activeStr->pop_back();
                    return;
                }
                else if (event.key.key == SDLK_RETURN)
                {
                    app.audio().playClick();
                    m_keyboardOpen = false;
                    m_focusedField = 0;
                    return;
                }
            }
        }

        // 2. Mouse Wheel Scroll on Comments Area
        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            float mx = 0.0f, my = 0.0f;
            SDL_GetMouseState(&mx, &my);
            const SDL_FPoint mPt = app.windowToCanvas(mx, my);
            if (Rect { 810.0f, 205.0f, 450.0f, 250.0f }.contains(mPt.x, mPt.y))
            {
                m_targetScrollY -= event.wheel.y * 38.0f;
            }
            return;
        }

        // 3. Mouse Drag Scroll on Comments Area
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
            app.audio().playSoftConfirm();
            if (m_category == "ideas") app.navigateTo(ScreenId::Ideas);
            else if (m_category == "reminders") app.navigateTo(ScreenId::Reminders);
            else if (m_category == "questions") app.navigateTo(ScreenId::Questions);
            else app.navigateTo(ScreenId::Others);
            return;
        }

        // Tap inside comments scrollable area to drag
        if (Rect { 810.0f, 205.0f, 450.0f, 250.0f }.contains(point.x, point.y))
        {
            m_isDragging = true;
            m_dragStartY = point.y;
            m_dragStartScrollY = m_targetScrollY;
        }

        // Click inputs to focus & open keyboard
        // 1. Title Input Box: [340, 205, 420, 54]
        if (Rect { 340.0f, 205.0f, 420.0f, 54.0f }.contains(point.x, point.y))
        {
            m_focusedField = 1;
            m_keyboardOpen = true;
            return;
        }
        // 2. Content Input Box: [340, 315, 420, 94]
        if (Rect { 340.0f, 315.0f, 420.0f, 94.0f }.contains(point.x, point.y))
        {
            m_focusedField = 2;
            m_keyboardOpen = true;
            return;
        }
        // 3. New Comment Box: [810, 470, 320, 48]
        if (Rect { 810.0f, 470.0f, 320.0f, 48.0f }.contains(point.x, point.y))
        {
            m_focusedField = 3;
            m_keyboardOpen = true;
            return;
        }

        // Action Buttons Click detection
        // Update Button: [340, 440, 195, 48]
        if (Rect { 340.0f, 440.0f, 195.0f, 48.0f }.contains(point.x, point.y))
        {
            saveChanges(app);
            return;
        }
        // Delete Button: [565, 440, 195, 48]
        if (Rect { 565.0f, 440.0f, 195.0f, 48.0f }.contains(point.x, point.y))
        {
            deleteItem(app);
            return;
        }
        // Add Comment Button: [1150, 470, 110, 48]
        if (Rect { 1150.0f, 470.0f, 110.0f, 48.0f }.contains(point.x, point.y))
        {
            addComment(app);
            return;
        }

        // Virtual Keyboard Keys Click detection
        if (m_keyboardOpen && m_keyboardAnim > 0.8f)
        {
            const float kbdY = 900.0f - m_keyboardAnim * 330.0f;

            std::string* activeStr = nullptr;
            if (m_focusedField == 1) activeStr = &m_editTitle;
            else if (m_focusedField == 2) activeStr = &m_editContent;
            else if (m_focusedField == 3) activeStr = &m_editComment;

            // Check suggestion bar click: kbdY to kbdY + 44.0f
            if (point.y >= kbdY && point.y <= kbdY + 44.0f && point.x >= 300.0f && point.x <= 1300.0f)
            {
                if (activeStr)
                {
                    auto suggestions = getWordSuggestions(*activeStr);
                    if (point.x >= 350.0f && point.x <= 620.0f && suggestions.size() > 0)
                    {
                        app.audio().playClick();
                        applySuggestion(*activeStr, suggestions[0]);
                    }
                    else if (point.x >= 665.0f && point.x <= 935.0f && suggestions.size() > 1)
                    {
                        app.audio().playClick();
                        applySuggestion(*activeStr, suggestions[1]);
                    }
                    else if (point.x >= 980.0f && point.x <= 1250.0f && suggestions.size() > 2)
                    {
                        app.audio().playClick();
                        applySuggestion(*activeStr, suggestions[2]);
                    }
                }
                return;
            }

            auto keys = getKeyboardKeys(300.0f, kbdY, m_keyboardMode);
            for (const auto& key : keys)
            {
                if (Rect{ key.x, key.y, key.w, key.h }.contains(point.x, point.y))
                {
                    app.audio().playClick();
                    std::string act = key.action;
                    
                    if (act == "backspace")
                    {
                        if (activeStr && !activeStr->empty()) activeStr->pop_back();
                    }
                    else if (act == "space")
                    {
                        if (activeStr) *activeStr += " ";
                    }
                    else if (act == "close")
                    {
                        m_keyboardOpen = false;
                        m_focusedField = 0;
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
                        if (activeStr)
                        {
                            std::string charToInsert = act;
                            if (charToInsert.size() == 1 && std::isalpha(charToInsert[0]))
                            {
                                if (!m_keyboardShift) charToInsert[0] = std::tolower(charToInsert[0]);
                            }
                            *activeStr += charToInsert;
                        }
                    }
                    return;
                }
            }

            // Click inside keyboard eats the event
            if (Rect{ 300.0f, kbdY, 1000.0f, 330.0f }.contains(point.x, point.y))
            {
                return;
            }
        }

        // Click completely outside closes keyboard
        m_keyboardOpen = false;
        m_focusedField = 0;
    }

    void DetailScreen::update(Application& app, float deltaSeconds)
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

        // Smooth scroll limits for comments
        float contentHeight = std::max(0.0f, static_cast<float>(m_commentsList.size()) * 48.0f);
        float visibleHeight = 250.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        m_targetScrollY = std::clamp(m_targetScrollY, 0.0f, maxScrollY);
        m_scrollY += (m_targetScrollY - m_scrollY) * 12.0f * deltaSeconds;
        if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
        {
            m_scrollY = m_targetScrollY;
        }
    }

    void DetailScreen::render(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Detail View", true, false, Icon::Plus);

        float mx = 0.0f, my = 0.0f;
        SDL_GetMouseState(&mx, &my);
        const SDL_FPoint mPt = app.windowToCanvas(mx, my);

        // Center glass card container
        Card container(Rect { 300.0f, 140.0f, 1000.0f, 640.0f }, Colors::Card, 32.0f);
        container.setShadow(Colors::Shadow, 8);
        container.setBorder(Colors::GlassBorder);
        container.render(renderer);

        // --- LEFT COLUMN ---
        renderer.drawText("Title", 340.0f, 175.0f, Colors::TextSecondary, 14);
        Rect titleRect { 340.0f, 205.0f, 420.0f, 54.0f };
        bool titleHovered = titleRect.contains(mPt.x, mPt.y) || m_focusedField == 1;
        renderer.fillRoundedRect(titleRect.x, titleRect.y, titleRect.w, titleRect.h, 12.0f, titleHovered ? SDL_Color{255,255,255,200} : SDL_Color{255,255,255,140});
        renderer.drawRoundedRect(titleRect.x, titleRect.y, titleRect.w, titleRect.h, 12.0f, (m_focusedField == 1) ? Colors::PrimaryLight : Colors::GlassBorder);
        renderer.drawText(m_editTitle + (m_focusedField == 1 && static_cast<int>(m_elapsed * 2.0f) % 2 == 0 ? "|" : ""), titleRect.x + 16.0f, titleRect.y + 16.0f, Colors::TextPrimary, 16);

        // Content / Date / Answer Label
        std::string contentLabel = "Content";
        if (m_category == "ideas") contentLabel = "Description";
        else if (m_category == "reminders") contentLabel = "Due Date / Time";
        else if (m_category == "questions") contentLabel = "AI Answer";

        renderer.drawText(contentLabel, 340.0f, 285.0f, Colors::TextSecondary, 14);
        Rect contentRect { 340.0f, 315.0f, 420.0f, 94.0f };
        bool contentHovered = contentRect.contains(mPt.x, mPt.y) || m_focusedField == 2;
        renderer.fillRoundedRect(contentRect.x, contentRect.y, contentRect.w, contentRect.h, 12.0f, contentHovered ? SDL_Color{255,255,255,200} : SDL_Color{255,255,255,140});
        renderer.drawRoundedRect(contentRect.x, contentRect.y, contentRect.w, contentRect.h, 12.0f, (m_focusedField == 2) ? Colors::PrimaryLight : Colors::GlassBorder);
        renderer.drawText(m_editContent + (m_focusedField == 2 && static_cast<int>(m_elapsed * 2.0f) % 2 == 0 ? "|" : ""), contentRect.x + 16.0f, contentRect.y + 16.0f, Colors::TextPrimary, 16);

        // Update Button
        Rect upBtn { 340.0f, 440.0f, 195.0f, 48.0f };
        bool upBtnHovered = upBtn.contains(mPt.x, mPt.y);
        renderer.fillRoundedRect(upBtn.x, upBtn.y, upBtn.w, upBtn.h, 14.0f, upBtnHovered ? Colors::PrimaryDark : Colors::Primary);
        renderer.drawTextCentered("Update", upBtn.x + upBtn.w * 0.5f, upBtn.y + 14.0f, Colors::White, 16);

        // Delete Button
        Rect delBtn { 565.0f, 440.0f, 195.0f, 48.0f };
        bool delBtnHovered = delBtn.contains(mPt.x, mPt.y);
        renderer.fillRoundedRect(delBtn.x, delBtn.y, delBtn.w, delBtn.h, 14.0f, delBtnHovered ? SDL_Color{230, 70, 70, 255} : SDL_Color{200, 50, 50, 255});
        renderer.drawTextCentered("Delete", delBtn.x + delBtn.w * 0.5f, delBtn.y + 14.0f, Colors::White, 16);

        // --- RIGHT COLUMN ---
        renderer.drawText("Comments", 810.0f, 175.0f, Colors::TextSecondary, 14);

        // Comments Card view container
        Card commBox(Rect{ 810.0f, 205.0f, 450.0f, 250.0f }, SDL_Color { 255, 255, 255, 100 }, 16.0f);
        commBox.setBorder(Colors::GlassBorder);
        commBox.render(renderer);

        // Clipped Comments List
        renderer.setClipRect(810.0f, 210.0f, 450.0f, 240.0f);
        if (m_commentsList.empty())
        {
            renderer.drawText("No comments yet", 830.0f, 230.0f, Colors::TextSecondary, 14);
        }
        else
        {
            for (std::size_t i = 0; i < m_commentsList.size(); ++i)
            {
                float cy = 220.0f + i * 48.0f - m_scrollY;
                renderer.drawGlowCircle(832.0f, cy + 12.0f, 6.0f, SDL_Color { 124, 92, 255, 40 }, 4);
                renderer.fillCircle(832.0f, cy + 12.0f, 4.0f, Colors::Primary);
                renderer.drawText(m_commentsList[i], 854.0f, cy + 4.0f, Colors::TextPrimary, 14);
            }
        }
        renderer.clearClipRect();

        // New Comment Input box
        Rect newCommRect { 810.0f, 470.0f, 320.0f, 48.0f };
        bool newCommHovered = newCommRect.contains(mPt.x, mPt.y) || m_focusedField == 3;
        renderer.fillRoundedRect(newCommRect.x, newCommRect.y, newCommRect.w, newCommRect.h, 12.0f, newCommHovered ? SDL_Color{255,255,255,200} : SDL_Color{255,255,255,140});
        renderer.drawRoundedRect(newCommRect.x, newCommRect.y, newCommRect.w, newCommRect.h, 12.0f, (m_focusedField == 3) ? Colors::PrimaryLight : Colors::GlassBorder);
        if (m_editComment.empty() && m_focusedField != 3)
        {
            renderer.drawText("Write a comment...", newCommRect.x + 16.0f, newCommRect.y + 13.0f, SDL_Color { 140, 140, 145, 255 }, 14);
        }
        else
        {
            renderer.drawText(m_editComment + (m_focusedField == 3 && static_cast<int>(m_elapsed * 2.0f) % 2 == 0 ? "|" : ""), newCommRect.x + 16.0f, newCommRect.y + 13.0f, Colors::TextPrimary, 14);
        }

        // Add Comment Button
        Rect addCommBtn { 1150.0f, 470.0f, 110.0f, 48.0f };
        bool addCommHovered = addCommBtn.contains(mPt.x, mPt.y);
        renderer.fillRoundedRect(addCommBtn.x, addCommBtn.y, addCommBtn.w, addCommBtn.h, 12.0f, addCommHovered ? Colors::PrimaryDark : Colors::Primary);
        renderer.drawTextCentered("Add", addCommBtn.x + addCommBtn.w * 0.5f, addCommBtn.y + 13.0f, Colors::White, 15);

        // --- VIRTUAL KEYBOARD PANEL ---
        if (m_keyboardAnim > 0.0f)
        {
            const float kbdX = 300.0f;
            const float kbdY = 900.0f - m_keyboardAnim * 330.0f;
            const float kbdW = 1000.0f;
            const float kbdH = 330.0f;

            Card kbdPanel(Rect{ kbdX, kbdY, kbdW, kbdH }, SDL_Color { 255, 255, 255, 225 }, 28.0f);
            kbdPanel.setShadow(Colors::Shadow, 12);
            kbdPanel.setBorder(Colors::PrimaryLight);
            kbdPanel.render(renderer);

            // Draw Suggestion Bar above the QWERTY keys
            renderer.drawLine(kbdX, kbdY + 44.0f, kbdX + kbdW, kbdY + 44.0f, Colors::GlassBorder);
            std::string activeTextForSuggestions = "";
            if (m_focusedField == 1) activeTextForSuggestions = m_editTitle;
            else if (m_focusedField == 2) activeTextForSuggestions = m_editContent;
            else if (m_focusedField == 3) activeTextForSuggestions = m_editComment;
            
            auto suggestions = getWordSuggestions(activeTextForSuggestions);
            for (std::size_t i = 0; i < suggestions.size(); ++i)
            {
                float sugX = kbdX + 50.0f + i * 315.0f;
                Rect sugRect{ sugX, kbdY + 6.0f, 270.0f, 32.0f };
                const bool sugHovered = sugRect.contains(mPt.x, mPt.y);
                
                renderer.fillRoundedRect(sugRect.x, sugRect.y, sugRect.w, sugRect.h, 16.0f, 
                    sugHovered ? SDL_Color{ 235, 230, 250, 255 } : SDL_Color{ 245, 243, 248, 200 });
                renderer.drawRoundedRect(sugRect.x, sugRect.y, sugRect.w, sugRect.h, 16.0f, Colors::GlassBorder);
                
                renderer.drawTextCentered(suggestions[i], sugRect.x + sugRect.w * 0.5f, sugRect.y + 6.0f, Colors::Primary, 13);
            }

            auto keys = getKeyboardKeys(kbdX, kbdY, m_keyboardMode);
            for (const auto& key : keys)
            {
                const bool keyHovered = Rect{ key.x, key.y, key.w, key.h }.contains(mPt.x, mPt.y);
                
                SDL_Color keyFill = keyHovered ? SDL_Color { 235, 230, 250, 255 } : SDL_Color { 248, 248, 250, 200 };
                SDL_Color keyBorder = keyHovered ? Colors::Primary : Colors::GlassBorder;
                
                std::string act = key.action;
                if (act == "space" || act == "backspace" || act == "close" || act == "shift" || act == "mode_sym" || act == "mode_abc" || act == "mode_extra")
                {
                    keyFill = keyHovered ? SDL_Color { 210, 195, 255, 255 } : SDL_Color { 230, 226, 240, 255 };
                }

                if (act == "shift" && m_keyboardShift)
                {
                    keyFill = Colors::Primary;
                }

                renderer.fillRoundedRect(key.x, key.y, key.w, key.h, 14.0f, keyFill);
                renderer.drawRoundedRect(key.x, key.y, key.w, key.h, 14.0f, keyBorder);

                SDL_Color textColor = (keyHovered && (act == "space" || act == "backspace" || act == "close")) ? Colors::White : Colors::TextPrimary;
                if (act == "shift" && m_keyboardShift) textColor = Colors::White;

                // Adjust label case based on shift
                std::string keyLabel = key.label;
                if (m_keyboardMode == 0 && !m_keyboardShift && keyLabel.size() == 1 && std::isalpha(keyLabel[0]))
                {
                    keyLabel[0] = std::tolower(keyLabel[0]);
                }
                renderer.drawTextCentered(keyLabel, key.x + key.w * 0.5f, key.y + key.h * 0.32f, textColor, 15);
            }
        }
    }

    void DetailScreen::loadItem(Application& app)
    {
        if (m_category == "ideas")
        {
            auto list = app.services().storage->loadAllIdeas();
            for (const auto& item : list)
            {
                if (item.id == m_itemId)
                {
                    m_editTitle = item.title;
                    m_editContent = item.content;
                    m_commentsList = splitComments(item.comments);
                    break;
                }
            }
        }
        else if (m_category == "reminders")
        {
            auto list = app.services().storage->loadAllReminders();
            for (const auto& item : list)
            {
                if (item.id == m_itemId)
                {
                    m_editTitle = item.title;
                    m_editContent = item.dateTime;
                    m_commentsList = splitComments(item.comments);
                    break;
                }
            }
        }
        else if (m_category == "questions")
        {
            auto list = app.services().storage->loadAllQuestions();
            for (const auto& item : list)
            {
                if (item.id == m_itemId)
                {
                    m_editTitle = item.text;
                    m_editContent = item.answer;
                    m_commentsList = splitComments(item.comments);
                    break;
                }
            }
        }
        else if (m_category == "others")
        {
            if (app.services().memoryService)
            {
                auto item = app.services().memoryService->getById(m_itemId);
                if (item.isValid())
                {
                    m_editTitle = item.title;
                    m_editContent = item.content;
                    m_commentsList = splitComments(item.comments);
                }
            }
        }
    }

    void DetailScreen::saveChanges(Application& app)
    {
        app.audio().playSoftConfirm();
        if (m_category == "ideas")
        {
            Idea item;
            item.id = m_itemId;
            item.title = m_editTitle;
            item.content = m_editContent;
            item.comments = joinComments(m_commentsList);
            item.timestamp = "Just now"; // Default timestamp update
            app.services().storage->saveIdea(item);
        }
        else if (m_category == "reminders")
        {
            Reminder item;
            item.id = m_itemId;
            item.title = m_editTitle;
            item.dateTime = m_editContent;
            item.comments = joinComments(m_commentsList);
            app.services().storage->saveReminder(item);
        }
        else if (m_category == "questions")
        {
            Question item;
            item.id = m_itemId;
            item.text = m_editTitle;
            item.answer = m_editContent;
            item.answered = !m_editContent.empty();
            item.comments = joinComments(m_commentsList);
            item.timestamp = "Just now";
            app.services().storage->saveQuestion(item);
        }
        else if (m_category == "others")
        {
            if (app.services().memoryService)
            {
                Memory item = app.services().memoryService->getById(m_itemId);
                item.title = m_editTitle;
                item.content = m_editContent;
                item.comments = joinComments(m_commentsList);
                if (app.services().time)
                {
                    item.updatedAt = app.services().time->getFormattedDateTime();
                }
                else
                {
                    item.updatedAt = "Just now";
                }
                item.timestamp = "Just now";
                app.services().memoryService->update(item);
            }
        }

        // Navigate back
        if (m_category == "ideas") app.navigateTo(ScreenId::Ideas);
        else if (m_category == "reminders") app.navigateTo(ScreenId::Reminders);
        else if (m_category == "questions") app.navigateTo(ScreenId::Questions);
        else app.navigateTo(ScreenId::Others);
    }

    void DetailScreen::deleteItem(Application& app)
    {
        app.audio().playSoftConfirm();
        if (m_category == "ideas")
        {
            app.services().storage->deleteIdea(m_itemId);
        }
        else if (m_category == "reminders")
        {
            app.services().storage->deleteReminder(m_itemId);
        }
        else if (m_category == "questions")
        {
            app.services().storage->deleteQuestion(m_itemId);
        }
        else if (m_category == "others")
        {
            if (app.services().memoryService)
            {
                app.services().memoryService->remove(m_itemId);
            }
        }

        // Navigate back
        if (m_category == "ideas") app.navigateTo(ScreenId::Ideas);
        else if (m_category == "reminders") app.navigateTo(ScreenId::Reminders);
        else if (m_category == "questions") app.navigateTo(ScreenId::Questions);
        else app.navigateTo(ScreenId::Others);
    }

    void DetailScreen::addComment(Application& app)
    {
        if (m_editComment.empty()) return;
        
        app.audio().playSoftConfirm();
        m_commentsList.push_back(m_editComment);
        m_editComment = "";

        // Save immediately
        if (m_category == "ideas")
        {
            Idea item;
            item.id = m_itemId;
            // Need to retrieve remaining fields so we don't overwrite with empty
            auto list = app.services().storage->loadAllIdeas();
            for (const auto& it : list) {
                if (it.id == m_itemId) { item = it; break; }
            }
            item.comments = joinComments(m_commentsList);
            app.services().storage->saveIdea(item);
        }
        else if (m_category == "reminders")
        {
            Reminder item;
            item.id = m_itemId;
            auto list = app.services().storage->loadAllReminders();
            for (const auto& it : list) {
                if (it.id == m_itemId) { item = it; break; }
            }
            item.comments = joinComments(m_commentsList);
            app.services().storage->saveReminder(item);
        }
        else if (m_category == "questions")
        {
            Question item;
            item.id = m_itemId;
            auto list = app.services().storage->loadAllQuestions();
            for (const auto& it : list) {
                if (it.id == m_itemId) { item = it; break; }
            }
            item.comments = joinComments(m_commentsList);
            app.services().storage->saveQuestion(item);
        }
        else if (m_category == "others")
        {
            if (app.services().memoryService)
            {
                Memory item = app.services().memoryService->getById(m_itemId);
                item.comments = joinComments(m_commentsList);
                app.services().memoryService->update(item);
            }
        }

        m_targetScrollY = 99999.0f; // Scroll comments to the bottom
    }

    std::vector<std::string> DetailScreen::splitComments(const std::string& str)
    {
        if (str.empty()) return {};
        std::vector<std::string> list;
        std::string::size_type start = 0;
        std::string::size_type pos;
        while ((pos = str.find('|', start)) != std::string::npos)
        {
            list.push_back(str.substr(start, pos - start));
            start = pos + 1;
        }
        list.push_back(str.substr(start));

        std::vector<std::string> filtered;
        for (const auto& s : list)
        {
            if (!s.empty()) filtered.push_back(s);
        }
        return filtered;
    }

    std::string DetailScreen::joinComments(const std::vector<std::string>& list)
    {
        std::string str;
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            if (i > 0) str += "|";
            str += list[i];
        }
        return str;
    }
}
