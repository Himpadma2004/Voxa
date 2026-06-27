#pragma once

#include <SDL3/SDL.h>
#include "../core/Screen.h"

namespace VOXA
{
    class HomeScreen : public Screen
    {
    public:
        ScreenId id() const override;
        void onEnter(Application& app) override;
        void handleEvent(Application& app, const SDL_Event& event) override;
        void update(Application& app, float deltaSeconds) override;
        void render(Application& app, Renderer& renderer) override;

    private:
        void renderPage0(Application& app, Renderer& renderer);
        void renderPage1(Application& app, Renderer& renderer);

        float m_elapsed { 0.0f };
        static constexpr int kNumPages = 2;
        int m_page { 0 };               // 0 = Assistant Home, 1 = Menu list

        // Swipe & transition page state (horizontal)
        bool  m_isDragging { false };
        float m_dragStartX { 0.0f };
        float m_dragStartY { 0.0f };
        float m_swipeOffset { 0.0f };

        // Menu scroll state (vertical)
        bool  m_isScrollDragging { false };
        float m_menuScrollY { 0.0f };
        float m_menuTargetScrollY { 0.0f };
    };
}
