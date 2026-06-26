#include <memory>
#include <unordered_map>

#include <SDL3/SDL.h>

#include "Screen.h"
#include "AudioEngine.h"
#include "../graphics/Renderer.h"
#include "app/Application.h"   // VoxaApp + Services

namespace VOXA
{
    class Application
    {
    public:
        Application();
        ~Application();

        bool initialize();
        void run();
        void shutdown();

        void navigateTo(ScreenId screenId);
        void requestQuit();

        [[nodiscard]] bool isRunning() const;
        [[nodiscard]] bool quitRequested() const;
        [[nodiscard]] float timeSeconds() const;

        [[nodiscard]] Renderer& renderer();
        [[nodiscard]] const Renderer& renderer() const;
        [[nodiscard]] AudioEngine& audio();
        [[nodiscard]] SDL_FPoint windowToCanvas(float windowX, float windowY) const;

        /// Access all backend services (reminders, search, battery, time, …).
        [[nodiscard]] Services& services();
        [[nodiscard]] const Services& services() const;

    private:
        struct EnumClassHash
        {
            template <typename T>
            std::size_t operator()(T value) const
            {
                return static_cast<std::size_t>(value);
            }
        };

        void registerScreens();
        void dispatchEvent(const SDL_Event& event);
        void commitPendingNavigation();

        Renderer    m_renderer;
        AudioEngine m_audio;
        VoxaApp     m_voxaApp;   // Owns all backend services
        std::unordered_map<ScreenId, std::unique_ptr<Screen>, EnumClassHash> m_screens;
        Screen* m_currentScreen { nullptr };
        ScreenId m_currentScreenId { ScreenId::Boot };
        ScreenId m_pendingScreenId { ScreenId::Boot };
        bool m_hasPendingNavigation { false };
        bool m_quitRequested { false };
        Uint64 m_lastTickMs { 0 };

        Screen* m_prevScreen { nullptr };
        bool m_inTransition { false };
        float m_transitionElapsed { 0.0f };
        float m_transitionDuration { 0.35f };
        bool m_transitionForward { true };
    };
}
