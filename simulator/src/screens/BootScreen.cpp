#include "BootScreen.h"

#include <cmath>

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Renderer.h"
#include "../widgets/ProgressBar.h"

namespace VOXA
{
    ScreenId BootScreen::id() const
    {
        return ScreenId::Boot;
    }

    void BootScreen::onEnter(Application& app)
    {
        m_elapsed = 0.0f;
        app.audio().playBoot();
    }

    void BootScreen::update(Application& app, float deltaSeconds)
    {
        m_elapsed += deltaSeconds;
        if (m_elapsed >= 2.5f)
        {
            app.navigateTo(ScreenId::Home);
        }
    }

    void BootScreen::render(Application&, Renderer& renderer)
    {
        renderer.clear(Colors::Background);

        const float width = static_cast<float>(renderer.canvasWidth());
        const float height = static_cast<float>(renderer.canvasHeight());

        for (int i = 0; i < 5; ++i)
        {
            const float offset = m_elapsed * 42.0f + static_cast<float>(i) * 18.0f;
            for (int x = 0; x < static_cast<int>(width); ++x)
            {
                const float xf = static_cast<float>(x);
                const float y = height * 0.76f + std::sin((xf + offset) * 0.018f) * 28.0f + std::sin((xf - offset) * 0.01f) * 18.0f + i * 6.0f;
                renderer.fillCircle(xf, y, 1.4f + i * 0.4f, SDL_Color { static_cast<Uint8>(124 + i * 12), static_cast<Uint8>(92 + i * 10), 255, static_cast<Uint8>(52 - i * 7) });
            }
        }

        renderer.drawGlowCircle(width * 0.84f, height * 0.7f, 128.0f, SDL_Color { 124, 92, 255, 18 }, 10);
        renderer.drawGlowCircle(width * 0.12f, height * 0.72f, 92.0f, SDL_Color { 92, 60, 240, 12 }, 8);
        renderer.drawGlowCircle(width * 0.5f, height * 0.22f, 160.0f, SDL_Color { 124, 92, 255, 10 }, 9);

        renderer.drawTextCentered("VOXA", width * 0.5f, 260.0f, SDL_Color { 210, 190, 255, 255 }, 64);
        renderer.drawTextCentered("Premium Embedded AI Companion", width * 0.5f, 350.0f, SDL_Color { 225, 225, 235, 255 }, 22);
        renderer.drawTextCentered("Booting secure memory canvas and assistant modules", width * 0.5f, 392.0f, SDL_Color { 175, 176, 190, 255 }, 16);

        ProgressBar progressBar(Rect { width * 0.35f, 642.0f, width * 0.3f, 10.0f }, m_elapsed / 2.5f, SDL_Color { 48, 48, 60, 255 }, Colors::Primary);
        progressBar.render(renderer);

        renderer.drawTextCentered("Initializing...", width * 0.5f, 604.0f, SDL_Color { 210, 210, 220, 255 }, 16);
    }
}
