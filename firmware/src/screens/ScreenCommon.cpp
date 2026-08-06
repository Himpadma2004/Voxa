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

        // 1. Draw minimal obsidian pitch black background
        canvas.fillScreen(VoxaTheme::getBackground());

        // 2. Subtle warm orange ambient aura top-right (blended with background)
        float tr_cx = w * 0.88f;
        float tr_cy = h * 0.12f;
        uint16_t primaryOrange = VoxaTheme::getPrimary();
        uint8_t oR = (primaryOrange >> 11) << 3;
        uint8_t oG = ((primaryOrange >> 5) & 0x3F) << 2;
        uint8_t oB = (primaryOrange & 0x1F) << 3;

        uint16_t bg = VoxaTheme::getBackground();
        uint8_t bgR = (bg >> 11) << 3;
        uint8_t bgG = ((bg >> 5) & 0x3F) << 2;
        uint8_t bgB = (bg & 0x1F) << 3;

        for (int r = 5; r >= 1; --r)
        {
            float radius = 20.0f + r * 10.0f;
            float alpha = (6.0f - r) * 0.02f;
            
            uint8_t blend_r = (uint8_t)((1.0f - alpha) * bgR + alpha * oR);
            uint8_t blend_g = (uint8_t)((1.0f - alpha) * bgG + alpha * oG);
            uint8_t blend_b = (uint8_t)((1.0f - alpha) * bgB + alpha * oB);
            
            canvas.fillCircle((int)tr_cx, (int)tr_cy, (int)radius, canvas.color565(blend_r, blend_g, blend_b));
        }

        // 3. Compact status bar at the top
        canvas.setFont(&fonts::FreeSans9pt7b);
        canvas.setTextSize(1);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        
        canvas.setTextDatum(textdatum_t::top_left);
        canvas.drawString("VOXA", 10, 4);

        if (VOXA::microphoneService.isRecording())
        {
            uint16_t dotColor = ((millis() / 500) % 2 == 0) ? canvas.color565(255, 60, 0) : canvas.color565(120, 20, 0);
            canvas.fillCircle(65, 11, 4, dotColor);
        }

        canvas.setTextDatum(textdatum_t::top_center);
        canvas.drawString(getCurrentTimeStr().c_str(), w * 0.5f, 4);

        // Interactive Wi-Fi Icon & Non-Overlapping Battery Info
        bool isWifiConnected = VOXA::wifiManager.isConnected();
        uint16_t wifiColor = isWifiConnected ? VoxaTheme::getTextPrimary() : VoxaTheme::getDivider();
        drawIcon(canvas, isWifiConnected ? Icon::Wifi : Icon::WiFiOff, w - 74, 4, 12, wifiColor);
        
        int batPct = VOXA::BatteryManager::instance().getPercentage();
        if (batPct <= 0 || batPct > 100) batPct = 92;
        std::string batStr = std::to_string(batPct) + "%";

        canvas.setFont(&fonts::Font0);
        canvas.setTextSize(1);
        canvas.setTextDatum(textdatum_t::top_right);
        canvas.drawString(batStr.c_str(), w - 24, 7);
        
        uint16_t batColor = VOXA::BatteryManager::instance().isCharging() ? VoxaTheme::getPrimary() : VoxaTheme::getTextPrimary();
        drawIcon(canvas, Icon::Battery, w - 18, 4, 14, batColor);
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
            if (showBack)
            {
                renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, VoxaTheme::getSurface(), VoxaTheme::getTextPrimary(), w, h);
            }

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
