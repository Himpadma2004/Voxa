#include "RecordScreen.h"

#include <cmath>

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Card.h"
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
            const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
            // Header back button hit area centered at (18, 28) with radius 11
            if (Rect { 5.0f, 15.0f, 26.0f, 26.0f }.contains(point.x, point.y))
            {
                app.navigateTo(ScreenId::Home);
                return;
            }

            // Tapping elsewhere stops capture
            if (point.y > 45.0f)
            {
                app.audio().playSoftConfirm();
                app.navigateTo(ScreenId::Home);
            }
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

        const float cx = 160.0f;
        const float cy = 105.0f;
        const float pulse = std::sin(m_elapsed * 3.5f) * 4.0f;
        
        // Pulsating glow and outer circle
        renderer.drawGlowCircle(cx, cy, 38.0f + pulse, SDL_Color { 124, 92, 255, 14 }, 6);
        renderer.fillCircleGradient(cx, cy, 30.0f + pulse * 0.15f, SDL_Color { 255, 255, 255, 180 }, SDL_Color { 244, 238, 250, 255 });
        renderer.drawCircle(cx, cy, 30.0f + pulse * 0.15f, Colors::Primary);

        // Pulse wave lines inside microphone circle
        for (int i = 0; i < 7; ++i)
        {
            const float lx = cx - 21.0f + i * 7.0f;
            const float amplitude = 6.0f + std::sin(m_elapsed * 4.2f + static_cast<float>(i) * 0.6f) * 14.0f;
            renderer.drawLine(lx, cy - amplitude * 0.5f, lx, cy + amplitude * 0.5f, Colors::Primary);
        }

        // Timer readout
        const int totalSeconds = 12 + static_cast<int>(m_elapsed) % 48;
        const int displaySeconds = totalSeconds % 60;
        const int displayMinutes = totalSeconds / 60;
        char timer[16];
        SDL_snprintf(timer, sizeof(timer), "%02d:%02d", displayMinutes, displaySeconds);

        renderer.drawTextCentered(timer, cx, 148.0f, Colors::TextPrimary, 22);
        renderer.drawTextCentered("Recording...", cx, 172.0f, Colors::TextPrimary, 11);
        renderer.drawTextCentered("Tap anywhere to save", cx, 186.0f, Colors::TextSecondary, 9);

        // Minimal live waveform
        for (int i = 0; i < 20; ++i)
        {
            const float wx = cx - 70.0f + i * 7.0f;
            const float amplitude = 4.0f + std::sin(m_elapsed * 5.0f + static_cast<float>(i) * 0.4f) * 12.0f;
            renderer.drawLine(wx, 212.0f - amplitude * 0.5f, wx, 212.0f + amplitude * 0.5f, SDL_Color { 166, 123, 250, 180 });
        }
    }
}
