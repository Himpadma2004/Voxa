#include "ScreenCommon.h"
#include "../ui/Theme.h"
#include "../ui/IconBitmaps.h"
#include "../services/TimeService.h"
#include "../services/WiFiManager.h"
#include "../services/BatteryManager.h"
#include "../services/MicrophoneService.h"
#include "../reminders/ReminderManager.h"

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
        // Intercept rendering to display active reminder popups
        ReminderManager::instance().checkAndShowPopup(canvas);

        // 1. Deep OLED Obsidian Black Background
        canvas.fillScreen(VoxaTheme::getBackground());

        // 2. Dual-tone Ambient Liquid Glass Glow (iOS 26 Refraction Aura) - Dark Mode Only
        if (VoxaTheme::isDarkMode())
        {
            // Top-Right: Warm Amber / Flame Glow
            float tr_cx = w * 0.90f;
            float tr_cy = h * 0.08f;
            uint16_t amber = VoxaTheme::getPrimary();
            uint8_t aR = (amber >> 11) << 3;
            uint8_t aG = ((amber >> 5) & 0x3F) << 2;
            uint8_t aB = (amber & 0x1F) << 3;

            for (int r = 4; r >= 1; --r)
            {
                float radius = 16.0f + r * 12.0f;
                float alpha = (5.0f - r) * 0.018f;
                canvas.fillCircle((int)tr_cx, (int)tr_cy, (int)radius, 
                    canvas.color565((uint8_t)(alpha * aR), (uint8_t)(alpha * aG), (uint8_t)(alpha * aB)));
            }

            // Top-Left: Subtle iOS Electric Cyan / Indigo Ambient Shimmer
            float tl_cx = w * 0.10f;
            float tl_cy = h * 0.06f;
            uint16_t cyan = VoxaTheme::getSystemBlue();
            uint8_t cR = (cyan >> 11) << 3;
            uint8_t cG = ((cyan >> 5) & 0x3F) << 2;
            uint8_t cB = (cyan & 0x1F) << 3;
            for (int r = 3; r >= 1; --r)
            {
                float radius = 12.0f + r * 10.0f;
                float alpha = (4.0f - r) * 0.015f;
                canvas.fillCircle((int)tl_cx, (int)tl_cy, (int)radius,
                    canvas.color565((uint8_t)(alpha * cR), (uint8_t)(alpha * cG), (uint8_t)(alpha * cB)));
            }
        }

        // 3. iOS 26 Dynamic Island (Centered Floating High-Contrast Capsule)
        int islandW = 158;
        int islandH = 22;
        int islandX = (w - islandW) / 2;
        int islandY = 3;
        int islandR = 11;

        bool isRecording = VOXA::microphoneService.isRecording();

        // Island Capsule Fill (Always sleek obsidian capsule)
        uint16_t islandFill = isRecording ? canvas.color565(32, 12, 16) : VoxaTheme::getDynamicIslandBg();
        uint16_t islandBorder = isRecording 
            ? (((millis() / 400) % 2 == 0) ? VoxaTheme::getSystemRed() : VoxaTheme::getPrimary()) 
            : (VoxaTheme::isDarkMode() ? VoxaTheme::getGlassBorder() : canvas.color565(30, 32, 40));

        canvas.fillRoundRect(islandX, islandY, islandW, islandH, islandR, islandFill);
        canvas.drawRoundRect(islandX, islandY, islandW, islandH, islandR, islandBorder);

        // Specular Optical Highlight along Island Top Edge
        canvas.drawFastHLine(islandX + islandR + 2, islandY + 1, islandW - (islandR * 2 + 4), 
            VoxaTheme::isDarkMode() ? VoxaTheme::getGlassHighlight() : canvas.color565(55, 60, 75));

        // Island Contents:
        int curX = islandX + 10;

        // Recording Live Indicator Dot
        if (isRecording)
        {
            uint16_t dotCol = ((millis() / 350) % 2 == 0) ? VoxaTheme::getSystemRed() : canvas.color565(160, 20, 20);
            canvas.fillCircle(curX + 4, islandY + islandH / 2, 4, dotCol);
            curX += 13;
        }

        // Clock String in Clean SF Typography (Always Crisp White Inside Dynamic Island)
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextSize(1);
        canvas.setTextDatum(textdatum_t::middle_left);
        canvas.setTextColor(VoxaTheme::getDynamicIslandFg(), islandFill);
        canvas.drawString(getCurrentTimeStr().c_str(), curX, islandY + islandH / 2 + 1);

        // Wi-Fi Icon
        bool isWifiConnected = VOXA::wifiManager.isConnected();
        uint16_t wifiColor = isWifiConnected ? VoxaTheme::getSystemBlue() : VoxaTheme::getTextSecondary();
        int wifiX = islandX + islandW - 46;
        drawIcon(canvas, isWifiConnected ? Icon::Wifi : Icon::WiFiOff, wifiX, islandY + 5, 12, wifiColor);

        // Battery Capsule (Authentic iOS Mini Capsule)
        int batX = islandX + islandW - 28;
        int batY = islandY + 6;
        int batW = 20;
        int batH = 10;

        int batPct = VOXA::BatteryManager::instance().getPercentage();
        if (batPct <= 0 || batPct > 100) batPct = 94;
        bool isCharging = VOXA::BatteryManager::instance().isCharging();

        // Capsule Shell (Crisp White inside Dynamic Island)
        canvas.drawRoundRect(batX, batY, batW, batH, 2, VoxaTheme::getDynamicIslandFg());
        canvas.fillRect(batX + batW, batY + 2, 2, batH - 4, VoxaTheme::getDynamicIslandFg());

        // Fill Level Bar
        int maxFillW = batW - 4;
        int fillW = std::max(2, (int)(maxFillW * (batPct / 100.0f)));
        uint16_t batFillCol = isCharging ? VoxaTheme::getSystemAmber() 
            : (batPct < 20 ? VoxaTheme::getSystemRed() : VoxaTheme::getSystemGreen());
        canvas.fillRoundRect(batX + 2, batY + 2, fillW, batH - 4, 1, batFillCol);
    }

    void renderPageDots(LovyanGFX& canvas, int activeIndex, int count, uint16_t w, uint16_t h)
    {
        float dotY = h - 12.0f;
        float spacing = 14.0f;
        float centerX = w * 0.5f;
        float startX = centerX - ((count - 1) * spacing) * 0.5f;

        for (int i = 0; i < count; ++i)
        {
            bool active = (i == activeIndex);
            uint16_t color = active ? VoxaTheme::getPrimary() : VoxaTheme::getGlassBorder();
            int dotW = active ? 16 : 6;
            int dotH = 5;
            canvas.fillRoundRect((int)(startX + i * spacing - dotW * 0.5f), (int)(dotY - dotH * 0.5f), dotW, dotH, 2, color);
            if (active)
            {
                // Subtle glow on active pill
                canvas.drawRoundRect((int)(startX + i * spacing - dotW * 0.5f), (int)(dotY - dotH * 0.5f), dotW, dotH, 2, VoxaTheme::getPrimaryLight());
            }
        }
    }

    void renderCircularButton(LovyanGFX& canvas, float centerX, float centerY, Icon icon, 
                              uint16_t fill, uint16_t iconColor, uint16_t w, uint16_t h)
    {
        float radius = 15.0f;
        int cx = (int)centerX;
        int cy = (int)centerY;
        int r = (int)radius;

        // Frosted Glass Circle Body
        canvas.fillCircle(cx, cy, r, fill);
        canvas.drawCircle(cx, cy, r, VoxaTheme::getGlassBorder());

        // Optical Glass Reflection Rim
        canvas.drawCircle(cx, cy - 1, r - 2, VoxaTheme::getGlassHighlight());

        // Centered Geometric Icon
        drawIcon(canvas, icon, centerX - 6.0f, centerY - 6.0f, 12.0f, iconColor);
    }

    void renderHeader(LovyanGFX& canvas, const std::string& title, bool showBack, 
                      bool showRightAction, Icon rightIcon, uint16_t w, uint16_t h)
    {
        if (showBack)
        {
            renderCircularButton(canvas, 22.0f, 46.0f, Icon::Back, VoxaTheme::getGlassSurface(), VoxaTheme::getTextPrimary(), w, h);
        }

        canvas.setFont(&fonts::FreeSansBold12pt7b);
        canvas.setTextSize(1);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        canvas.drawString(title.c_str(), w * 0.5f, 46.0f);

        if (showRightAction)
        {
            renderCircularButton(canvas, w - 22.0f, 46.0f, rightIcon, VoxaTheme::getGlassSurface(), VoxaTheme::getTextPrimary(), w, h);
        }
    }

    void drawGlassCard(LovyanGFX& canvas, float x, float y, float w, float h, float radius, 
                       bool isPressed, uint16_t accentColor)
    {
        int ix = (int)x;
        int iy = (int)y;
        int iw = (int)w;
        int ih = (int)h;
        int ir = (int)radius;

        uint16_t fillCol = isPressed 
            ? (accentColor != 0 ? accentColor : VoxaTheme::getPrimary()) 
            : VoxaTheme::getGlassSurface();
        uint16_t borderCol = isPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getGlassBorder();
        uint16_t shineCol = isPressed ? 0xFFFF : VoxaTheme::getGlassHighlight();

        // 1. Frosted Glass Body
        canvas.fillRoundRect(ix, iy, iw, ih, ir, fillCol);

        // 2. Translucent Glass Border
        canvas.drawRoundRect(ix, iy, iw, ih, ir, borderCol);

        // 3. Specular Optical Highlight along Top Edge (iOS 26 Refraction Line)
        if (!isPressed && iw > ir * 2 + 4)
        {
            canvas.drawFastHLine(ix + ir + 2, iy + 1, iw - (ir * 2 + 4), shineCol);
        }
    }

        void drawIcon(LovyanGFX& canvas, Icon icon, float x, float y, float size, uint16_t color)
        {
            // ── Bitmap dispatch (smartwatch-style PROGMEM bitmaps) ────────────
            // For the 8 status-bar / small icons we keep vector drawing.
            // For all menu icons we blit pre-rasterized 20x20 bitmaps.
            const uint8_t* bmp = nullptr;
            bool isBitmapIcon = true;

            switch (icon)
            {
                case Icon::Bell:          bmp = ICON_BMP_BELL;          break;
                case Icon::Lightbulb:     bmp = ICON_BMP_LIGHTBULB;     break;
                case Icon::Question:      bmp = ICON_BMP_QUESTION;      break;
                case Icon::Folder:        bmp = ICON_BMP_FOLDER;        break;
                case Icon::Settings:      bmp = ICON_BMP_GEAR;          break;
                case Icon::Search:        bmp = ICON_BMP_SEARCH;        break;
                case Icon::Mic:           bmp = ICON_BMP_MIC;           break;
                case Icon::Note:          bmp = ICON_BMP_NOTE;          break;
                case Icon::Wifi:          bmp = ICON_BMP_WIFI;          break;
                case Icon::WiFiOff:       bmp = ICON_BMP_WIFI_OFF;      break;
                case Icon::Cloud:         bmp = ICON_BMP_CLOUD;         break;
                case Icon::Rotate:        bmp = ICON_BMP_ROTATE;        break;
                case Icon::Power:         bmp = ICON_BMP_POWER;         break;
                case Icon::Reset:         bmp = ICON_BMP_RESET;         break;
                case Icon::Bluetooth:     bmp = ICON_BMP_BLUETOOTH;     break;
                case Icon::Volume:        bmp = ICON_BMP_VOLUME;        break;
                case Icon::Sun:           bmp = ICON_BMP_SUN;           break;
                case Icon::Moon:          bmp = ICON_BMP_MOON;          break;
                case Icon::Play:          bmp = ICON_BMP_PLAY;          break;


                case Icon::Pause:         bmp = ICON_BMP_PAUSE;         break;
                case Icon::Star:          bmp = ICON_BMP_STAR;          break;
                case Icon::Upload:        bmp = ICON_BMP_UPLOAD;        break;
                case Icon::Filter:        bmp = ICON_BMP_FILTER;        break;
                case Icon::Info:          bmp = ICON_BMP_INFO;          break;
                case Icon::Storage:       bmp = ICON_BMP_STORAGE;       break;
                case Icon::Calendar:      bmp = ICON_BMP_CALENDAR;      break;
                case Icon::Chat:          bmp = ICON_BMP_CHAT;          break;
                case Icon::Spark:         bmp = ICON_BMP_SPARK;         break;
                case Icon::ChevronRight:  bmp = ICON_BMP_CHEVRON_RIGHT; break;
                case Icon::Back:          bmp = ICON_BMP_CHEVRON_LEFT;  break;
                default: isBitmapIcon = false; break;
            }

            if (isBitmapIcon && bmp != nullptr)
            {
                // Bitmap is always 20x20. If requested size != 20, centre it.
                int bmpSz = ICON_BMP_SIZE;
                int ix = (int)(x + (size - bmpSz) * 0.5f);
                int iy = (int)(y + (size - bmpSz) * 0.5f);
                canvas.drawBitmap(ix, iy, bmp, bmpSz, bmpSz, color);
                return;
            }

            // ── Fallback vector for Battery & Plus ─────────────────────────
            float cx = x + size * 0.5f;
            float cy = y + size * 0.5f;
            int th = std::max(2, (int)(size * 0.13f));

            switch (icon)
            {
                case Icon::Battery:
                {
                    int bx = (int)(x + size*0.04f), by = (int)(y + size*0.28f);
                    int bw = (int)(size*0.78f), bh = (int)(size*0.44f);
                    canvas.fillRoundRect(bx, by, bw, bh, 2, color);
                    canvas.fillRect(bx + bw, by + bh/4, (int)(size*0.10f), bh/2, color);
                    canvas.fillRoundRect(bx + 2, by + 2, (int)((bw-4)*0.80f), bh - 4, 1, VoxaTheme::getBackground());
                }
                break;

                case Icon::Plus:
                    canvas.fillRect((int)(cx - th/2), (int)(y + size*0.12f), th, (int)(size*0.76f), color);
                    canvas.fillRect((int)(x + size*0.12f), (int)(cy - th/2), (int)(size*0.76f), th, color);
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
