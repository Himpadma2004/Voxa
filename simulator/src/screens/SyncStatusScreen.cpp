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
        // Header back button hit area centered at (18, 28) with radius 11
        if (Rect { 5.0f, 15.0f, 26.0f, 26.0f }.contains(point.x, point.y))
        {
            app.navigateTo(ScreenId::Settings);
            return;
        }

        // Tap "Sync Now" button
        if (Rect { 110.0f, 182.0f, 100.0f, 22.0f }.contains(point.x, point.y))
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
        const float cardX = 40.0f;
        const float cardY = 52.0f;
        const float cardW = 240.0f;
        const float cardH = 180.0f;
        
        Card container(Rect { cardX, cardY, cardW, cardH }, Colors::Card, 16.0f);
        container.setShadow(Colors::Shadow, 4);
        container.setBorder(Colors::GlassBorder);
        container.render(renderer);

        const float cx = cardX + cardW * 0.5f;
        const float cy = cardY + 45.0f;

        renderer.drawGlowCircle(cx, cy, 32.0f, SDL_Color { 124, 92, 255, 18 }, 5);
        renderer.fillCircleGradient(cx, cy, 26.0f, SDL_Color { 255, 255, 255, 180 }, SDL_Color { 245, 240, 250, 255 });
        renderer.drawCircle(cx, cy, 26.0f, Colors::Primary);
        drawIcon(renderer, Icon::Cloud, cx - 12.0f, cy - 12.0f, 24.0f, Colors::Primary);
        
        // Upward arrow inside cloud
        renderer.drawLine(cx, cy + 8.0f, cx, cy - 5.0f, Colors::Primary);
        renderer.drawLine(cx, cy - 5.0f, cx - 4.0f, cy - 1.0f, Colors::Primary);
        renderer.drawLine(cx, cy - 5.0f, cx + 4.0f, cy - 1.0f, Colors::Primary);

        renderer.drawTextCentered("3 files waiting to sync", cx, cardY + 90.0f, Colors::TextPrimary, 11);
        renderer.drawTextCentered("Last synced: 2 hours ago", cx, cardY + 108.0f, Colors::TextSecondary, 9);

        Button(Rect { cx - 50.0f, cardY + 130.0f, 100.0f, 22.0f }, "Sync Now", true).render(renderer);
    }
}
