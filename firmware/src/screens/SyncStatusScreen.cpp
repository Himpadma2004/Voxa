#include "SyncStatusScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "Transition.h"
#include <cmath>

namespace VOXA
{
    ScreenId SyncStatusScreen::show(Touch& touch)
    {
        int entryFrame = 0;
        float dragStartX = 0.0f;
        float dragStartY = 0.0f;
        bool swipeBackCandidate = false;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            return ScreenId::Settings;
        }

        ScreenId targetScreen = ScreenId::SyncStatus;
        float elapsed = 0.0f;
        uint32_t lastMs = millis();

        while (targetScreen == ScreenId::SyncStatus)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            elapsed += deltaSecs;

            // 1. Process Touch
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            if (touched && entryFrame >= 10)
            {
                m_lastDragX = tx;
                m_lastDragY = ty;
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    dragStartX = tx;
                    dragStartY = ty;
                    swipeBackCandidate = (tx < 50);
                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isBackPressed = true;
                    }
                }
                else
                {
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        targetScreen = ScreenId::Settings;
                        swipeBackCandidate = false;
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;
                    if (m_isBackPressed)
                    {
                        targetScreen = ScreenId::Settings; // returns to Settings Screen
                    }
                    m_isBackPressed = false;
                }
            }

            // Dimensions re-query
            w = Display::width();
            h = Display::height();

            // 2. Render
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Sync Status", true, false, Icon::Plus, w, h);

            float cx = w * 0.5f;
            float cy = h * 0.52f;

            // Pulsating sync check animation
            float angle = elapsed * 3.0f;
            float pulse = std::sin(elapsed * 2.0f) * 4.0f;

            // Outer circle
            canvas.drawCircle((int)cx, (int)cy, (int)(40 + pulse), VoxaTheme::getPrimaryLight());
            
            // Spinning arc inside circle
            canvas.drawArc((int)cx, (int)cy, 28, 34, (int)(angle * 57.3f), (int)((angle + 2.0f) * 57.3f), VoxaTheme::getPrimary());

            // Check icon in center
            canvas.setFont(&fonts::DejaVu18);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(VoxaTheme::getPrimary());
            canvas.drawString("OK", cx, cy);

            // Details
            canvas.setFont(&fonts::DejaVu12);
            canvas.setTextSize(1);
            canvas.setTextColor(VoxaTheme::getTextPrimary());
            canvas.drawString("Cloud Sync: Up to Date", cx, h * 0.82f);
            
            canvas.setTextColor(VoxaTheme::getTextSecondary());
            canvas.drawString("Last Sync: Just Now", cx, h * 0.90f);

            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::SyncStatus), entryFrame, 10);
                entryFrame++;
            }
            else
            {
                canvas.pushSprite(0, 0);
            }

            uint32_t frameMs = millis() - nowMs;
            if (frameMs < 16)
            {
                delay(16 - frameMs);
            }
        }

        return targetScreen;
    }
}
