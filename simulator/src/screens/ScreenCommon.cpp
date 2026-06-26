#include "ScreenCommon.h"

#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Renderer.h"

namespace VOXA::ScreenCommon
{
    void renderSurface(Renderer& renderer)
    {
        const float width = static_cast<float>(renderer.canvasWidth());
        const float height = static_cast<float>(renderer.canvasHeight());
        renderer.fillVerticalGradient(0.0f, 0.0f, width, height, SDL_Color { 247, 243, 238, 255 }, SDL_Color { 237, 233, 229, 255 });
        renderer.fillCircleGradient(width * 0.82f, height * 0.16f, 220.0f, SDL_Color { 255, 255, 255, 150 }, SDL_Color { 243, 236, 231, 0 });
        renderer.fillCircleGradient(width * 0.1f, height * 0.88f, 200.0f, SDL_Color { 255, 255, 255, 95 }, SDL_Color { 243, 239, 235, 0 });
        renderer.fillCircleGradient(width * 0.55f, height * 0.5f, 360.0f, SDL_Color { 172, 145, 255, 18 }, SDL_Color { 240, 236, 232, 0 });
    }

    void renderPageDots(Renderer& renderer, int activeIndex, int count)
    {
        const float centerX = static_cast<float>(renderer.canvasWidth()) * 0.5f;
        const float startX = centerX - static_cast<float>((count - 1) * 18);
        for (int i = 0; i < count; ++i)
        {
            const bool active = i == activeIndex;
            renderer.fillRoundedRect(startX + i * 28.0f, 850.0f, active ? 26.0f : 12.0f, 6.0f, 3.0f, active ? Colors::Primary : SDL_Color { 216, 212, 209, 255 });
        }
    }

    void renderCircularButton(Renderer& renderer, float centerX, float centerY, Icon icon, SDL_Color fill, SDL_Color iconColor)
    {
        renderer.drawSoftShadow(centerX - 28.0f, centerY - 28.0f, 56.0f, 56.0f, 28.0f, 5, SDL_Color { 18, 18, 20, 10 });
        renderer.fillCircle(centerX, centerY, 28.0f, fill);
        drawIcon(renderer, icon, centerX - 13.0f, centerY - 13.0f, 26.0f, iconColor);
    }

    void renderHeader(Renderer& renderer, const std::string& title, bool showBack, bool showRightAction, Icon rightIcon)
    {
        const float width = static_cast<float>(renderer.canvasWidth());

        if (showBack)
        {
            renderCircularButton(renderer, 72.0f, 62.0f, Icon::Back, SDL_Color { 250, 247, 243, 255 }, Colors::TextPrimary);
        }

        renderer.drawTextCentered(title, width * 0.5f, 46.0f, Colors::TextPrimary, 26);

        if (showRightAction)
        {
            renderCircularButton(renderer, width - 72.0f, 62.0f, rightIcon, SDL_Color { 250, 247, 243, 255 }, Colors::TextPrimary);
        }
    }
}
