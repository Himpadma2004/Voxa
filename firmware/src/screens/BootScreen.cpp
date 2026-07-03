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
        const float W = static_cast<float>(renderer.canvasWidth());   // 320
        const float H = static_cast<float>(renderer.canvasHeight());  // 240

        // Premium deep space background
        renderer.fillVerticalGradient(0.0f, 0.0f, W, H, SDL_Color { 8, 8, 12, 255 }, SDL_Color { 18, 14, 28, 255 });

        // Wave animation adapted to watch screen height
        for (int i = 0; i < 5; ++i)
        {
            const float offset = m_elapsed * 42.0f + static_cast<float>(i) * 18.0f;
            for (int x = 0; x < static_cast<int>(W); ++x)
            {
                const float xf = static_cast<float>(x);
                const float y = H * 0.8f + std::sin((xf + offset) * 0.03f) * 10.0f + std::sin((xf - offset) * 0.02f) * 6.0f + i * 2.0f;
                renderer.fillCircle(xf, y, 0.8f + i * 0.2f, SDL_Color { static_cast<Uint8>(124 + i * 12), static_cast<Uint8>(92 + i * 10), 255, static_cast<Uint8>(52 - i * 7) });
            }
        }

        renderer.drawGlowCircle(W * 0.84f, H * 0.7f, 40.0f, SDL_Color { 124, 92, 255, 18 }, 6);
        renderer.drawGlowCircle(W * 0.12f, H * 0.72f, 30.0f, SDL_Color { 92, 60, 240, 12 }, 5);
        renderer.drawGlowCircle(W * 0.5f, H * 0.22f, 50.0f, SDL_Color { 124, 92, 255, 10 }, 5);

        const float fadeAlpha = std::min(1.0f, m_elapsed * 1.5f);
        const Uint8 textAlpha = static_cast<Uint8>(fadeAlpha * 255.0f);

        // Centered VOXA title (size 36 is highly readable on watch)
        renderer.drawTextCentered("VOXA", W * 0.5f, 60.0f, SDL_Color { 210, 190, 255, textAlpha }, 36);
        
        // Tagline: "Take care of your moments"
        renderer.drawTextCentered("Take care of your moments", W * 0.5f, 105.0f, SDL_Color { 225, 225, 235, textAlpha }, 13);

        // Status text
        renderer.drawTextCentered("Initializing...", W * 0.5f, 135.0f, SDL_Color { 210, 210, 220, textAlpha }, 10);
        
        // Progress bar
        ProgressBar progressBar(Rect { W * 0.3f, 150.0f, W * 0.4f, 5.0f }, m_elapsed / 2.5f, SDL_Color { 48, 48, 60, 255 }, Colors::Primary);
        progressBar.render(renderer);
    }
}
