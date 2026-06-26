#include "ScreenCommon.h"

#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Icons.h"
#include "../graphics/Renderer.h"

#include <chrono>
#include <ctime>
#include <string>

namespace
{
    std::string getCurrentTimeAndDate()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm;
#if defined(_MSC_VER)
        localtime_s(&local_tm, &now_time);
#else
        localtime_r(&now_time, &local_tm);
#endif

        char buffer[128];
        // Format: "Friday, Jun 26  11:44 AM"
        std::strftime(buffer, sizeof(buffer), "%A, %b %d  %I:%M %p", &local_tm);
        return std::string(buffer);
    }
}

namespace VOXA::ScreenCommon
{
    void renderSurface(Renderer& renderer)
    {
        const float width = static_cast<float>(renderer.canvasWidth());
        const float height = static_cast<float>(renderer.canvasHeight());
        
        // 1. Draw a very subtle off-white/gray gradient background
        renderer.fillVerticalGradient(0.0f, 0.0f, width, height, SDL_Color { 248, 248, 249, 255 }, SDL_Color { 240, 240, 243, 255 });
        
        // 2. Overlay very soft, low-opacity ambient corner glows
        renderer.fillCircleGradient(width * 0.90f, height * 0.10f, 300.0f, SDL_Color { 166, 123, 250, 4 }, SDL_Color { 248, 248, 249, 0 });
        renderer.fillCircleGradient(width * 0.10f, height * 0.90f, 250.0f, SDL_Color { 124, 92, 255, 3 }, SDL_Color { 248, 248, 249, 0 });

        // 3. Draw a unified premium status bar at the top (y = 20.0f)
        // Left: VOXA
        renderer.drawText("VOXA", 48.0f, 18.0f, Colors::TextPrimary, 13);
        // Center: Date & Time
        renderer.drawTextCentered(getCurrentTimeAndDate(), width * 0.5f, 18.0f, Colors::TextPrimary, 13);
        // Right: Wifi and Battery status
        drawIcon(renderer, Icon::Wifi, width - 170.0f, 24.0f, 15.0f, Colors::TextPrimary);
        renderer.drawText("92%", width - 142.0f, 20.0f, Colors::TextPrimary, 11);
        drawIcon(renderer, Icon::Battery, width - 96.0f, 22.0f, 18.0f, Colors::TextPrimary);
    }

    void renderPageDots(Renderer& renderer, int activeIndex, int count)
    {
        const float centerX = static_cast<float>(renderer.canvasWidth()) * 0.5f;
        const float startX = centerX - static_cast<float>((count - 1) * 20) * 0.5f;
        for (int i = 0; i < count; ++i)
        {
            const bool active = i == activeIndex;
            SDL_Color color = active ? Colors::Primary : SDL_Color { 190, 190, 195, 255 };
            renderer.fillRoundedRect(startX + i * 20.0f, 830.0f, active ? 18.0f : 8.0f, 8.0f, 4.0f, color);
        }
    }

    void renderCircularButton(Renderer& renderer, float centerX, float centerY, Icon icon, SDL_Color fill, SDL_Color iconColor)
    {
        // VisionOS style glass circular button
        renderer.drawSoftShadow(centerX - 28.0f, centerY - 28.0f, 56.0f, 56.0f, 28.0f, 6, SDL_Color { 0, 0, 0, 14 });
        renderer.fillCircle(centerX, centerY, 28.0f, SDL_Color { 255, 255, 255, 160 }); // frosted white
        renderer.drawCircle(centerX, centerY, 28.0f, SDL_Color { 255, 255, 255, 180 }); // thin border highlight
        drawIcon(renderer, icon, centerX - 12.0f, centerY - 12.0f, 24.0f, iconColor);
    }

    void renderHeader(Renderer& renderer, const std::string& title, bool showBack, bool showRightAction, Icon rightIcon)
    {
        const float width = static_cast<float>(renderer.canvasWidth());

        if (showBack)
        {
            renderCircularButton(renderer, 72.0f, 62.0f, Icon::Back, SDL_Color { 255, 255, 255, 255 }, Colors::TextPrimary);
        }

        renderer.drawTextCentered(title, width * 0.5f, 48.0f, Colors::TextPrimary, 22);

        if (showRightAction)
        {
            renderCircularButton(renderer, width - 72.0f, 62.0f, rightIcon, SDL_Color { 255, 255, 255, 255 }, Colors::TextPrimary);
        }
    }
}
