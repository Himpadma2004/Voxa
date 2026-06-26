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
        float m_elapsed { 0.0f };
        int m_page { 0 };               // 0 = Watch Face, 1 = Option Grid
        bool m_isDragging { false };
        float m_dragStartX { 0.0f };
        float m_dragStartY { 0.0f };
    };
}
