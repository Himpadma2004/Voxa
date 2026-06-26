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
            // Back button hit area matches renderCircularButton at (72, 62) radius 28
            if (Rect { 44.0f, 34.0f, 56.0f, 56.0f }.contains(point.x, point.y))
            {
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

        // Center glass card frame
        const float cardX = 200.0f;
        const float cardY = 140.0f;
        const float cardW = 1200.0f;
        const float cardH = 620.0f;
        
        Card frame(Rect { cardX, cardY, cardW, cardH }, Colors::Card, 32.0f);
        frame.setShadow(Colors::Shadow, 8);
        frame.setBorder(Colors::GlassBorder);
        frame.render(renderer);

        // Dynamic pulsing circles (Left side)
        const float cx = cardX + 320.0f;
        const float cy = cardY + 310.0f;
        const float pulse = std::sin(m_elapsed * 3.5f) * 10.0f;
        
        renderer.drawGlowCircle(cx, cy, 150.0f + pulse, SDL_Color { 124, 92, 255, 14 }, 10);
        renderer.fillCircleGradient(cx, cy, 120.0f + pulse * 0.15f, SDL_Color { 255, 255, 255, 180 }, SDL_Color { 244, 238, 250, 255 });
        renderer.drawCircle(cx, cy, 120.0f + pulse * 0.15f, Colors::Primary);
        renderer.drawCircle(cx, cy, 140.0f + pulse * 0.1f, SDL_Color { 170, 148, 255, 80 });

        // Pulse wave lines inside mic circle
        for (int i = 0; i < 11; ++i)
        {
            const float lx = cx - 80.0f + i * 16.0f;
            const float amplitude = 18.0f + std::sin(m_elapsed * 4.2f + static_cast<float>(i) * 0.6f) * 40.0f;
            renderer.drawLine(lx, cy - amplitude * 0.5f, lx, cy + amplitude * 0.5f, Colors::Primary);
        }

        // Timer and Labels (Right side)
        const int totalSeconds = 12 + static_cast<int>(m_elapsed) % 48;
        const int displaySeconds = totalSeconds % 60;
        const int displayMinutes = totalSeconds / 60;
        char timer[16];
        SDL_snprintf(timer, sizeof(timer), "%02d:%02d", displayMinutes, displaySeconds);

        const float tx = cardX + 680.0f;
        renderer.drawText(timer, tx, cardY + 110.0f, Colors::TextPrimary, 78);
        renderer.drawText("Recording...", tx + 2.0f, cardY + 210.0f, Colors::TextPrimary, 30);
        renderer.drawText("Tap anywhere to stop and save the voice memory", tx + 2.0f, cardY + 260.0f, Colors::TextSecondary, 14);
        renderer.drawText("Live waveform", tx + 2.0f, cardY + 360.0f, Colors::TextPrimary, 18);

        // Horizontal audio track waveform
        for (int i = 0; i < 30; ++i)
        {
            const float wx = tx + i * 14.0f;
            const float amplitude = 12.0f + std::sin(m_elapsed * 5.0f + static_cast<float>(i) * 0.4f) * 36.0f;
            renderer.drawLine(wx, cardY + 440.0f - amplitude * 0.5f, wx, cardY + 440.0f + amplitude * 0.5f, SDL_Color { 166, 123, 250, 180 });
        }
    }
}
