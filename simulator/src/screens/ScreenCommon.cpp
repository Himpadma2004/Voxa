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
        // watch-like compact time: e.g. "11:44 AM"
        std::strftime(buffer, sizeof(buffer), "%I:%M %p", &local_tm);
        std::string s(buffer);
        if (!s.empty() && s[0] == '0') s = s.substr(1);
        return s;
    }
}

namespace VOXA::ScreenCommon
{
    void renderSurface(Renderer& renderer)
    {
        const float width = static_cast<float>(renderer.canvasWidth());
        const float height = static_cast<float>(renderer.canvasHeight());
        
        // 1. Draw a very subtle off-white/gray watch background
        renderer.fillVerticalGradient(0.0f, 0.0f, width, height, SDL_Color { 248, 248, 249, 255 }, SDL_Color { 240, 240, 243, 255 });
        
        // 2. Overlay soft, low-opacity ambient corner glows
        renderer.fillCircleGradient(width * 0.90f, height * 0.10f, 80.0f, SDL_Color { 166, 123, 250, 6 }, SDL_Color { 248, 248, 249, 0 });
        renderer.fillCircleGradient(width * 0.10f, height * 0.90f, 80.0f, SDL_Color { 124, 92, 255, 5 }, SDL_Color { 248, 248, 249, 0 });

        // 3. Compact status bar at the top (watch-optimized)
        // Force recompilation for smartwatch status bar updates
        // Left: VOXA Brand Name
        renderer.drawText("VOXA", 10.0f, 4.0f, Colors::TextPrimary, 9);
        
        // Center: Compact Watch Time (only hours/minutes, no day/date overlap)
        renderer.drawTextCentered(getCurrentTimeAndDate(), width * 0.5f, 4.0f, Colors::TextPrimary, 9);
        
        // Right: Wifi icon, battery percentage, battery icon
        drawIcon(renderer, Icon::Wifi, width - 48.0f, 4.0f, 10.0f, Colors::TextPrimary);
        renderer.drawText("92%", width - 34.0f, 4.0f, Colors::TextPrimary, 8);
        drawIcon(renderer, Icon::Battery, width - 16.0f, 4.0f, 10.0f, Colors::TextPrimary);
    }

    void renderPageDots(Renderer& renderer, int activeIndex, int count)
    {
        const float centerX = static_cast<float>(renderer.canvasWidth()) * 0.5f;
        const float startX = centerX - static_cast<float>((count - 1) * 12) * 0.5f;
        for (int i = 0; i < count; ++i)
        {
            const bool active = i == activeIndex;
            SDL_Color color = active ? Colors::Primary : SDL_Color { 190, 190, 195, 255 };
            renderer.fillRoundedRect(startX + i * 12.0f, 225.0f, active ? 10.0f : 4.0f, 4.0f, 2.0f, color);
        }
    }

    void renderCircularButton(Renderer& renderer, float centerX, float centerY, Icon icon, SDL_Color fill, SDL_Color iconColor)
    {
        // Compact circular button - visually smaller, but hit-test will be kept large (40x40)
        renderer.drawSoftShadow(centerX - 9.0f, centerY - 9.0f, 18.0f, 18.0f, 9.0f, 2, SDL_Color { 0, 0, 0, 14 });
        renderer.fillCircle(centerX, centerY, 9.0f, fill); // white card fill
        renderer.drawCircle(centerX, centerY, 9.0f, SDL_Color { 255, 255, 255, 180 }); // border highlight
        drawIcon(renderer, icon, centerX - 4.5f, centerY - 4.5f, 9.0f, iconColor);
    }

    void renderHeader(Renderer& renderer, const std::string& title, bool showBack, bool showRightAction, Icon rightIcon)
    {
        const float width = static_cast<float>(renderer.canvasWidth());

        if (showBack)
        {
            renderCircularButton(renderer, 18.0f, 28.0f, Icon::Back, SDL_Color { 255, 255, 255, 255 }, Colors::TextPrimary);
        }

        renderer.drawTextCentered(title, width * 0.5f, 22.0f, Colors::TextPrimary, 13);

        if (showRightAction)
        {
            renderCircularButton(renderer, width - 18.0f, 28.0f, rightIcon, SDL_Color { 255, 255, 255, 255 }, Colors::TextPrimary);
        }
    }
}
