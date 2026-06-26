#pragma once

#include "../core/Screen.h"

namespace VOXA
{
    class QuestionsScreen : public Screen
    {
    public:
        ScreenId id() const override;
        void handleEvent(Application& app, const SDL_Event& event) override;
        void update(Application& app, float deltaSeconds) override;
        void render(Application& app, Renderer& renderer) override;

    private:
        float m_scrollY { 0.0f };
        float m_targetScrollY { 0.0f };
        bool m_isDragging { false };
        float m_dragStartY { 0.0f };
        float m_dragStartScrollY { 0.0f };
    };
}
