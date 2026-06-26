#pragma once

#include <string>
#include "../core/Screen.h"

namespace VOXA
{
    class SearchScreen : public Screen
    {
    public:
        ScreenId id() const override;
        void onEnter(Application& app) override;
        void handleEvent(Application& app, const SDL_Event& event) override;
        void update(Application& app, float deltaSeconds) override;
        void render(Application& app, Renderer& renderer) override;

    private:
        float m_elapsed { 0.0f };
        bool m_keyboardOpen { false };
        float m_keyboardAnim { 0.0f };
        bool m_keyboardShift { false };
        int  m_keyboardMode { 0 }; // 0: Alphabet, 1: Numbers/Basic, 2: Extra Symbols
        std::string m_searchQuery;
        bool m_voiceSearchOpen { false };
        float m_voiceSearchAnim { 0.0f };
        float m_voiceElapsed { 0.0f };

        float m_scrollY { 0.0f };
        float m_targetScrollY { 0.0f };
        bool m_isDragging { false };
        float m_dragStartY { 0.0f };
        float m_dragStartScrollY { 0.0f };
    };
}
