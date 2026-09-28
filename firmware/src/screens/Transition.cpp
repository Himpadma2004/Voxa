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
        /// iOS 26 fluid spring curve: rapid launch, buttery smooth deceleration
        float easeIOSSpring(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            float f = 1.0f - t;
            // Quartic deceleration curve matches Apple CoreAnimation CAMediaTimingFunction
            return 1.0f - (f * f * f * f);
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

        // Detail and TextInput screens are modals that slide up
        if (to == ScreenId::Detail || to == ScreenId::TextInput)
            return TransitionType::SlideUp;
        if (from == ScreenId::Detail || from == ScreenId::TextInput)
            return TransitionType::SlideDown;

        // SyncStatus and WiFiSettings are sub-pages of Settings → slide left
        if (to == ScreenId::SyncStatus || to == ScreenId::WiFiSettings)
            return TransitionType::SlideLeft;
        if (from == ScreenId::SyncStatus || from == ScreenId::WiFiSettings)
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
        float t = easeIOSSpring(rawT);

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

    // ─────────────────────────────────────────────────────────────
    // Zoom Bloom: smooth scale-from-icon using pushRotateZoom
    // ─────────────────────────────────────────────────────────────
    //  • Sprite is pre-rendered once, then scaled each frame
    //  • pushRotateZoom scales the sprite centered at (cx, cy) on LCD
    //  • Destination center slides: icon origin → screen center
    //  • Quartic ease-out + 1.02× micro-overshoot at tail
    //  • No setClipRect, no full fillScreen per-frame → zero tearing
    // ─────────────────────────────────────────────────────────────

    void playZoomBloomFrame(LGFX_Sprite& canvas, int frame, int maxFrames,
                            int originCx, int originCy, bool isOpen)
    {
        uint16_t w = Display::width();
        uint16_t h = Display::height();

        float rawT = (float)frame / (float)(maxFrames - 1);
        rawT = std::max(0.0f, std::min(1.0f, rawT));

        // Reverse for collapse animation
        float t = isOpen ? rawT : (1.0f - rawT);

        // Quartic ease-out: explosive start, buttery deceleration
        float easedT = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t) * (1.0f - t);

        // Scale: grows from 0.05 → 1.0
        float scale = 0.05f + easedT * 0.95f;

        // Tiny 1.02× overshoot ping at the tail (last 12% of animation)
        if (t > 0.88f)
        {
            float ot = (t - 0.88f) / 0.12f;           // 0..1 within overshoot window
            scale += 0.02f * std::sin(ot * 3.14159f);  // +2% ping then settle
        }

        // Destination centre: interpolates from icon pos → screen centre
        float cx = (float)originCx + (w * 0.5f - (float)originCx) * easedT;
        float cy = (float)originCy + (h * 0.5f - (float)originCy) * easedT;

        // Black fill then scaled sprite — pushRotateZoom handles the alpha border
        Display::lcd.startWrite();
        Display::lcd.fillRect(0, 0, w, h, 0x0000);
        canvas.pushRotateZoom(&Display::lcd, cx, cy, 0.0f, scale, scale);
        Display::lcd.endWrite();
    }

} // namespace VOXA
