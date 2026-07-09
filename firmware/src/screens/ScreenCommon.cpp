#include "ScreenCommon.h"
#include "../ui/Theme.h"
#include "../services/TimeService.h"
#include "../services/WiFiManager.h"
#include <chrono>
#include <cmath>
#include <ctime>

namespace
{
    constexpr float kPi = 3.1415926535f;

    std::string getCurrentTimeStr()
    {
        return VOXA::timeService.getCurrentTime();
    }
}

namespace VOXA::ScreenCommon
{
    void renderSurface(LovyanGFX& canvas, uint16_t w, uint16_t h)
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

        // 3. Compact status bar at the top (using anti-aliased FreeSans9pt7b font)
        canvas.setFont(&fonts::FreeSans9pt7b);
        canvas.setTextSize(1);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        
        canvas.setTextDatum(textdatum_t::top_left);
        canvas.drawString("VOXA", 10, 4);

        canvas.setTextDatum(textdatum_t::top_center);
        canvas.drawString(getCurrentTimeStr().c_str(), w * 0.5f, 4);

        // Status bar icons (Wifi, Battery)
        uint16_t wifiColor = VOXA::wifiManager.isConnected() ? VoxaTheme::getTextPrimary() : VoxaTheme::getDivider();
        drawIcon(canvas, Icon::Wifi, w - 48, 4, 10, wifiColor);
        
        canvas.setTextDatum(textdatum_t::top_right);
        canvas.drawString("92%", w - 20, 4);
        
        drawIcon(canvas, Icon::Battery, w - 16, 4, 10, VoxaTheme::getTextPrimary());
    }

    void renderPageDots(LovyanGFX& canvas, int activeIndex, int count, uint16_t w, uint16_t h)
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

    void renderCircularButton(LovyanGFX& canvas, float centerX, float centerY, Icon icon, 
                              uint16_t fill, uint16_t iconColor, uint16_t w, uint16_t h)
    {
        float radius = 12.0f;
        canvas.fillCircle((int)centerX, (int)centerY, (int)radius, fill);
        canvas.drawCircle((int)centerX, (int)centerY, (int)radius, VoxaTheme::getDivider());
        drawIcon(canvas, icon, centerX - 5.0f, centerY - 5.0f, 10.0f, iconColor);
    }

    void renderHeader(LovyanGFX& canvas, const std::string& title, bool showBack, 
                      bool showRightAction, Icon rightIcon, uint16_t w, uint16_t h)
    {
        // Safe spacing: shift Y coordinates down to 45.0f to prevent status bar overlap
        if (showBack)
        {
            renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, VoxaTheme::getSurface(), VoxaTheme::getTextPrimary(), w, h);
        }

        // Header Title in anti-aliased FreeSansBold12pt7b
        canvas.setFont(&fonts::FreeSansBold12pt7b);
        canvas.setTextSize(1);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        canvas.drawString(title.c_str(), w * 0.5f, 45.0f);

        if (showRightAction)
        {
            renderCircularButton(canvas, w - 20.0f, 45.0f, rightIcon, VoxaTheme::getSurface(), VoxaTheme::getTextPrimary(), w, h);
        }
    }

    void drawIcon(LovyanGFX& canvas, Icon icon, float x, float y, float size, uint16_t color)
    {
        float cx = x + size * 0.5f;
        float cy = y + size * 0.5f;

        switch (icon)
        {
            case Icon::ChevronRight:
                for (int t = -1; t <= 1; t++) {
                    canvas.drawLine(x+size*0.35f, y+size*0.2f+t, x+size*0.65f, cy+t, color);
                    canvas.drawLine(x+size*0.65f, cy+t, x+size*0.35f, y+size*0.8f+t, color);
                }
                break;

            case Icon::Back:
                for (int t = -1; t <= 1; t++) {
                    canvas.drawLine(x+size*0.65f, y+size*0.2f+t, x+size*0.35f, cy+t, color);
                    canvas.drawLine(x+size*0.35f, cy+t, x+size*0.65f, y+size*0.8f+t, color);
                }
                break;

            case Icon::Plus:
                {
                    int barW = std::max(2, (int)(size*0.15f));
                    canvas.fillRect(cx-barW/2, y+size*0.15f, barW, size*0.7f, color);
                    canvas.fillRect(x+size*0.15f, cy-barW/2, size*0.7f, barW, color);
                }
                break;

            case Icon::Wifi:
                canvas.fillCircle(cx, y+size*0.82f, size*0.07f, color);
                canvas.drawArc(cx, y+size*0.82f, size*0.25f, size*0.33f, 220, 320, color);
                canvas.drawArc(cx, y+size*0.82f, size*0.48f, size*0.56f, 220, 320, color);
                canvas.drawArc(cx, y+size*0.82f, size*0.70f, size*0.78f, 220, 320, color);
                break;

            case Icon::Battery:
                {
                    int bx=x+size*0.05f, by=y+size*0.25f, bw=size*0.80f, bh=size*0.50f;
                    canvas.drawRoundRect(bx, by, bw, bh, 2, color);
                    canvas.fillRect(bx+bw, by+bh*0.30f, size*0.10f, bh*0.40f, color);
                    canvas.fillRoundRect(bx+2, by+2, (bw-4)*0.88f, bh-4, 1, color);
                }
                break;

            case Icon::Search:
                for(int r=0;r<2;r++) canvas.drawCircle(x+size*0.43f, y+size*0.43f, size*0.26f-r, color);
                for(int t=-1;t<=1;t++) canvas.drawLine(x+size*0.61f+t, y+size*0.61f, x+size*0.85f+t, y+size*0.85f, color);
                break;

            case Icon::Mic:
                drawMicShape(canvas, cx, cy, size, color, VoxaTheme::getBackground());
                break;

            case Icon::Bell:
                canvas.fillCircle(cx, cy-size*0.12f, size*0.28f, color);
                canvas.fillRect(cx-size*0.28f, cy-size*0.12f, size*0.56f, size*0.28f, color);
                canvas.fillRoundRect(cx-size*0.34f, cy+size*0.16f, size*0.68f, size*0.12f, 2, color);
                canvas.fillCircle(cx, cy+size*0.36f, size*0.09f, color);
                canvas.fillRect(cx-size*0.05f, cy-size*0.44f, size*0.10f, size*0.14f, color);
                break;

            case Icon::Lightbulb:
                canvas.fillCircle(cx, cy-size*0.08f, size*0.26f, color);
                canvas.fillRect(cx-size*0.14f, cy+size*0.17f, size*0.28f, size*0.08f, color);
                canvas.fillRect(cx-size*0.11f, cy+size*0.25f, size*0.22f, size*0.08f, color);
                canvas.fillRect(cx-size*0.04f, cy-size*0.44f, size*0.08f, size*0.12f, color);
                break;

            case Icon::Question:
                canvas.drawArc(cx, cy-size*0.18f, size*0.18f, size*0.24f, 200, 360+60, color);
                for(int t=-1;t<=1;t++) canvas.drawLine(cx+t, cy-size*0.02f, cx+t, cy+size*0.18f, color);
                canvas.fillCircle(cx, cy+size*0.30f, size*0.07f, color);
                break;

            case Icon::Folder:
                canvas.fillRoundRect(x+size*0.05f, y+size*0.28f, size*0.90f, size*0.58f, 3, color);
                canvas.fillRoundRect(x+size*0.05f, y+size*0.16f, size*0.38f, size*0.18f, 3, color);
                break;

            case Icon::Settings:
                canvas.fillCircle(cx, cy, size*0.16f, color);
                for (int i=0; i<8; i++) {
                    float angle = i * (kPi/4.0f);
                    float tx = cx + cos(angle)*size*0.32f;
                    float ty = cy + sin(angle)*size*0.32f;
                    canvas.fillCircle(tx, ty, size*0.09f, color);
                }
                for(int r=0;r<3;r++) canvas.drawCircle(cx, cy, size*0.22f+r, color);
                break;

            case Icon::Star:
                for (int i=0; i<5; i++) {
                    float a1 = -kPi*0.5f + i*(2*kPi/5.0f);
                    float a2 = a1 + kPi/5.0f;
                    float a3 = a1 + 2*kPi/5.0f;
                    int x1=cx+cos(a1)*size*0.42f, y1=cy+sin(a1)*size*0.42f;
                    int x2=cx+cos(a2)*size*0.18f, y2=cy+sin(a2)*size*0.18f;
                    int x3=cx+cos(a3)*size*0.42f, y3=cy+sin(a3)*size*0.42f;
                    canvas.fillTriangle(cx,cy, x1,y1, x2,y2, color);
                    canvas.fillTriangle(cx,cy, x2,y2, x3,y3, color);
                }
                break;

            case Icon::Upload:
                canvas.fillRect(cx-size*0.06f, cy-size*0.10f, size*0.12f, size*0.40f, color);
                canvas.fillTriangle(cx, cy-size*0.42f, cx-size*0.22f, cy-size*0.16f, cx+size*0.22f, cy-size*0.16f, color);
                canvas.fillRect(cx-size*0.30f, cy+size*0.34f, size*0.60f, size*0.10f, color);
                break;

            case Icon::Rotate:
                canvas.drawArc(cx, cy, size*0.28f, size*0.38f, 60, 340, color);
                {
                    float ah = 60.0f * kPi/180.0f;
                    int arx = cx + cos(ah)*size*0.33f;
                    int ary = cy - sin(ah)*size*0.33f;
                    for(int t=-1;t<=1;t++) {
                        canvas.drawLine(arx, ary, arx+size*0.12f, ary-size*0.10f, color);
                        canvas.drawLine(arx, ary, arx+size*0.14f, ary+size*0.08f, color);
                    }
                }
                break;

            case Icon::Cloud:
                canvas.fillCircle(cx - size * 0.18f, cy + size * 0.08f, size * 0.16f, color);
                canvas.fillCircle(cx + size * 0.18f, cy + size * 0.08f, size * 0.16f, color);
                canvas.fillCircle(cx, cy - size * 0.06f, size * 0.22f, color);
                canvas.fillRect(cx - size * 0.18f, cy - size * 0.08f, size * 0.36f, size * 0.32f, color);
                break;

            case Icon::Storage:
                canvas.fillRoundRect(x + size * 0.10f, y + size * 0.15f, size * 0.80f, size * 0.20f, 2, color);
                canvas.fillRoundRect(x + size * 0.10f, y + size * 0.40f, size * 0.80f, size * 0.20f, 2, color);
                canvas.fillRoundRect(x + size * 0.10f, y + size * 0.65f, size * 0.80f, size * 0.20f, 2, color);
                break;

            case Icon::Info:
                canvas.drawCircle(cx, cy, size * 0.45f, color);
                canvas.drawCircle(cx, cy, size * 0.40f, color);
                canvas.fillCircle(cx, cy - size * 0.16f, size * 0.08f, color);
                canvas.fillRect(cx - size * 0.06f, cy - size * 0.04f, size * 0.12f, size * 0.32f, color);
                break;

            case Icon::Note:
                canvas.drawRoundRect(x + size * 0.15f, y + size * 0.10f, size * 0.70f, size * 0.80f, 2, color);
                canvas.fillRect(x + size * 0.28f, y + size * 0.30f, size * 0.44f, size * 0.08f, color);
                canvas.fillRect(x + size * 0.28f, y + size * 0.48f, size * 0.44f, size * 0.08f, color);
                canvas.fillRect(x + size * 0.28f, y + size * 0.66f, size * 0.30f, size * 0.08f, color);
                break;

            case Icon::Chat:
                canvas.fillRoundRect(x + size * 0.10f, y + size * 0.15f, size * 0.80f, size * 0.55f, 3, color);
                canvas.fillTriangle(x + size * 0.25f, y + size * 0.70f, x + size * 0.40f, y + size * 0.70f, x + size * 0.25f, y + size * 0.88f, color);
                break;

            case Icon::Spark:
                for (int i = 0; i < 4; i++) {
                    float angle = i * (kPi / 2.0f);
                    int x1 = cx + cos(angle) * size * 0.45f;
                    int y1 = cy + sin(angle) * size * 0.45f;
                    int x2 = cx + cos(angle + kPi/4.0f) * size * 0.12f;
                    int y2 = cy + sin(angle + kPi/4.0f) * size * 0.12f;
                    int x3 = cx + cos(angle - kPi/4.0f) * size * 0.12f;
                    int y3 = cy + sin(angle - kPi/4.0f) * size * 0.12f;
                    canvas.fillTriangle(cx, cy, x1, y1, x2, y2, color);
                    canvas.fillTriangle(cx, cy, x1, y1, x3, y3, color);
                }
                break;

            case Icon::Calendar:
                canvas.drawRoundRect(x + size * 0.10f, y + size * 0.20f, size * 0.80f, size * 0.70f, 2, color);
                canvas.fillRect(x + size * 0.10f, y + size * 0.20f, size * 0.80f, size * 0.18f, color);
                canvas.fillRect(x + size * 0.25f, y + size * 0.08f, size * 0.08f, size * 0.20f, color);
                canvas.fillRect(x + size * 0.67f, y + size * 0.08f, size * 0.08f, size * 0.20f, color);
                canvas.fillRect(x + size * 0.25f, y + size * 0.48f, size * 0.12f, size * 0.12f, color);
                canvas.fillRect(x + size * 0.63f, y + size * 0.48f, size * 0.12f, size * 0.12f, color);
                canvas.fillRect(x + size * 0.25f, y + size * 0.68f, size * 0.12f, size * 0.12f, color);
                canvas.fillRect(x + size * 0.63f, y + size * 0.68f, size * 0.12f, size * 0.12f, color);
                break;

            case Icon::Filter:
                canvas.fillTriangle(cx, cy + size * 0.35f, x + size * 0.15f, y + size * 0.15f, x + size * 0.85f, y + size * 0.15f, color);
                canvas.fillRect(cx - size * 0.08f, cy, size * 0.16f, size * 0.38f, color);
                break;

            default:
                canvas.drawRect((int)x, (int)y, (int)size, (int)size, color);
                break;
        }
    }

    void drawMicShape(LovyanGFX& canvas, float cx, float cy, float size, uint16_t color, uint16_t bgColor)
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
