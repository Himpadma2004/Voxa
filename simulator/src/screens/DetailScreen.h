#pragma once

#include "../core/Screen.h"
#include <string>
#include <vector>

namespace VOXA
{
    class DetailScreen : public Screen
    {
    public:
        ScreenId id() const override;

        void onEnter(Application& app) override;
        void handleEvent(Application& app, const SDL_Event& event) override;
        void update(Application& app, float deltaSeconds) override;
        void render(Application& app, Renderer& renderer) override;

    private:
        float m_elapsed { 0.0f };

        // Item identifiers
        std::string m_category;
        uint32_t    m_itemId { 0 };

        // Editing buffers
        std::string m_editTitle;
        std::string m_editContent;
        std::string m_editComment;
        std::vector<std::string> m_commentsList;

        // Focus and Virtual Keyboard
        int  m_focusedField { 0 }; // 0: None, 1: Title, 2: Content/Answer/Date, 3: New Comment
        bool m_keyboardOpen { false };
        float m_keyboardAnim { 0.0f };
        bool m_keyboardShift { false };

        // Scrolling comments list inside detail screen
        float m_scrollY { 0.0f };
        float m_targetScrollY { 0.0f };
        bool  m_isDragging { false };
        float m_dragStartY { 0.0f };
        float m_dragStartScrollY { 0.0f };

        // Helpers
        void saveChanges(Application& app);
        void deleteItem(Application& app);
        void addComment(Application& app);
        void loadItem(Application& app);
        std::vector<std::string> splitComments(const std::string& str);
        std::string joinComments(const std::vector<std::string>& list);
    };
}
