#include "ScreenCommon.h"
#include "../ui/Theme.h"
#include <chrono>
#include <cmath>
#include <ctime>

namespace
{
    constexpr float kPi = 3.1415926535f;

    std::string getCurrentTimeStr()
    {
        std::time_t tNow = std::time(nullptr);
        std::tm local_tm;
#if defined(_MSC_VER)
        localtime_s(&local_tm, &tNow);
#else
        localtime_r(&tNow, &local_tm);
#endif
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%I:%M %p", &local_tm);
        std::string s(buffer);
        if (!s.empty() && s[0] == '0') s = s.substr(1);
        return s;
    }
}

namespace VOXA::ScreenCommon
{
    void renderSurface(LGFX_Sprite& canvas, uint16_t w, uint16_t h)
    {
        bool isLight = (VoxaTheme::getThemeMode() == VoxaTheme::ThemeMode::Light);

        // 1. Draw premium linear vertical gradient background
        // Light mode: #FFFFFF to #F6F6F7
        // Dark mode: #1A1B21 to #121212
        uint8_t topR = isLight ? 255 : 26;
        uint8_t topG = isLight ? 255 : 27;
        uint8_t topB = isLight ? 255 : 33;

        uint8_t botR = isLight ? 246 : 18;
        uint8_t botG = isLight ? 246 : 18;
        uint8_t botB = isLight ? 247 : 18;

        for (int y = 0; y < h; ++y)
        {
            float t = (float)y / (h - 1);
            uint8_t r = (uint8_t)((1.0f - t) * topR + t * botR);
            uint8_t g = (uint8_t)((1.0f - t) * topG + t * botG);
            uint8_t b = (uint8_t)((1.0f - t) * topB + t * botB);
            canvas.drawFastHLine(0, y, w, canvas.color565(r, g, b));
        }

        // 2. Overlay very subtle ambient glows to match premium watch theme
        // Top right: Primary Light glow
        float tr_cx = w * 0.90f;
        float tr_cy = h * 0.10f;
        float trAlphaBase = isLight ? 0.01f : 0.015f;
        for (int r = 5; r >= 1; --r)
        {
            float radius = 30.0f + r * 8.0f;
            float alpha = (6.0f - r) * trAlphaBase;
            
            float t_bg = tr_cy / (h - 1);
            uint8_t bg_r = (uint8_t)((1.0f - t_bg) * topR + t_bg * botR);
            uint8_t bg_g = (uint8_t)((1.0f - t_bg) * topG + t_bg * botG);
            uint8_t bg_b = (uint8_t)((1.0f - t_bg) * topB + t_bg * botB);
            
            uint8_t blend_r = (uint8_t)((1.0f - alpha) * bg_r + alpha * 166);
            uint8_t blend_g = (uint8_t)((1.0f - alpha) * bg_g + alpha * 123);
            uint8_t blend_b = (uint8_t)((1.0f - alpha) * bg_b + alpha * 250);
            
            canvas.fillCircle((int)tr_cx, (int)tr_cy, (int)radius, canvas.color565(blend_r, blend_g, blend_b));
        }

        // Bottom left: Primary glow
        float bl_cx = w * 0.10f;
        float bl_cy = h * 0.90f;
        float blAlphaBase = isLight ? 0.008f : 0.012f;
        for (int r = 5; r >= 1; --r)
        {
            float radius = 30.0f + r * 8.0f;
            float alpha = (6.0f - r) * blAlphaBase;
            
            float t_bg = bl_cy / (h - 1);
            uint8_t bg_r = (uint8_t)((1.0f - t_bg) * topR + t_bg * botR);
            uint8_t bg_g = (uint8_t)((1.0f - t_bg) * topG + t_bg * botG);
            uint8_t bg_b = (uint8_t)((1.0f - t_bg) * topB + t_bg * botB);
            
            uint8_t blend_r = (uint8_t)((1.0f - alpha) * bg_r + alpha * 124);
            uint8_t blend_g = (uint8_t)((1.0f - alpha) * bg_g + alpha * 92);
            uint8_t blend_b = (uint8_t)((1.0f - alpha) * bg_b + alpha * 255);
            
            canvas.fillCircle((int)bl_cx, (int)bl_cy, (int)radius, canvas.color565(blend_r, blend_g, blend_b));
        }

        // 3. Compact status bar at the top (using anti-aliased DejaVu12 font)
        canvas.setFont(&fonts::DejaVu12);
        canvas.setTextSize(1);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        
        canvas.setTextDatum(textdatum_t::top_left);
        canvas.drawString("VOXA", 10, 4);

        canvas.setTextDatum(textdatum_t::top_center);
        canvas.drawString(getCurrentTimeStr().c_str(), w * 0.5f, 4);

        // Status bar icons (Wifi, Battery)
        drawIcon(canvas, Icon::Wifi, w - 48, 4, 10, VoxaTheme::getTextPrimary());
        
        canvas.setTextDatum(textdatum_t::top_right);
        canvas.drawString("92%", w - 20, 4);
        
        drawIcon(canvas, Icon::Battery, w - 16, 4, 10, VoxaTheme::getTextPrimary());
    }

    void renderPageDots(LGFX_Sprite& canvas, int activeIndex, int count, uint16_t w, uint16_t h)
    {
        float dotY = h - 12.0f;
        float spacing = 12.0f;
        float centerX = w * 0.5f;
        float startX = centerX - ((count - 1) * spacing) * 0.5f;
        
        for (int i = 0; i < count; ++i)
        {
            bool active = (i == activeIndex);
            uint16_t color = active ? VoxaTheme::getPrimary() : VoxaTheme::getDivider();
            int dotW = active ? 10 : 4;
            int dotH = 4;
            canvas.fillRoundRect((int)(startX + i * spacing - dotW * 0.5f), (int)(dotY - dotH * 0.5f), dotW, dotH, 2, color);
        }
    }

    void renderCircularButton(LGFX_Sprite& canvas, float centerX, float centerY, Icon icon, 
                              uint16_t fill, uint16_t iconColor, uint16_t w, uint16_t h)
    {
        float radius = 12.0f;
        canvas.fillCircle((int)centerX, (int)centerY, (int)radius, fill);
        canvas.drawCircle((int)centerX, (int)centerY, (int)radius, VoxaTheme::getDivider());
        drawIcon(canvas, icon, centerX - 5.0f, centerY - 5.0f, 10.0f, iconColor);
    }

    void renderHeader(LGFX_Sprite& canvas, const std::string& title, bool showBack, 
                      bool showRightAction, Icon rightIcon, uint16_t w, uint16_t h)
    {
        // Safe spacing: shift Y coordinates down to 45.0f to prevent status bar overlap
        if (showBack)
        {
            renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, VoxaTheme::getSurface(), VoxaTheme::getTextPrimary(), w, h);
        }

        // Header Title in anti-aliased DejaVu18
        canvas.setFont(&fonts::DejaVu18);
        canvas.setTextSize(1);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        canvas.drawString(title.c_str(), w * 0.5f, 45.0f);

        if (showRightAction)
        {
            renderCircularButton(canvas, w - 20.0f, 45.0f, rightIcon, VoxaTheme::getSurface(), VoxaTheme::getTextPrimary(), w, h);
        }
    }

    void drawIcon(LGFX_Sprite& canvas, Icon icon, float x, float y, float size, uint16_t color)
    {
        float cx = x + size * 0.5f;
        float cy = y + size * 0.5f;

        switch (icon)
        {
            case Icon::ChevronRight:
                canvas.drawLine((int)(x + size * 0.35f), (int)(y + size * 0.2f), 
                                (int)(x + size * 0.65f), (int)(cy), color);
                canvas.drawLine((int)(x + size * 0.65f), (int)(cy), 
                                (int)(x + size * 0.35f), (int)(y + size * 0.8f), color);
                break;

            case Icon::Back:
                canvas.drawLine((int)(x + size * 0.65f), (int)(y + size * 0.2f), 
                                (int)(x + size * 0.35f), (int)(cy), color);
                canvas.drawLine((int)(x + size * 0.35f), (int)(cy), 
                                (int)(x + size * 0.65f), (int)(y + size * 0.8f), color);
                break;

            case Icon::Plus:
                canvas.drawLine((int)(x + size * 0.2f), (int)(cy), 
                                (int)(x + size * 0.8f), (int)(cy), color);
                canvas.drawLine((int)(cx), (int)(y + size * 0.2f), 
                                (int)(cx), (int)(y + size * 0.8f), color);
                break;

            case Icon::Wifi:
                canvas.fillCircle((int)cx, (int)(y + size * 0.85f), 1, color);
                canvas.drawArc((int)cx, (int)(y + size * 0.85f), (int)(size * 0.35f), (int)(size * 0.45f), 225, 315, color);
                canvas.drawArc((int)cx, (int)(y + size * 0.85f), (int)(size * 0.65f), (int)(size * 0.75f), 225, 315, color);
                break;

            case Icon::Battery:
                canvas.drawRoundRect((int)x, (int)(y + size * 0.2f), (int)(size * 0.85f), (int)(size * 0.6f), 1, color);
                canvas.fillRect((int)(x + size * 0.85f), (int)(y + size * 0.35f), (int)(size * 0.15f), (int)(size * 0.3f), color);
                canvas.fillRect((int)(x + 2), (int)(y + size * 0.2f + 2), (int)((size * 0.85f - 4) * 0.9f), (int)(size * 0.6f - 4), color);
                break;

            case Icon::Search:
                canvas.drawCircle((int)(x + size * 0.45f), (int)(y + size * 0.45f), (int)(size * 0.25f), color);
                canvas.drawLine((int)(x + size * 0.6f), (int)(y + size * 0.6f), 
                                (int)(x + size * 0.85f), (int)(y + size * 0.85f), color);
                break;

            case Icon::Mic:
                drawMicShape(canvas, cx, cy, size, color, VoxaTheme::getBackground());
                break;

            case Icon::Bell:
                canvas.fillCircle((int)cx, (int)(cy - size * 0.08f), (int)(size * 0.22f), color);
                canvas.fillRect((int)(cx - size * 0.32f), (int)(cy + size * 0.14f), (int)(size * 0.64f), (int)(size * 0.10f), color);
                canvas.drawCircle((int)cx, (int)(cy - size * 0.32f), (int)(size * 0.08f), color);
                canvas.fillCircle((int)cx, (int)(cy + size * 0.28f), (int)(size * 0.08f), color);
                break;

            case Icon::Lightbulb:
                canvas.drawCircle((int)cx, (int)(cy - size * 0.10f), (int)(size * 0.24f), color);
                canvas.fillRect((int)(cx - size * 0.12f), (int)(cy + size * 0.14f), (int)(size * 0.24f), (int)(size * 0.06f), color);
                canvas.fillRect((int)(cx - size * 0.08f), (int)(cy + size * 0.22f), (int)(size * 0.16f), (int)(size * 0.06f), color);
                canvas.drawLine((int)cx, (int)(cy - size * 0.46f), (int)cx, (int)(cy - size * 0.38f), color);
                canvas.drawLine((int)(cx - size * 0.32f), (int)(cy - size * 0.32f), 
                                (int)(cx - size * 0.22f), (int)(cy - size * 0.22f), color);
                canvas.drawLine((int)(cx + size * 0.32f), (int)(cy - size * 0.32f), 
                                (int)(cx + size * 0.22f), (int)(cy - size * 0.22f), color);
                break;

            case Icon::Question:
                canvas.setFont(&fonts::DejaVu12);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(color);
                canvas.setTextSize(size >= 14.0f ? 2 : 1);
                canvas.drawString("?", cx, cy);
                break;

            case Icon::Folder:
                canvas.drawRoundRect((int)x, (int)(y + size * 0.25f), (int)(size * 0.9f), (int)(size * 0.6f), 1, color);
                canvas.fillRoundRect((int)(x + size * 0.1f), (int)(y + size * 0.12f), 
                                     (int)(size * 0.35f), (int)(size * 0.2f), 1, color);
                break;

            case Icon::Settings:
                canvas.drawCircle((int)cx, (int)cy, (int)(size * 0.12f), color);
                canvas.drawCircle((int)cx, (int)cy, (int)(size * 0.3f), color);
                for (int i = 0; i < 6; ++i)
                {
                    float angle = i * (kPi / 3.0f);
                    canvas.drawLine((int)(cx + cos(angle) * size * 0.3f), (int)(cy + sin(angle) * size * 0.3f),
                                    (int)(cx + cos(angle) * size * 0.42f), (int)(cy + sin(angle) * size * 0.42f), color);
                }
                break;

            default:
                canvas.drawRect((int)x, (int)y, (int)size, (int)size, color);
                break;
        }
    }

    void drawMicShape(LGFX_Sprite& canvas, float cx, float cy, float size, uint16_t color, uint16_t bgColor)
    {
        const float bW      = size * 0.32f;
        const float bH      = size * 0.52f;
        const float bR      = bW * 0.5f;
        
        const float innerR  = bW * 0.5f + size * 0.08f;
        const float armThk  = size * 0.08f;
        const float outerR  = innerR + armThk;
        const float R       = (outerR + innerR) * 0.5f;
        const float tipR    = armThk * 0.5f;
        
        const float postW   = size * 0.08f;
        const float postH   = size * 0.14f;
        const float baseW   = size * 0.48f;
        const float baseH   = size * 0.08f;

        const float bTop    = cy - size * 0.46f;
        const float bBottom = bTop + bH;
        const float arcCy   = bBottom - bR - size * 0.02f;
        const float postTop = arcCy + outerR;
        const float baseTop = postTop + postH;

        canvas.fillCircle((int)cx, (int)arcCy, (int)outerR, color);
        canvas.fillCircle((int)cx, (int)arcCy, (int)innerR, bgColor);
        canvas.fillRect((int)(cx - outerR - 1.0f), (int)(arcCy - outerR - 1.0f), 
                        (int)((outerR + 1.0f) * 2.0f), (int)outerR, bgColor);
        canvas.fillCircle((int)(cx - R), (int)arcCy, (int)tipR, color);
        canvas.fillCircle((int)(cx + R), (int)arcCy, (int)tipR, color);

        canvas.fillRect((int)(cx - bR), (int)(bTop + bR), (int)bW, (int)(bH - bR * 2.0f), color);
        canvas.fillCircle((int)cx, (int)(bTop + bR), (int)bR, color);
        canvas.fillCircle((int)cx, (int)(bBottom - bR), (int)bR, color);

        canvas.fillRect((int)(cx - postW * 0.5f), (int)postTop, (int)postW, (int)postH, color);

        canvas.fillRect((int)(cx - baseW * 0.5f + baseH * 0.5f), (int)baseTop, (int)(baseW - baseH), (int)baseH, color);
        canvas.fillCircle((int)(cx - baseW * 0.5f + baseH * 0.5f), (int)(baseTop + baseH * 0.5f), (int)(baseH * 0.5f), color);
        canvas.fillCircle((int)(cx + baseW * 0.5f - baseH * 0.5f), (int)(baseTop + baseH * 0.5f), (int)(baseH * 0.5f), color);
    }
}
