#include "ScreenCommon.h"

#include "../graphics/Colors.h"
#include "../graphics/Fonts.h"
#include "../graphics/Icons.h"
#include "../graphics/Renderer.h"

#include <chrono>
#include <cmath>
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

    // Draws a proper curved microphone icon using the punch-out technique with circular rounded tips.
    // Shape: pill capsule body + smooth U-arc with circular ends + vertical post + rounded base.
    // bgColor is the background color used to "erase" the arc interior and top half.
    void drawMicShape(Renderer& renderer, float cx, float cy, float size, SDL_Color color, SDL_Color bgColor)
    {
        // ── Elegant Proportions ─────────────────────────────────────
        const float bW      = size * 0.32f;   // Capsule width
        const float bH      = size * 0.52f;   // Capsule height
        const float bR      = bW * 0.5f;      // Capsule radius
        
        const float innerR  = bW * 0.5f + size * 0.08f; // Inner radius of U-arc (creates gap)
        const float armThk  = size * 0.08f;   // U-arc thickness
        const float outerR  = innerR + armThk; // Outer radius of U-arc
        const float R       = (outerR + innerR) * 0.5f; // Middle radius of U-arc
        const float tipR    = armThk * 0.5f;  // Radius of rounded tips
        
        const float postW   = size * 0.08f;   // Post width
        const float postH   = size * 0.14f;   // Post height
        const float baseW   = size * 0.48f;   // Base width
        const float baseH   = size * 0.08f;   // Base height

        // ── Vertical Anchors (centered at cy) ───────────────────────
        const float bTop    = cy - size * 0.46f;
        const float bBottom = bTop + bH;
        const float arcCy   = bBottom - bR - size * 0.02f; // Center U-arc relative to capsule bottom
        const float postTop = arcCy + outerR;
        const float baseTop = postTop + postH;

        // ── Drawing ─────────────────────────────────────────────────

        // 1. Draw U-arc outer circle
        renderer.fillCircle(cx, arcCy, outerR, color);
        // 2. Punch out U-arc inner hole
        renderer.fillCircle(cx, arcCy, innerR, bgColor);
        // 3. Erase top half of U-arc
        renderer.fillRect(cx - outerR - 1.0f, arcCy - outerR - 1.0f, (outerR + 1.0f) * 2.0f, outerR, bgColor);
        // 4. Draw rounded tips on the U-arc (perfectly circles matching outerR and innerR bounds)
        renderer.fillCircle(cx - R, arcCy, tipR, color);
        renderer.fillCircle(cx + R, arcCy, tipR, color);

        // 5. Draw Capsule Body (perfect pill shape: middle rect + top circle + bottom circle)
        renderer.fillRect(cx - bR, bTop + bR, bW, bH - bR * 2.0f, color);
        renderer.fillCircle(cx, bTop + bR, bR, color);
        renderer.fillCircle(cx, bBottom - bR, bR, color);

        // 6. Draw Post
        renderer.fillRect(cx - postW * 0.5f, postTop, postW, postH, color);

        // 7. Draw Base (perfect horizontal pill: middle rect + left circle + right circle)
        renderer.fillRect(cx - baseW * 0.5f + baseH * 0.5f, baseTop, baseW - baseH, baseH, color);
        renderer.fillCircle(cx - baseW * 0.5f + baseH * 0.5f, baseTop + baseH * 0.5f, baseH * 0.5f, color);
        renderer.fillCircle(cx + baseW * 0.5f - baseH * 0.5f, baseTop + baseH * 0.5f, baseH * 0.5f, color);
    }
}
