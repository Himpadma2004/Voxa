#include "RecordScreen.h"

#include <cmath>

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "ScreenCommon.h"

namespace VOXA
{
    ScreenId RecordScreen::id() const
    {
        return ScreenId::Record;
    }

    void RecordScreen::onEnter(Application&)
    {
        m_elapsed = 0.0f;
    }

    void RecordScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            app.navigateTo(ScreenId::Home);
        }
    }

    void RecordScreen::update(Application&, float deltaSeconds)
    {
        m_elapsed += deltaSeconds;
    }

    void RecordScreen::render(Application&, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Voice Capture", true, true, Icon::Spark);

        const float pulse = std::sin(m_elapsed * 3.5f) * 10.0f;
        renderer.drawGlowCircle(520.0f, 430.0f, 176.0f + pulse, SDL_Color { 124, 92, 255, 18 }, 12);
        renderer.fillCircleGradient(520.0f, 430.0f, 148.0f + pulse * 0.15f, SDL_Color { 255, 255, 255, 255 }, SDL_Color { 244, 238, 250, 255 });
        renderer.drawCircle(520.0f, 430.0f, 148.0f + pulse * 0.15f, Colors::Primary);
        renderer.drawCircle(520.0f, 430.0f, 170.0f + pulse * 0.1f, SDL_Color { 170, 148, 255, 120 });

        for (int i = 0; i < 13; ++i)
        {
            const float x = 420.0f + i * 16.0f;
            const float amplitude = 22.0f + std::sin(m_elapsed * 4.2f + static_cast<float>(i) * 0.6f) * 46.0f;
            renderer.drawLine(x, 430.0f - amplitude * 0.5f, x, 430.0f + amplitude * 0.5f, Colors::Primary);
        }

        const int totalSeconds = 12 + static_cast<int>(m_elapsed) % 48;
        const int displaySeconds = totalSeconds % 60;
        const int displayMinutes = totalSeconds / 60;
        char timer[16];
        SDL_snprintf(timer, sizeof(timer), "%02d:%02d", displayMinutes, displaySeconds);

        renderer.drawText(timer, 920.0f, 286.0f, Colors::TextPrimary, 78);
        renderer.drawText("Recording...", 922.0f, 386.0f, Colors::TextPrimary, 30);
        renderer.drawText("Tap anywhere to stop and save the voice memory", 922.0f, 436.0f, Colors::TextSecondary, 20);
        renderer.drawText("Live waveform", 922.0f, 546.0f, Colors::TextPrimary, 22);

        for (int i = 0; i < 24; ++i)
        {
            const float x = 920.0f + i * 22.0f;
            const float amplitude = 14.0f + std::sin(m_elapsed * 5.0f + static_cast<float>(i) * 0.6f) * 48.0f;
            renderer.drawLine(x, 638.0f - amplitude * 0.5f, x, 638.0f + amplitude * 0.5f, SDL_Color { 167, 143, 255, 180 });
        }
    }
}
