// DetailScreen.cpp — smartwatch detail editor for Waveshare 2.8" 320x240 display
#include "DetailScreen.h"

#include <array>
#include <cmath>
#include <algorithm>
#include <cctype>

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
            keys.push_back({"Done", startX + 240.0f, startY + 96.0f, 65.0f, 20.0f, "close"});
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
            keys.push_back({"Done", startX + 240.0f, startY + 96.0f, 65.0f, 20.0f, "close"});
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
            keys.push_back({"Done", startX + 240.0f, startY + 96.0f, 65.0f, 20.0f, "close"});
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
        m_category = app.getSelectedItem().category;
        m_itemId = app.getSelectedItem().id;
        m_focusedField = 0;
        m_keyboardOpen = false;
        m_keyboardAnim = 0.0f;
        m_keyboardShift = false;
        m_keyboardMode = 0;
        m_scrollY = 0.0f;
        m_targetScrollY = 0.0f;
        m_isDragging = false;

        m_editTitle = "";
        m_editContent = "";
        m_editComment = "";
        m_commentsList.clear();

        loadItem(app);
    }

    void DetailScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        // 1. Handle physical keyboard inputs
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
                    if (event.key.mod & SDL_KMOD_SHIFT) c = std::toupper(c);
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

        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            float mx = 0.0f, my = 0.0f;
            SDL_GetMouseState(&mx, &my);
            const SDL_FPoint mPt = app.windowToCanvas(mx, my);
            if (Rect { 10.0f, 48.0f, 300.0f, 182.0f }.contains(mPt.x, mPt.y))
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
            m_isDragging = false;
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
            app.audio().playSoftConfirm();
            app.navigateBack();
            return;
        }

        // Tap inside main area to drag-scroll or focus fields
        if (Rect { 10.0f, 48.0f, 300.0f, 182.0f }.contains(point.x, point.y))
        {
            // Title Box click detection (adjust with scroll)
            if (Rect { 10.0f, 64.0f - m_scrollY, 300.0f, 30.0f }.contains(point.x, point.y))
            {
                m_focusedField = 1;
                m_keyboardOpen = true;
                m_targetScrollY = 0.0f;
                return;
            }
            // Content Box click detection
            if (Rect { 10.0f, 114.0f - m_scrollY, 300.0f, 44.0f }.contains(point.x, point.y))
            {
                m_focusedField = 2;
                m_keyboardOpen = true;
                m_targetScrollY = 20.0f;
                return;
            }
            // Update Button click detection
            if (Rect { 10.0f, 168.0f - m_scrollY, 145.0f, 28.0f }.contains(point.x, point.y))
            {
                saveChanges(app);
                return;
            }
            // Delete Button click detection
            if (Rect { 165.0f, 168.0f - m_scrollY, 145.0f, 28.0f }.contains(point.x, point.y))
            {
                deleteItem(app);
                return;
            }

            // Calculate comments start Y exactly like in render
            float commStartY = 222.0f - m_scrollY;
            if (m_commentsList.empty())
            {
                commStartY += 24.0f;
            }
            else
            {
                commStartY += static_cast<float>(m_commentsList.size()) * 32.0f;
            }
            float commentInputY = commStartY + 10.0f;

            // New Comment input Box click detection
            if (Rect { 10.0f, commentInputY, 230.0f, 30.0f }.contains(point.x, point.y))
            {
                m_focusedField = 3;
                m_keyboardOpen = true;
                m_targetScrollY = commentInputY - 60.0f;
                return;
            }

            // Add Comment Button click detection
            if (Rect { 250.0f, commentInputY, 60.0f, 30.0f }.contains(point.x, point.y))
            {
                addComment(app);
                return;
            }

            // If we tapped outside input boxes/buttons but inside list area, start dragging
            m_isDragging = true;
            m_dragStartY = point.y;
            m_dragStartScrollY = m_targetScrollY;
        }

        // Virtual Keyboard Keys Click detection
        if (m_keyboardOpen && m_keyboardAnim > 0.8f)
        {
            const float kbdY = 240.0f - m_keyboardAnim * 135.0f;

            std::string* activeStr = nullptr;
            if (m_focusedField == 1) activeStr = &m_editTitle;
            else if (m_focusedField == 2) activeStr = &m_editContent;
            else if (m_focusedField == 3) activeStr = &m_editComment;

            // Check suggestion bar click
            if (point.y >= kbdY && point.y <= kbdY + 20.0f && point.x >= 5.0f && point.x <= 315.0f)
            {
                if (activeStr)
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
            if (Rect{ 5.0f, kbdY, 310.0f, 135.0f }.contains(point.x, point.y))
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

        // Calculate total content height
        float contentHeight = 202.0f + static_cast<float>(m_commentsList.size()) * 32.0f + 60.0f;
        float visibleHeight = 182.0f;
        if (m_keyboardOpen)
        {
            visibleHeight = 105.0f; // height is reduced when keyboard covers bottom of screen
        }
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

        // Content Y limit
        float visibleHeight = m_keyboardOpen ? 105.0f : 182.0f;
        renderer.setClipRect(5.0f, 48.0f, 310.0f, visibleHeight);

        // --- 1. Title Input Box ---
        Rect titleLabelRect { 12.0f, 52.0f - m_scrollY, 200.0f, 15.0f };
        renderer.drawText("Title", 12.0f, 50.0f - m_scrollY, Colors::TextSecondary, 10);
        
        Rect titleRect { 10.0f, 64.0f - m_scrollY, 300.0f, 30.0f };
        bool titleHovered = titleRect.contains(mPt.x, mPt.y) || m_focusedField == 1;
        renderer.fillRoundedRect(titleRect.x, titleRect.y, titleRect.w, titleRect.h, 8.0f, SDL_Color { 255, 255, 255, 255 });
        renderer.drawRoundedRect(titleRect.x, titleRect.y, titleRect.w, titleRect.h, 8.0f, (m_focusedField == 1) ? Colors::Primary : SDL_Color { 230, 230, 235, 255 });
        
        std::string displayTitle = m_editTitle;
        if (m_focusedField == 1 && static_cast<int>(m_elapsed * 2.0f) % 2 == 0) displayTitle += "|";
        renderer.drawText(displayTitle, titleRect.x + 10.0f, titleRect.y + 8.0f, Colors::TextPrimary, 11);

        // --- 2. Content Input Box ---
        std::string contentLabel = "Content";
        if (m_category == "ideas") contentLabel = "Description";
        else if (m_category == "reminders") contentLabel = "Due Date / Time";
        else if (m_category == "questions") contentLabel = "AI Answer";

        renderer.drawText(contentLabel, 12.0f, 100.0f - m_scrollY, Colors::TextSecondary, 10);
        
        Rect contentRect { 10.0f, 114.0f - m_scrollY, 300.0f, 44.0f };
        bool contentHovered = contentRect.contains(mPt.x, mPt.y) || m_focusedField == 2;
        renderer.fillRoundedRect(contentRect.x, contentRect.y, contentRect.w, contentRect.h, 8.0f, SDL_Color { 255, 255, 255, 255 });
        renderer.drawRoundedRect(contentRect.x, contentRect.y, contentRect.w, contentRect.h, 8.0f, (m_focusedField == 2) ? Colors::Primary : SDL_Color { 230, 230, 235, 255 });
        
        std::string displayContent = m_editContent;
        if (m_focusedField == 2 && static_cast<int>(m_elapsed * 2.0f) % 2 == 0) displayContent += "|";
        renderer.drawText(displayContent, contentRect.x + 10.0f, contentRect.y + 8.0f, Colors::TextPrimary, 11);

        // --- 3. Update & Delete Buttons ---
        Rect upBtn { 10.0f, 168.0f - m_scrollY, 145.0f, 28.0f };
        bool upBtnHovered = upBtn.contains(mPt.x, mPt.y);
        renderer.fillRoundedRect(upBtn.x, upBtn.y, upBtn.w, upBtn.h, 8.0f, upBtnHovered ? Colors::PrimaryDark : Colors::Primary);
        renderer.drawTextCentered("Update", upBtn.x + upBtn.w * 0.5f, upBtn.y + 8.0f, Colors::White, 11);

        Rect delBtn { 165.0f, 168.0f - m_scrollY, 145.0f, 28.0f };
        bool delBtnHovered = delBtn.contains(mPt.x, mPt.y);
        renderer.fillRoundedRect(delBtn.x, delBtn.y, delBtn.w, delBtn.h, 8.0f, delBtnHovered ? SDL_Color{220, 60, 60, 255} : SDL_Color{200, 50, 50, 255});
        renderer.drawTextCentered("Delete", delBtn.x + delBtn.w * 0.5f, delBtn.y + 8.0f, Colors::White, 11);

        // --- 4. Comments Header ---
        float commentsHeaderY = 206.0f - m_scrollY;
        renderer.drawText("Comments", 12.0f, commentsHeaderY, Colors::TextSecondary, 10);

        // --- 5. Comments List ---
        float commentsStartY = 222.0f - m_scrollY;
        if (m_commentsList.empty())
        {
            renderer.drawText("No comments yet", 12.0f, commentsStartY + 4.0f, Colors::TextDisabled, 11);
            commentsStartY += 24.0f;
        }
        else
        {
            for (std::size_t i = 0; i < m_commentsList.size(); ++i)
            {
                float cy = commentsStartY + i * 32.0f;
                // Purple bullet
                renderer.fillCircle(18.0f, cy + 12.0f, 3.5f, Colors::Primary);
                // Comment text
                renderer.drawText(m_commentsList[i], 32.0f, cy + 6.0f, Colors::TextPrimary, 11);
            }
            commentsStartY += static_cast<float>(m_commentsList.size()) * 32.0f;
        }

        // --- 6. New Comment Box and Button ---
        float commentInputY = commentsStartY + 10.0f;
        Rect newCommRect { 10.0f, commentInputY, 230.0f, 30.0f };
        bool newCommHovered = newCommRect.contains(mPt.x, mPt.y) || m_focusedField == 3;
        renderer.fillRoundedRect(newCommRect.x, newCommRect.y, newCommRect.w, newCommRect.h, 8.0f, SDL_Color { 255, 255, 255, 255 });
        renderer.drawRoundedRect(newCommRect.x, newCommRect.y, newCommRect.w, newCommRect.h, 8.0f, (m_focusedField == 3) ? Colors::Primary : SDL_Color { 230, 230, 235, 255 });
        
        std::string displayComment = m_editComment;
        if (m_focusedField == 3 && static_cast<int>(m_elapsed * 2.0f) % 2 == 0) displayComment += "|";
        
        if (m_editComment.empty() && m_focusedField != 3)
        {
            renderer.drawText("Write a comment...", newCommRect.x + 10.0f, newCommRect.y + 8.0f, SDL_Color { 160, 160, 165, 255 }, 11);
        }
        else
        {
            renderer.drawText(displayComment, newCommRect.x + 10.0f, newCommRect.y + 8.0f, Colors::TextPrimary, 11);
        }

        Rect addCommBtn { 250.0f, commentInputY, 60.0f, 30.0f };
        bool addCommHovered = addCommBtn.contains(mPt.x, mPt.y);
        renderer.fillRoundedRect(addCommBtn.x, addCommBtn.y, addCommBtn.w, addCommBtn.h, 8.0f, addCommHovered ? Colors::PrimaryDark : Colors::Primary);
        renderer.drawTextCentered("Add", addCommBtn.x + addCommBtn.w * 0.5f, addCommBtn.y + 8.0f, Colors::White, 11);

        renderer.clearClipRect();

        // --- VIRTUAL KEYBOARD PANEL ---
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
            std::string activeTextForSuggestions = "";
            if (m_focusedField == 1) activeTextForSuggestions = m_editTitle;
            else if (m_focusedField == 2) activeTextForSuggestions = m_editContent;
            else if (m_focusedField == 3) activeTextForSuggestions = m_editComment;
            
            auto suggestions = getWordSuggestions(activeTextForSuggestions);
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

            auto keys = getKeyboardKeys(5.0f, kbdY, m_keyboardMode);
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

                renderer.fillRoundedRect(key.x, key.y, key.w, key.h, 6.0f, keyFill);
                renderer.drawRoundedRect(key.x, key.y, key.w, key.h, 6.0f, keyBorder);

                SDL_Color textColor = (keyHovered && (act == "space" || act == "backspace" || act == "close")) ? Colors::White : Colors::TextPrimary;
                if (act == "shift" && m_keyboardShift) textColor = Colors::White;

                std::string keyLabel = key.label;
                if (m_keyboardMode == 0 && !m_keyboardShift && keyLabel.size() == 1 && std::isalpha(keyLabel[0]))
                {
                    keyLabel[0] = std::tolower(keyLabel[0]);
                }
                renderer.drawTextCentered(keyLabel, key.x + key.w * 0.5f, key.y + key.h * 0.25f, textColor, 10);
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
            item.timestamp = "Just now";
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
        app.navigateBack();
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
        app.navigateBack();
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
