#pragma once

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
    };
}
