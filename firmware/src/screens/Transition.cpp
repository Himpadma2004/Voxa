#include "Transition.h"
#include "../ui/Theme.h"
#include "../display/Display.h"
#include <cmath>
#include <algorithm>

namespace VOXA
{
    ScreenId g_lastScreenId = ScreenId::Boot;

    namespace
    {
        /// Cubic ease-out: fast start, decelerates to a stop
        float easeOutCubic(float t)
        {
            float f = 1.0f - t;
            return 1.0f - (f * f * f);
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Screen hierarchy — used to infer transition direction
    // ─────────────────────────────────────────────────────────────

    TransitionType getTransitionType(ScreenId from, ScreenId to)
    {
        if (from == to || from == ScreenId::Boot)
            return TransitionType::None;

        // Going back to Home always slides down (pop close)
        if (to == ScreenId::Home)
            return TransitionType::SlideDown;

        // Detail screen is a modal that slides up
        if (to == ScreenId::Detail)
            return TransitionType::SlideUp;
        if (from == ScreenId::Detail)
            return TransitionType::SlideDown;

        // SyncStatus is a sub-page of Settings → slide left
        if (to == ScreenId::SyncStatus)
            return TransitionType::SlideLeft;
        if (from == ScreenId::SyncStatus)
            return TransitionType::SlideRight;

        // Any top-level screen navigated into from Home → slide up (pop)
        if (from == ScreenId::Home)
            return TransitionType::SlideUp;

        // Default: slide left (forward)
        return TransitionType::SlideLeft;
    }

    // ─────────────────────────────────────────────────────────────
    // Slide In Frame Rendering (0 extra heap allocations)
    // ─────────────────────────────────────────────────────────────

    void playSlideInFrame(LGFX_Sprite& canvas, TransitionType type, int frame, int maxFrames)
    {
        if (type == TransitionType::None)
        {
            canvas.pushSprite(0, 0);
            return;
        }

        uint16_t w = Display::width();
        uint16_t h = Display::height();
        
        // Progress from 0.0 to 1.0
        float rawT = (float)frame / (float)maxFrames;
        float t = easeOutCubic(rawT);

        Display::lcd.startWrite();
        uint16_t bg = VoxaTheme::getBackground();

        switch (type)
        {
        case TransitionType::SlideLeft: // enters from right
            {
                int newX = w - (int)(t * w);
                canvas.pushSprite(newX, 0);
                if (newX > 0)
                {
                    Display::lcd.fillRect(0, 0, newX, h, bg);
                }
            }
            break;

        case TransitionType::SlideRight: // enters from left
            {
                int newX = -w + (int)(t * w);
                canvas.pushSprite(newX, 0);
                int rightBound = newX + w;
                if (rightBound < w)
                {
                    Display::lcd.fillRect(rightBound, 0, w - rightBound, h, bg);
                }
            }
            break;

        case TransitionType::SlideUp: // enters from bottom
            {
                int newY = h - (int)(t * h);
                canvas.pushSprite(0, newY);
                if (newY > 0)
                {
                    Display::lcd.fillRect(0, 0, w, newY, bg);
                }
            }
            break;

        case TransitionType::SlideDown: // enters from top
            {
                int newY = -h + (int)(t * h);
                canvas.pushSprite(0, newY);
                int bottomBound = newY + h;
                if (bottomBound < h)
                {
                    Display::lcd.fillRect(0, bottomBound, w, h - bottomBound, bg);
                }
            }
            break;

        case TransitionType::FadeScale:
            {
                canvas.pushSprite(0, 0);
                int lines = (int)((1.0f - t) * h);
                if (lines > 0)
                {
                    int step = std::max(1, h / lines);
                    for (int y = 0; y < h; y += step)
                    {
                        Display::lcd.drawFastHLine(0, y, w, bg);
                    }
                }
            }
            break;

        default:
            canvas.pushSprite(0, 0);
            break;
        }
        Display::lcd.endWrite();
    }

} // namespace VOXA
