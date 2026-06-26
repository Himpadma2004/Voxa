#pragma once

#include <SDL3/SDL.h>

namespace VOXA
{
    class Application;
    class Renderer;

    enum class ScreenId
    {
        Boot,
        Home,
        Record,
        Search,
        Reminders,
        Questions,
        Ideas,
        Others,
        Settings,
        SyncStatus,
        Detail
    };

    class Screen
    {
    public:
        virtual ~Screen() = default;

        virtual ScreenId id() const = 0;
        virtual void onEnter(Application& app);
        virtual void onExit(Application& app);
        virtual void handleEvent(Application& app, const SDL_Event& event);
        virtual void update(Application& app, float deltaSeconds) = 0;
        virtual void render(Application& app, Renderer& renderer) = 0;
    };
}
