#include "Application.h"

#include <algorithm>

#include "../screens/BootScreen.h"
#include "../screens/HomeScreen.h"
#include "../screens/IdeasScreen.h"
#include "../screens/OthersScreen.h"
#include "../screens/QuestionsScreen.h"
#include "../screens/RecordScreen.h"
#include "../screens/ReminderScreen.h"
#include "../screens/SearchScreen.h"
#include "../screens/SettingsScreen.h"
#include "../screens/SyncStatusScreen.h"
#include "../screens/DetailScreen.h"

namespace VOXA
{
    void Screen::onEnter(Application&)
    {
    }

    void Screen::onExit(Application&)
    {
    }

    void Screen::handleEvent(Application&, const SDL_Event&)
    {
    }

    Application::Application() = default;

    Application::~Application()
    {
        shutdown();
    }

    bool Application::initialize()
    {
        if (!m_voxaApp.initialize())
        {
            return false;
        }

        if (!m_renderer.initialize("VOXA Simulator", 640, 480, 320, 240))
        {
            return false;
        }

        m_audio.initialize();

        registerScreens();
        m_currentScreenId = ScreenId::Boot;
        m_currentScreen = m_screens.at(m_currentScreenId).get();
        m_currentScreen->onEnter(*this);
        m_lastTickMs = SDL_GetTicks();
        return true;
    }

    void Application::registerScreens()
    {
        m_screens.emplace(ScreenId::Boot, std::make_unique<BootScreen>());
        m_screens.emplace(ScreenId::Home, std::make_unique<HomeScreen>());
        m_screens.emplace(ScreenId::Record, std::make_unique<RecordScreen>());
        m_screens.emplace(ScreenId::Search, std::make_unique<SearchScreen>());
        m_screens.emplace(ScreenId::Reminders, std::make_unique<ReminderScreen>());
        m_screens.emplace(ScreenId::Questions, std::make_unique<QuestionsScreen>());
        m_screens.emplace(ScreenId::Ideas, std::make_unique<IdeasScreen>());
        m_screens.emplace(ScreenId::Others, std::make_unique<OthersScreen>());
        m_screens.emplace(ScreenId::Settings, std::make_unique<SettingsScreen>());
        m_screens.emplace(ScreenId::SyncStatus, std::make_unique<SyncStatusScreen>());
        m_screens.emplace(ScreenId::Detail, std::make_unique<DetailScreen>());
    }

    void Application::run()
    {
        while (isRunning())
        {
            SDL_Event event {};
            while (SDL_PollEvent(&event))
            {
                dispatchEvent(event);
            }

            const Uint64 nowMs = SDL_GetTicks();
            const float deltaSeconds = std::clamp(static_cast<float>(nowMs - m_lastTickMs) / 1000.0f, 0.0f, 0.05f);
            m_lastTickMs = nowMs;

            commitPendingNavigation();

            // Update screens and transition state
            if (m_inTransition)
            {
                m_transitionElapsed += deltaSeconds;
                if (m_transitionElapsed >= m_transitionDuration)
                {
                    m_inTransition = false;
                    if (m_prevScreen != nullptr)
                    {
                        m_prevScreen->onExit(*this);
                        m_prevScreen = nullptr;
                    }
                }
            }

            if (m_prevScreen != nullptr)
            {
                m_prevScreen->update(*this, deltaSeconds);
            }
            m_currentScreen->update(*this, deltaSeconds);

            m_renderer.beginFrame();
            
            if (m_inTransition && m_prevScreen != nullptr)
            {
                float t = m_transitionElapsed / m_transitionDuration;
                if (t > 1.0f) t = 1.0f;
                // Ease out cubic curves for phone-like physics
                t = 1.0f - std::pow(1.0f - t, 3.0f);

                const float width = static_cast<float>(m_renderer.canvasWidth());
                const float offsetPrev = m_transitionForward ? (-t * width) : (t * width);
                const float offsetCurr = m_transitionForward ? ((1.0f - t) * width) : (-(1.0f - t) * width);

                // Draw previous screen translated
                m_renderer.setLogicalOffset(offsetPrev, 0.0f);
                m_prevScreen->render(*this, m_renderer);

                // Draw current screen translated
                m_renderer.setLogicalOffset(offsetCurr, 0.0f);
                m_currentScreen->render(*this, m_renderer);

                m_renderer.resetLogicalOffset();
            }
            else
            {
                m_currentScreen->render(*this, m_renderer);
            }

            m_renderer.endFrame();

            commitPendingNavigation();
            SDL_Delay(16);
        }
    }

    void Application::shutdown()
    {
        m_voxaApp.shutdown();

        if (m_currentScreen != nullptr)
        {
            m_currentScreen->onExit(*this);
            m_currentScreen = nullptr;
        }

        m_screens.clear();
        m_audio.shutdown();
        m_renderer.shutdown();
    }

    void Application::navigateTo(ScreenId screenId)
    {
        m_pendingScreenId = screenId;
        m_hasPendingNavigation = true;
    }

    void Application::requestQuit()
    {
        m_quitRequested = true;
    }

    bool Application::isRunning() const
    {
        return m_renderer.isRunning() && !m_quitRequested;
    }

    bool Application::quitRequested() const
    {
        return m_quitRequested;
    }

    float Application::timeSeconds() const
    {
        return static_cast<float>(SDL_GetTicks()) / 1000.0f;
    }

    Renderer& Application::renderer()
    {
        return m_renderer;
    }

    const Renderer& Application::renderer() const
    {
        return m_renderer;
    }

    AudioEngine& Application::audio()
    {
        return m_audio;
    }

    SDL_FPoint Application::windowToCanvas(float windowX, float windowY) const
    {
        return m_renderer.windowToCanvas(windowX, windowY);
    }

    void Application::dispatchEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            requestQuit();
            return;
        }

        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            switch (event.key.key)
            {
            case SDLK_ESCAPE:
                navigateTo(ScreenId::Home);
                return;
            case SDLK_1:
                navigateTo(ScreenId::Home);
                return;
            case SDLK_2:
                navigateTo(ScreenId::Record);
                return;
            case SDLK_3:
                navigateTo(ScreenId::Search);
                return;
            case SDLK_4:
                navigateTo(ScreenId::Reminders);
                return;
            case SDLK_5:
                navigateTo(ScreenId::Questions);
                return;
            case SDLK_6:
                navigateTo(ScreenId::Ideas);
                return;
            case SDLK_7:
                navigateTo(ScreenId::Others);
                return;
            case SDLK_8:
                navigateTo(ScreenId::Settings);
                return;
            default:
                break;
            }
        }

        if (m_currentScreen != nullptr)
        {
            m_currentScreen->handleEvent(*this, event);
        }
    }

    void Application::commitPendingNavigation()
    {
        if (!m_hasPendingNavigation)
        {
            return;
        }

        m_hasPendingNavigation = false;

        if (m_pendingScreenId == m_currentScreenId)
        {
            return;
        }

        // If we are currently transitioning, skip intermediate transitions
        if (m_inTransition)
        {
            if (m_prevScreen != nullptr)
            {
                m_prevScreen->onExit(*this);
            }
            m_prevScreen = nullptr;
            m_inTransition = false;
        }

        // Home screen is the primary screen. Navigation to it is backward (slide right).
        // Navigation from it is forward (slide left).
        m_transitionForward = (m_pendingScreenId != ScreenId::Home);

        // Special exceptions: Settings to SyncStatus is forward, SyncStatus to Settings is backward.
        if (m_currentScreenId == ScreenId::Settings && m_pendingScreenId == ScreenId::SyncStatus)
        {
            m_transitionForward = true;
        }
        else if (m_currentScreenId == ScreenId::SyncStatus && m_pendingScreenId == ScreenId::Settings)
        {
            m_transitionForward = false;
        }

        m_prevScreen = m_currentScreen;
        m_inTransition = true;
        m_transitionElapsed = 0.0f;

        m_currentScreenId = m_pendingScreenId;
        m_currentScreen = m_screens.at(m_currentScreenId).get();
        m_currentScreen->onEnter(*this);
    }

    Services& Application::services()
    {
        return m_voxaApp.services();
    }

    const Services& Application::services() const
    {
        return m_voxaApp.services();
    }
}
