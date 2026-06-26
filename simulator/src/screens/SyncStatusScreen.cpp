#include "SyncStatusScreen.h"

#include "../core/Application.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Button.h"
#include "../widgets/Card.h"
#include "ScreenCommon.h"

namespace VOXA
{
    ScreenId SyncStatusScreen::id() const
    {
        return ScreenId::SyncStatus;
    }

    void SyncStatusScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            return;
        }

        const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
        if (Rect { 44.0f, 34.0f, 56.0f, 56.0f }.contains(point.x, point.y))
        {
            app.navigateTo(ScreenId::Settings);
            return;
        }

        // Tap "Sync Now" button
        if (Rect { 700.0f, 540.0f, 200.0f, 48.0f }.contains(point.x, point.y))
        {
            app.audio().playSoftConfirm();
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

        // Center glass card container
        const float cardX = 550.0f;
        const float cardY = 200.0f;
        const float cardW = 500.0f;
        const float cardH = 500.0f;
        
        Card container(Rect { cardX, cardY, cardW, cardH }, Colors::Card, 32.0f);
        container.setShadow(Colors::Shadow, 8);
        container.setBorder(Colors::GlassBorder);
        container.render(renderer);

        const float cx = cardX + cardW * 0.5f;
        const float cy = cardY + 140.0f;

        renderer.drawGlowCircle(cx, cy, 70.0f, SDL_Color { 124, 92, 255, 18 }, 8);
        renderer.fillCircleGradient(cx, cy, 58.0f, SDL_Color { 255, 255, 255, 180 }, SDL_Color { 245, 240, 250, 255 });
        renderer.drawCircle(cx, cy, 58.0f, Colors::Primary);
        drawIcon(renderer, Icon::Cloud, cx - 24.0f, cy - 24.0f, 48.0f, Colors::Primary);
        
        // Upward arrow
        renderer.drawLine(cx, cy + 18.0f, cx, cy - 10.0f, Colors::Primary);
        renderer.drawLine(cx, cy - 10.0f, cx - 8.0f, cy - 2.0f, Colors::Primary);
        renderer.drawLine(cx, cy - 10.0f, cx + 8.0f, cy - 2.0f, Colors::Primary);

        renderer.drawTextCentered("3 files waiting to sync", cx, cardY + 230.0f, Colors::TextPrimary, 18);
        renderer.drawTextCentered("Last synced: 2 hours ago", cx, cardY + 270.0f, Colors::TextSecondary, 13);

        Button(Rect { cx - 100.0f, cardY + 340.0f, 200.0f, 48.0f }, "Sync Now", true).render(renderer);
    }
}
