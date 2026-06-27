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
            
            // Back button tap target (top-left 40x40)
            if (Rect { 0.0f, 0.0f, 40.0f, 40.0f }.contains(point.x, point.y))
            {
                app.audio().playSoftConfirm();
                app.navigateTo(ScreenId::Home);
                return;
            }

            // Right action home button tap target (top-right 40x40)
            const float width = 320.0f;
            if (Rect { width - 40.0f, 0.0f, 40.0f, 40.0f }.contains(point.x, point.y))
            {
                app.audio().playSoftConfirm();
                app.navigateTo(ScreenId::Home);
                return;
            }

            // Central microphone button tap target (centered at cx=160, cy=100 with radius 32)
            const float cx = 160.0f;
            const float cy = 100.0f;
            const float dx = point.x - cx;
            const float dy = point.y - cy;
            if (dx * dx + dy * dy <= 32.0f * 32.0f)
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
        // Header using spark action on the right (maps to home icon)
        ScreenCommon::renderHeader(renderer, "Voice Capture", true, true, Icon::Spark);

        const float cx = 160.0f;
        const float cy = 100.0f;
        
        // Sine wave pulse factor
        const float pulse = std::sin(m_elapsed * 4.0f) * 0.5f + 0.5f; // 0.0 to 1.0
        
        // 1. Concentric pulsing halos around the mic button
        renderer.drawCircle(cx, cy, 32.0f + pulse * 4.0f, SDL_Color { 124, 92, 255, static_cast<Uint8>(40 + pulse * 20) });
        renderer.drawCircle(cx, cy, 38.0f + pulse * 8.0f, SDL_Color { 124, 92, 255, static_cast<Uint8>(15 + pulse * 10) });

        // 2. Soft button shadow
        renderer.drawSoftShadow(cx - 26.0f, cy - 26.0f, 52.0f, 52.0f, 26.0f, 3, SDL_Color { 0, 0, 0, 15 });

        // 3. Central recording button circle
        renderer.fillCircle(cx, cy, 26.0f, Colors::Primary);
        renderer.drawCircle(cx, cy, 26.0f, SDL_Color { 255, 255, 255, 60 });

        // 4. White microphone icon centered
        const float micIconSz = 16.0f;
        drawIcon(renderer, Icon::Mic, cx - micIconSz * 0.5f, cy - micIconSz * 0.5f, micIconSz, Colors::White);

        // 5. Timer readout
        const int totalSeconds = static_cast<int>(m_elapsed);
        const int displaySeconds = totalSeconds % 60;
        const int displayMinutes = totalSeconds / 60;
        char timer[16];
        SDL_snprintf(timer, sizeof(timer), "%02d:%02d", displayMinutes, displaySeconds);

        renderer.drawTextCentered(timer, cx, 142.0f, Colors::TextPrimary, 22);
        renderer.drawTextCentered("Recording...", cx, 166.0f, Colors::Primary, 11);
        renderer.drawTextCentered("Tap button to save", cx, 180.0f, Colors::TextSecondary, 9);

        // 6. Premium full-width horizontal oscilloscope wave visualizer
        for (int i = 0; i < 30; ++i)
        {
            const float wx = 15.0f + i * 10.0f;
            const float amplitude = 4.0f + std::sin(m_elapsed * 6.0f + static_cast<float>(i) * 0.35f) * 16.0f;
            renderer.drawLine(wx, 210.0f - amplitude * 0.5f, wx, 210.0f + amplitude * 0.5f, SDL_Color { 124, 92, 255, 180 });
        }
    }
}
