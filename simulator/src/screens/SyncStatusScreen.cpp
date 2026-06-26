#include "SyncStatusScreen.h"

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Button.h"
#include "ScreenCommon.h"

namespace VOXA
{
    ScreenId SyncStatusScreen::id() const
    {
        return ScreenId::SyncStatus;
    }

    void SyncStatusScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            app.navigateTo(ScreenId::Settings);
        }
    }

    void SyncStatusScreen::update(Application&, float)
    {
    }

    void SyncStatusScreen::render(Application&, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Sync Status", true, false, Icon::Plus);

        renderer.drawGlowCircle(120.0f, 114.0f, 56.0f, SDL_Color { 124, 92, 255, 14 }, 8);
        renderer.fillCircleGradient(120.0f, 114.0f, 46.0f, SDL_Color { 255, 255, 255, 255 }, SDL_Color { 245, 240, 250, 255 });
        renderer.drawCircle(120.0f, 114.0f, 42.0f, Colors::Primary);
        drawIcon(renderer, Icon::Cloud, 105.0f, 98.0f, 30.0f, Colors::Primary);
        renderer.drawLine(120.0f, 115.0f, 120.0f, 104.0f, Colors::Primary);
        renderer.drawLine(120.0f, 104.0f, 114.0f, 110.0f, Colors::Primary);
        renderer.drawLine(120.0f, 104.0f, 126.0f, 110.0f, Colors::Primary);

        renderer.drawTextCentered("3 files waiting to sync", 120.0f, 190.0f, Colors::TextPrimary, 12);
        renderer.drawTextCentered("Last synced: 2h ago", 120.0f, 206.0f, Colors::TextSecondary, 10);

        Button(Rect { 38.0f, 258.0f, 164.0f, 20.0f }, "Sync Now", true).render(renderer);
    }
}
