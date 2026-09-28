#include "HomeScreen.h"
#include "../ui/Theme.h"
#include "Transition.h"
#include "../services/ApiClient.h"
#include "../services/ReminderService.h"
#include "../services/IdeaService.h"
#include "../services/QuestionService.h"
#include "../services/RecordingService.h"
#include "../services/DataService.h"
#include "../ui/QuickPanel.h"
#include "../audio/AudioManager.h"
#include "../services/ButtonService.h"
#include "../services/PowerManager.h"
#include "../services/BatteryManager.h"

#include <array>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace VOXA
{
    extern ReminderService reminderService;
    extern IdeaService ideaService;
    extern QuestionService questionService;
    extern RecordingService recordingService;
    extern DataService dataService;

    static std::string truncateFit(LovyanGFX& c, const std::string& text, int maxW)
    {
        if (text.empty()) return "";
        if (c.textWidth(text.c_str()) <= maxW) return text;
        std::string res = text;
        while (!res.empty() && c.textWidth((res + "..").c_str()) > maxW)
        {
            res.pop_back();
        }
        return res + "..";
    }

    HomeScreen::HomeScreen()
    {
    }

    void HomeScreen::begin()
    {
    }

    void HomeScreen::renderPage0(LovyanGFX& canvas, uint16_t w, uint16_t h, float offsetX,
                                 int remCount, int ideaCount, int qCount, int taskCount, int memCount)
    {
        std::time_t tNow = std::time(nullptr);
        std::tm local_tm;
#if defined(_MSC_VER)
        localtime_s(&local_tm, &tNow);
#else
        localtime_r(&tNow, &local_tm);
#endif

        char timeBuf[16];
        if (local_tm.tm_year > 100)
        {
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", local_tm.tm_hour, local_tm.tm_min);
        }
        else
        {
            snprintf(timeBuf, sizeof(timeBuf), "10:42");
        }

        // 1. Top Status Bar: Time (Left), WiFi + Battery + Flash (Right)
        canvas.setFont(&fonts::DejaVu9);
        canvas.setTextDatum(textdatum_t::top_left);
        canvas.setTextColor(0xFDC0);
        canvas.drawString(timeBuf, 14.0f + offsetX, 10.0f);

        float statRight = w - 14.0f + offsetX;

        // WiFi Icon
        int wx = (int)(statRight - 54.0f);
        int wy = 12;
        canvas.drawCircle(wx, wy + 5, 6, 0x3DFE);
        canvas.drawCircle(wx, wy + 5, 3, 0x3DFE);
        canvas.fillCircle(wx, wy + 5, 1, 0x3DFE);
        canvas.fillRect(wx - 8, wy + 6, 16, 7, 0x0000);

        // Battery percentage
        int batPct = BatteryManager::instance().getPercentage();
        if (batPct <= 0 || batPct > 100) batPct = 94;
        char batBuf[16];
        snprintf(batBuf, sizeof(batBuf), "%d%%", batPct);

        canvas.setFont(&fonts::DejaVu9);
        canvas.setTextDatum(textdatum_t::middle_left);
        canvas.setTextColor(0xFFFF);
        canvas.drawString(batBuf, statRight - 36.0f, 15.0f);

        // Lightning bolt icon (Amber)
        int lx = (int)(statRight - 6.0f);
        int ly = 10;
        canvas.fillTriangle(lx, ly, lx - 4, ly + 6, lx, ly + 6, 0xFDC0);
        canvas.fillTriangle(lx, ly + 5, lx - 3, ly + 13, lx + 1, ly + 5, 0xFDC0);

        // 2. Greeting / Big Clock / Date
        int hour = local_tm.tm_hour;
        const char* greeting = "GOOD EVENING";
        if (hour >= 5 && hour < 12) greeting = "GOOD MORNING";
        else if (hour >= 12 && hour < 17) greeting = "GOOD AFTERNOON";
        else if (hour >= 17 && hour < 22) greeting = "GOOD EVENING";
        else greeting = "GOOD NIGHT";

        // Greeting Text
        canvas.setFont(&fonts::DejaVu9);
        canvas.setTextDatum(textdatum_t::top_left);
        canvas.setTextColor(0x94A3B8);
        canvas.drawString(greeting, 14.0f + offsetX, 32.0f);

        // Big Clock Time
        canvas.setFont(&fonts::FreeSansBold24pt7b);
        canvas.setTextDatum(textdatum_t::top_left);
        canvas.setTextColor(0xFFFF);
        canvas.drawString(timeBuf, 14.0f + offsetX, 46.0f);

        // Date (e.g. Thursday, Oct 24)
        static const char* const s_days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
        static const char* const s_months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

        char dateBuf[64];
        if (local_tm.tm_year > 100)
        {
            snprintf(dateBuf, sizeof(dateBuf), "%s, %s %d",
                     s_days[local_tm.tm_wday], s_months[local_tm.tm_mon], local_tm.tm_mday);
        }
        else
        {
            snprintf(dateBuf, sizeof(dateBuf), "Thursday, Oct 24");
        }

        canvas.setFont(&fonts::DejaVu9);
        canvas.setTextDatum(textdatum_t::top_left);
        canvas.setTextColor(0xCBD5E1);
        canvas.drawString(dateBuf, 14.0f + offsetX, 92.0f);

        // 3. Middle Sound Waveform Visualizer (7 vertical rounded bars)
        int waveCx = (int)(w * 0.5f + offsetX);
        int waveCy = 120;
        int barHeights[] = { 6, 12, 20, 26, 20, 12, 6 };
        for (int b = 0; b < 7; ++b)
        {
            int bx = waveCx - 18 + b * 6;
            int bh = barHeights[b];
            canvas.fillRoundRect(bx, waveCy - bh / 2, 3, bh, 1, (b == 3) ? canvas.color565(140, 160, 185) : canvas.color565(80, 95, 115));
        }

        // 4. 2x2 Quick Metrics Hub Tiles
        float tileW = 106.0f;
        float tileH = 28.0f;

        // Tile 0: TASKS (Top-Left)
        float t0X = 12.0f + offsetX;
        float t0Y = 146.0f;
        bool isT0Pressed = (m_pressedCardIndex == 10);
        canvas.fillRoundRect((int)t0X, (int)t0Y, (int)tileW, (int)tileH, 6, isT0Pressed ? canvas.color565(32, 36, 48) : canvas.color565(18, 20, 26));
        canvas.drawRoundRect((int)t0X, (int)t0Y, (int)tileW, (int)tileH, 6, isT0Pressed ? 0x3DFE : canvas.color565(34, 38, 48));

        // Checkmark circle
        canvas.drawCircle((int)(t0X + 12.0f), (int)(t0Y + 14.0f), 5, 0x3DFE);
        canvas.drawLine((int)(t0X + 10.0f), (int)(t0Y + 14.0f), (int)(t0X + 12.0f), (int)(t0Y + 16.0f), 0x3DFE);
        canvas.drawLine((int)(t0X + 12.0f), (int)(t0Y + 16.0f), (int)(t0X + 15.0f), (int)(t0Y + 11.0f), 0x3DFE);

        canvas.setFont(&fonts::DejaVu9);
        canvas.setTextDatum(textdatum_t::middle_left);
        canvas.setTextColor(0xCBD5E1);
        canvas.drawString("TASKS", t0X + 22.0f, t0Y + 14.0f);

        canvas.setTextDatum(textdatum_t::middle_right);
        canvas.setTextColor(0xFFFF);
        char cBuf[16];
        snprintf(cBuf, sizeof(cBuf), "%d", taskCount > 0 ? taskCount : 12);
        canvas.drawString(cBuf, t0X + tileW - 8.0f, t0Y + 14.0f);

        // Tile 1: IDEAS (Top-Right)
        float t1X = 122.0f + offsetX;
        float t1Y = 146.0f;
        bool isT1Pressed = (m_pressedCardIndex == 11);
        canvas.fillRoundRect((int)t1X, (int)t1Y, (int)tileW, (int)tileH, 6, isT1Pressed ? canvas.color565(32, 36, 48) : canvas.color565(18, 20, 26));
        canvas.drawRoundRect((int)t1X, (int)t1Y, (int)tileW, (int)tileH, 6, isT1Pressed ? 0xFDC0 : canvas.color565(34, 38, 48));

        // Lightbulb
        canvas.fillCircle((int)(t1X + 12.0f), (int)(t1Y + 12.0f), 3, 0xFDC0);
        canvas.fillRect((int)(t1X + 11.0f), (int)(t1Y + 15.0f), 2, 2, 0xFDC0);

        canvas.setTextDatum(textdatum_t::middle_left);
        canvas.setTextColor(0xCBD5E1);
        canvas.drawString("IDEAS", t1X + 22.0f, t1Y + 14.0f);

        canvas.setTextDatum(textdatum_t::middle_right);
        canvas.setTextColor(0xFDC0);
        snprintf(cBuf, sizeof(cBuf), "%d", ideaCount > 0 ? ideaCount : 28);
        canvas.drawString(cBuf, t1X + tileW - 8.0f, t1Y + 14.0f);

        // Tile 2: MEMOS (Bottom-Left)
        float t2X = 12.0f + offsetX;
        float t2Y = 178.0f;
        bool isT2Pressed = (m_pressedCardIndex == 12);
        canvas.fillRoundRect((int)t2X, (int)t2Y, (int)tileW, (int)tileH, 6, isT2Pressed ? canvas.color565(32, 36, 48) : canvas.color565(18, 20, 26));
        canvas.drawRoundRect((int)t2X, (int)t2Y, (int)tileW, (int)tileH, 6, isT2Pressed ? 0x3DFE : canvas.color565(34, 38, 48));

        // Cassette / loops
        canvas.drawCircle((int)(t2X + 10.0f), (int)(t2Y + 14.0f), 3, 0x3DFE);
        canvas.drawCircle((int)(t2X + 15.0f), (int)(t2Y + 14.0f), 3, 0x3DFE);
        canvas.drawLine((int)(t2X + 10.0f), (int)(t2Y + 17.0f), (int)(t2X + 15.0f), (int)(t2Y + 17.0f), 0x3DFE);

        canvas.setTextDatum(textdatum_t::middle_left);
        canvas.setTextColor(0xCBD5E1);
        canvas.drawString("MEMOS", t2X + 22.0f, t2Y + 14.0f);

        canvas.setTextDatum(textdatum_t::middle_right);
        canvas.setTextColor(0xFFFF);
        snprintf(cBuf, sizeof(cBuf), "%d", memCount > 0 ? memCount : 42);
        canvas.drawString(cBuf, t2X + tileW - 8.0f, t2Y + 14.0f);

        // Tile 3: OTHERS (Bottom-Right)
        float t3X = 122.0f + offsetX;
        float t3Y = 178.0f;
        bool isT3Pressed = (m_pressedCardIndex == 13);
        canvas.fillRoundRect((int)t3X, (int)t3Y, (int)tileW, (int)tileH, 6, isT3Pressed ? canvas.color565(32, 36, 48) : canvas.color565(18, 20, 26));
        canvas.drawRoundRect((int)t3X, (int)t3Y, (int)tileW, (int)tileH, 6, isT3Pressed ? 0xFDC0 : canvas.color565(34, 38, 48));

        // Folder outline icon
        canvas.drawRoundRect((int)(t3X + 8.0f), (int)(t3Y + 11.0f), 8, 7, 1, 0xFDC0);
        canvas.drawFastHLine((int)(t3X + 9.0f), (int)(t3Y + 10.0f), 4, 0xFDC0);

        canvas.setTextDatum(textdatum_t::middle_left);
        canvas.setTextColor(0xCBD5E1);
        canvas.drawString("OTHERS", t3X + 19.0f, t3Y + 14.0f);

        canvas.setTextDatum(textdatum_t::middle_right);
        canvas.setTextColor(0x3DFE);
        canvas.drawString("ALL", t3X + tileW - 8.0f, t3Y + 14.0f);

        // 5. Bottom Solid White Button: "Record Voice" (Y = 222, H = 42)
        float btnX = 12.0f + offsetX;
        float btnY = 222.0f;
        float btnFullW = w - 24.0f;
        bool isRecPressed = (m_pressedCardIndex == 0);

        canvas.fillRoundRect((int)btnX, (int)btnY, (int)btnFullW, 42, 8, isRecPressed ? canvas.color565(210, 215, 225) : 0xFFFF);

        // Calculate unified centering for icon + text
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        int textW = canvas.textWidth("Record Voice");
        float totalW = 13.0f + 10.0f + (float)textW; // 13px mic + 10px gap + text
        float startX = btnX + (btnFullW - totalW) * 0.5f;
        float micCx = startX + 6.0f;
        float micCy = btnY + 21.0f;

        // Clean studio mic capsule body (Black on White button)
        canvas.fillRoundRect((int)micCx - 3, (int)micCy - 8, 7, 13, 3, 0x0000);
        // Studio mic horizontal grille slit
        canvas.drawFastHLine((int)micCx - 2, (int)micCy - 2, 5, 0xFFFF);
        // U-cradle arc around lower half
        canvas.drawArc((int)micCx, (int)micCy, 6, 6, 0.0f, 180.0f, 0x0000);
        canvas.drawFastVLine((int)micCx - 6, (int)micCy - 4, 5, 0x0000);
        canvas.drawFastVLine((int)micCx + 6, (int)micCy - 4, 5, 0x0000);
        // Stem + base foot
        canvas.drawFastVLine((int)micCx, (int)micCy + 6, 4, 0x0000);
        canvas.drawFastHLine((int)micCx - 4, (int)micCy + 10, 9, 0x0000);

        // Text: Record Voice
        canvas.setTextDatum(textdatum_t::middle_left);
        canvas.setTextColor(0x0000);
        canvas.drawString("Record Voice", micCx + 17.0f, micCy);

        // 6. Footer: "VOXA: We take care your momemts" (Y = 296)
        canvas.setFont(&fonts::DejaVu9);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(0x64748B);
        canvas.drawString("VOXA: We take care your momemts", w * 0.5f + offsetX, 296.0f);
    }

    void HomeScreen::renderPage1(LovyanGFX& canvas, uint16_t w, uint16_t h,
                                 int remCount, int ideaCount, int qCount, int taskCount, int memCount, float offsetX,
                                 float animT)
    {
        // 1. Top Status Bar: Small crisp Font0 (Time, GRID, Battery)
        std::time_t tNow = std::time(nullptr);
        std::tm local_tm;
#if defined(_MSC_VER)
        localtime_s(&local_tm, &tNow);
#else
        localtime_r(&tNow, &local_tm);
#endif

        char timeBuf[16];
        if (local_tm.tm_year > 100)
        {
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", local_tm.tm_hour, local_tm.tm_min);
        }
        else
        {
            snprintf(timeBuf, sizeof(timeBuf), "10:42");
        }

        // Left Time
        canvas.setFont(&fonts::DejaVu9);
        canvas.setTextDatum(textdatum_t::top_left);
        canvas.setTextColor(0xFDC0);
        canvas.drawString(timeBuf, 18.0f + offsetX, 12.0f);

        // Center Title: GRID in clean subtle white
        canvas.setTextDatum(textdatum_t::top_center);
        canvas.setTextColor(0xFFFF);
        canvas.drawString("GRID", w * 0.5f + offsetX, 12.0f);

        // Right Battery
        int batPct = BatteryManager::instance().getPercentage();
        if (batPct <= 0 || batPct > 100) batPct = 84;
        char batBuf[16];
        snprintf(batBuf, sizeof(batBuf), "%d%%", batPct);

        canvas.setTextDatum(textdatum_t::top_right);
        canvas.setTextColor(0xFFFF);
        canvas.drawString(batBuf, w - 18.0f + offsetX, 12.0f);

        // 2. Page Header: Quick Apps (With generous top and bottom black space)
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextDatum(textdatum_t::top_left);
        canvas.setTextColor(0xFFFF);
        canvas.drawString("Quick Apps", 18.0f + offsetX, 38.0f);

        // 3. Grid Coordinates (3 columns x 3 rows evenly spaced)
        bool isLandscape = (w > h);
        float colCenters[3] = { w * 0.20f, w * 0.50f, w * 0.80f };
        float rowCenters[3] = {
            isLandscape ? 72.0f : 96.0f,
            isLandscape ? 130.0f : 172.0f,
            isLandscape ? 188.0f : 246.0f
        };

        // Subtitle counts
        char taskSub[16]; snprintf(taskSub, sizeof(taskSub), "%d", (taskCount > 0 ? taskCount : 12));
        char remSub[16];  snprintf(remSub, sizeof(remSub), "%d", (remCount > 0 ? remCount : 3));
        char ideaSub[16]; snprintf(ideaSub, sizeof(ideaSub), "%d", (ideaCount > 0 ? ideaCount : 28));
        char qSub[16];    snprintf(qSub, sizeof(qSub), "%d", (qCount > 0 ? qCount : 5));
        char recSub[16];  snprintf(recSub, sizeof(recSub), "%d", (memCount > 0 ? memCount : 39));

        struct GridItemDef
        {
            const char* label;
            const char* sub;
            uint16_t iconColor;
            uint16_t subColor;
        };

        GridItemDef items[9] = {
            // Row 0
            { "Tasks",    taskSub,  canvas.color565(100, 195, 255), 0xFFFF },
            { "Remind",   remSub,   canvas.color565(250, 240, 220), canvas.color565(250, 240, 220) },
            { "Ideas",    ideaSub,  canvas.color565(210, 160, 255), canvas.color565(210, 160, 255) },
            // Row 1
            { "Question", qSub,     0xFFFF,                         canvas.color565(120, 220, 245) },
            { "Music",    "HQ",     canvas.color565(80, 240, 170),  canvas.color565(80, 240, 170) },
            { "Record",   recSub,   canvas.color565(255, 170, 120), canvas.color565(255, 170, 120) },
            // Row 2
            { "Search",   "",       canvas.color565(90, 185, 255),  0xFFFF },
            { "Others",   "",       canvas.color565(110, 210, 255), 0xFFFF },
            { "Config",   "",       0xFFFF,                         0xFFFF }
        };

        for (int i = 0; i < 9; ++i)
        {
            int col = i % 3;
            int row = i / 3;
            float cx = colCenters[col] + offsetX;

            // Staggered slide-in from bottom: item i starts after i*55ms, 320ms quartic ease-out
            const float STAGGER = 0.055f;
            const float DUR     = 0.32f;
            float itemT = std::max(0.0f, std::min(1.0f, (animT - i * STAGGER) / DUR));
            float ease  = 1.0f - (1.0f - itemT) * (1.0f - itemT) * (1.0f - itemT) * (1.0f - itemT);
            // Tiny landing bounce in the last 15% of travel
            float bounce = 0.0f;
            if (itemT > 0.85f) {
                float bt = (itemT - 0.85f) / 0.15f;
                bounce = -5.0f * std::sin(bt * 3.14159f) * (1.0f - bt);
            }
            float slideY = (float)h * (1.0f - ease) + bounce;
            float cy = rowCenters[row] + slideY;
            if (cy > (float)h + 30.0f) continue;  // still offscreen, skip

            bool isPressed = (m_pressedItemIndex == i);

            // Tactile selection feedback (sleek glowing saucer cradle curve like reference Image 2)
            if (isPressed)
            {
                // Glowing saucer cradle arc below app
                canvas.drawCircle((int)cx, (int)(cy + 8.0f), 24, items[i].iconColor);
                canvas.fillRect((int)(cx - 26.0f), (int)(cy - 20.0f), 52, 28, 0x0000); // mask top half
                // Subtle highlight pill
                canvas.fillRoundRect((int)(cx - 26.0f), (int)(cy - 24.0f), 52, 48, 8, canvas.color565(18, 22, 32));
            }

            // Draw Pixel-Perfect Vector Icons Matching Image 2
            uint16_t col565 = items[i].iconColor;
            switch (i)
            {
                case 0: // Tasks: Checkbox with Checkmark inside
                {
                    canvas.drawRoundRect((int)(cx - 9.0f), (int)(cy - 23.0f), 18, 17, 3, col565);
                    // Checkmark
                    canvas.drawLine((int)(cx - 5.0f), (int)(cy - 15.0f), (int)(cx - 2.0f), (int)(cy - 12.0f), col565);
                    canvas.drawLine((int)(cx - 4.0f), (int)(cy - 15.0f), (int)(cx - 2.0f), (int)(cy - 13.0f), col565);
                    canvas.drawLine((int)(cx - 2.0f), (int)(cy - 12.0f), (int)(cx + 5.0f), (int)(cy - 19.0f), col565);
                    canvas.drawLine((int)(cx - 2.0f), (int)(cy - 13.0f), (int)(cx + 5.0f), (int)(cy - 20.0f), col565);
                    break;
                }
                case 1: // Remind: Sleek Bell Outline
                {
                    // Bell top ring
                    canvas.drawCircle((int)cx, (int)(cy - 24.0f), 2, col565);
                    // Bell dome
                    canvas.drawCircle((int)cx, (int)(cy - 18.0f), 6, col565);
                    canvas.fillRect((int)(cx - 7.0f), (int)(cy - 15.0f), 14, 6, 0x0000);
                    // Bell flared sides & base
                    canvas.drawLine((int)(cx - 6.0f), (int)(cy - 18.0f), (int)(cx - 8.0f), (int)(cy - 12.0f), col565);
                    canvas.drawLine((int)(cx + 6.0f), (int)(cy - 18.0f), (int)(cx + 8.0f), (int)(cy - 12.0f), col565);
                    canvas.drawLine((int)(cx - 8.0f), (int)(cy - 12.0f), (int)(cx + 8.0f), (int)(cy - 12.0f), col565);
                    // Ringer
                    canvas.fillCircle((int)cx, (int)(cy - 10.0f), 2, col565);
                    break;
                }
                case 2: // Ideas: Lightbulb Outline with screw base
                {
                    // Bulb dome
                    canvas.drawCircle((int)cx, (int)(cy - 19.0f), 7, col565);
                    canvas.fillRect((int)(cx - 4.0f), (int)(cy - 13.0f), 8, 3, 0x0000);
                    // Base screw threads
                    canvas.drawLine((int)(cx - 3.0f), (int)(cy - 13.0f), (int)(cx + 3.0f), (int)(cy - 13.0f), col565);
                    canvas.drawLine((int)(cx - 3.0f), (int)(cy - 11.0f), (int)(cx + 3.0f), (int)(cy - 11.0f), col565);
                    canvas.drawLine((int)(cx - 2.0f), (int)(cy - 9.0f),  (int)(cx + 2.0f), (int)(cy - 9.0f),  col565);
                    break;
                }
                case 3: // Question: Chat Speech Bubble with inner '?'
                {
                    canvas.drawRoundRect((int)(cx - 9.0f), (int)(cy - 23.0f), 18, 14, 3, col565);
                    canvas.fillRect((int)(cx - 7.0f), (int)(cy - 10.0f), 5, 2, 0x0000); // notch for tail
                    canvas.drawLine((int)(cx - 7.0f), (int)(cy - 10.0f), (int)(cx - 9.0f), (int)(cy - 6.0f), col565);
                    canvas.drawLine((int)(cx - 9.0f), (int)(cy - 6.0f),  (int)(cx - 3.0f), (int)(cy - 10.0f), col565);
                    break;
                }
                case 4: // Music: Equalizer Vertical Wave Bars (5 bars)
                {
                    canvas.fillRoundRect((int)(cx - 8.0f), (int)(cy - 18.0f), 2, 8,  1, col565);
                    canvas.fillRoundRect((int)(cx - 4.0f), (int)(cy - 23.0f), 2, 17, 1, col565);
                    canvas.fillRoundRect((int)cx,          (int)(cy - 20.0f), 2, 12, 1, col565);
                    canvas.fillRoundRect((int)(cx + 4.0f), (int)(cy - 25.0f), 2, 21, 1, col565);
                    canvas.fillRoundRect((int)(cx + 8.0f), (int)(cy - 17.0f), 2, 6,  1, col565);
                    break;
                }
                case 5: // Record: Studio Microphone Outline
                {
                    // Center solid capsule
                    canvas.fillRoundRect((int)cx - 3, (int)cy - 26, 7, 13, 3, col565);
                    // Studio mic horizontal grille slit
                    canvas.drawFastHLine((int)cx - 2, (int)cy - 20, 5, 0x0000);
                    // U-shaped Cradle arc around lower half of capsule
                    canvas.drawArc((int)cx, (int)cy - 18, 6, 6, 0.0f, 180.0f, col565);
                    // Cradle vertical arms
                    canvas.drawFastVLine((int)cx - 6, (int)cy - 22, 5, col565);
                    canvas.drawFastVLine((int)cx + 6, (int)cy - 22, 5, col565);
                    // Stem & base foot
                    canvas.drawFastVLine((int)cx, (int)cy - 12, 4, col565);
                    canvas.drawFastHLine((int)cx - 4, (int)cy - 8, 9, col565);
                    break;
                }
                case 6: // Search: Magnifying Glass
                {
                    canvas.drawCircle((int)(cx - 2.0f), (int)(cy - 19.0f), 6, col565);
                    canvas.drawLine((int)(cx + 3.0f), (int)(cy - 14.0f), (int)(cx + 8.0f), (int)(cy - 9.0f), col565);
                    canvas.drawLine((int)(cx + 4.0f), (int)(cy - 14.0f), (int)(cx + 9.0f), (int)(cy - 9.0f), col565);
                    break;
                }
                case 7: // Others: Folder Outline
                {
                    // Tab top
                    canvas.drawLine((int)(cx - 9.0f), (int)(cy - 21.0f), (int)(cx - 9.0f), (int)(cy - 24.0f), col565);
                    canvas.drawLine((int)(cx - 9.0f), (int)(cy - 24.0f), (int)(cx - 4.0f), (int)(cy - 24.0f), col565);
                    canvas.drawLine((int)(cx - 4.0f), (int)(cy - 24.0f), (int)(cx - 2.0f), (int)(cy - 21.0f), col565);
                    // Folder body
                    canvas.drawRoundRect((int)(cx - 9.0f), (int)(cy - 21.0f), 18, 14, 2, col565);
                    break;
                }
                case 8: // Config: Gear / Cog Outline
                {
                    canvas.drawCircle((int)cx, (int)(cy - 17.0f), 6, col565);
                    canvas.drawCircle((int)cx, (int)(cy - 17.0f), 2, col565);
                    // Teeth
                    canvas.drawLine((int)cx, (int)(cy - 24.0f), (int)cx, (int)(cy - 22.0f), col565);
                    canvas.drawLine((int)cx, (int)(cy - 12.0f), (int)cx, (int)(cy - 10.0f), col565);
                    canvas.drawLine((int)(cx - 7.0f), (int)(cy - 17.0f), (int)(cx - 5.0f), (int)(cy - 17.0f), col565);
                    canvas.drawLine((int)(cx + 5.0f), (int)(cy - 17.0f), (int)(cx + 7.0f), (int)(cy - 17.0f), col565);
                    canvas.drawLine((int)(cx - 5.0f), (int)(cy - 22.0f), (int)(cx - 4.0f), (int)(cy - 21.0f), col565);
                    canvas.drawLine((int)(cx + 4.0f), (int)(cy - 13.0f), (int)(cx + 5.0f), (int)(cy - 12.0f), col565);
                    canvas.drawLine((int)(cx + 4.0f), (int)(cy - 21.0f), (int)(cx + 5.0f), (int)(cy - 22.0f), col565);
                    canvas.drawLine((int)(cx - 5.0f), (int)(cy - 12.0f), (int)(cx - 4.0f), (int)(cy - 13.0f), col565);
                    break;
                }
            }

            // Clean vector App Label using DejaVu9
            canvas.setFont(&fonts::DejaVu9);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(0xFFFF);
            canvas.drawString(items[i].label, cx, cy + 6.0f);

            // Clean vector Badge / Count using DejaVu9
            if (items[i].sub && items[i].sub[0] != '\0')
            {
                canvas.setTextColor(items[i].subColor);
                canvas.drawString(items[i].sub, cx, cy + 18.0f);
            }
        }

        // 4. Bottom Home Indicator Bar (Clean subtle rounded pill at bottom)
        canvas.fillRoundRect((int)(w * 0.5f - 18.0f + offsetX), (int)(h - 12.0f), 36, 4, 2, canvas.color565(60, 65, 75));
    }

    void HomeScreen::processTouch(Touch& touch, uint16_t w, uint16_t h, 
                                  int remCount, int ideaCount, int qCount, int taskCount, int memCount, 
                                  ScreenId& targetScreen)
    {
        uint16_t tx = 0, ty = 0;
        bool touched = touch.getPoint(tx, ty);

        if (touched)
        {
            uint32_t nowMs = millis();

            if (!m_wasTouched)
            {
                // Touch Down
                m_wasTouched = true;
                m_dragStartX = tx;
                m_dragStartY = ty;
                m_lastDragX = tx;
                m_lastDragY = ty;
                m_lastTouchSampleMs = nowMs;
                m_isDragging = false;
                m_swipeOffset = 0.0f;

                if (m_page == 0)
                {
                    // 2x2 Hub Metrics Tiles Hitboxes (Y = 144 to 212)
                    if (ty >= 144.0f && ty <= 174.0f)
                    {
                        if (tx >= 10.0f && tx <= 118.0f) m_pressedCardIndex = 10; // TASKS
                        else if (tx >= 120.0f && tx <= 230.0f) m_pressedCardIndex = 11; // IDEAS
                    }
                    else if (ty >= 176.0f && ty <= 212.0f)
                    {
                        if (tx >= 10.0f && tx <= 118.0f) m_pressedCardIndex = 12; // MEMOS
                        else if (tx >= 120.0f && tx <= 230.0f) m_pressedCardIndex = 13; // OTHERS
                    }
                    // Bottom Action Button: Record Voice (Y = 218 to 275)
                    else if (ty >= 218.0f && ty <= 275.0f)
                    {
                        m_pressedCardIndex = 0; // Record
                    }
                    // Tap clock / top region to switch to Page 1
                    else if (ty >= 30.0f && ty < 144.0f)
                    {
                        m_pressedCardIndex = 1; // App Menu Page
                    }
                }
                else if (m_page == 1)
                {
                    // 3x3 Tactile Grid Hitboxes (dynamically responsive)
                    bool isLandscape = (w > h);
                    float colW = w / 3.0f;
                    int col = (int)(tx / colW);
                    if (col < 0 || col > 2) col = -1;

                    int row = -1;
                    if (!isLandscape)
                    {
                        if (ty >= 64 && ty <= 132) row = 0;
                        else if (ty >= 136 && ty <= 208) row = 1;
                        else if (ty >= 212 && ty <= 284) row = 2;
                    }
                    else
                    {
                        if (ty >= 44 && ty <= 100) row = 0;
                        else if (ty >= 102 && ty <= 158) row = 1;
                        else if (ty >= 160 && ty <= 218) row = 2;
                    }

                    if (col >= 0 && row >= 0)
                    {
                        m_pressedItemIndex = row * 3 + col;
                    }
                }
            }
            else
            {
                // Touch Move (Dragging)
                float dx = tx - m_dragStartX;
                float dy = ty - m_dragStartY;

                if (!m_isDragging)
                {
                    // Swiping between Page 0 and Page 1
                    if (std::abs(dx) > 1.5f * std::abs(dy) && std::abs(dx) > 15.0f)
                    {
                        m_isDragging = true;
                        m_pressedCardIndex = -1;
                        m_pressedItemIndex = -1;
                    }
                }

                if (m_isDragging)
                {
                    m_swipeOffset = dx;
                }

                m_lastDragX = tx;
                m_lastDragY = ty;
            }
        }
        else
        {
            if (m_wasTouched)
            {
                // Touch Up (Release)
                m_wasTouched = false;
                float rx = m_lastDragX;
                float ry = m_lastDragY;

                if (m_isDragging)
                {
                    float dx = rx - m_dragStartX;
                    if (dx < -60.0f && m_page == 0)
                    {
                        m_page = 1;
                    }
                    else if (dx > 60.0f && m_page == 1)
                    {
                        m_page = 0;
                    }
                    m_isDragging = false;
                    m_swipeOffset = 0.0f;
                }
                else
                {
                    // Tap Action Execution
                    if (m_page == 0)
                    {
                        if (m_pressedCardIndex == 0)
                        {
                            AudioManager::instance().playTone(1200, 80);
                            targetScreen = ScreenId::Record;
                        }
                        else if (m_pressedCardIndex == 10)
                        {
                            AudioManager::instance().playTapSoundAsync();
                            m_visitedTasks = true;
                            targetScreen = ScreenId::Tasks;
                        }
                        else if (m_pressedCardIndex == 11)
                        {
                            AudioManager::instance().playTapSoundAsync();
                            m_visitedIdeas = true;
                            targetScreen = ScreenId::Ideas;
                        }
                        else if (m_pressedCardIndex == 12)
                        {
                            AudioManager::instance().playTapSoundAsync();
                            targetScreen = ScreenId::RecordingsLibrary;
                        }
                        else if (m_pressedCardIndex == 13)
                        {
                            AudioManager::instance().playTapSoundAsync();
                            targetScreen = ScreenId::Others;
                        }
                        else if (m_pressedCardIndex == 1)
                        {
                            AudioManager::instance().playTapSoundAsync();
                            m_page = 1;
                        }
                    }
                    else if (m_page == 1)
                    {
                        // 3x3 Tactile Grid Launch
                        if (m_pressedItemIndex >= 0)
                        {
                            AudioManager::instance().playTapSoundAsync();
                            switch (m_pressedItemIndex)
                            {
                                case 0: m_visitedTasks     = true; targetScreen = ScreenId::Tasks;             break;
                                case 1: m_visitedReminders = true; targetScreen = ScreenId::Reminders;         break;
                                case 2: m_visitedIdeas     = true; targetScreen = ScreenId::Ideas;             break;
                                case 3: m_visitedQuestions = true; targetScreen = ScreenId::Questions;         break;
                                case 4: targetScreen = ScreenId::Music;                                         break;
                                case 5: targetScreen = ScreenId::RecordingsLibrary;                             break;
                                case 6: targetScreen = ScreenId::Search;                                        break;
                                case 7: m_visitedOthers    = true; targetScreen = ScreenId::Others;            break;
                                case 8: targetScreen = ScreenId::Settings;                                      break;
                            }
                        }
                    }
                }

                m_pressedCardIndex = -1;
                m_pressedItemIndex = -1;
            }
        }
    }

    ScreenId HomeScreen::show(Touch& touch)
    {
        uint16_t w = Display::width();
        uint16_t h = Display::height();

        // Create double-buffering canvas sprite
        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        bool useSprite = canvas.createSprite(w, h);
        if (useSprite) { canvas.fillScreen(TFT_BLACK); }
        LovyanGFX& target = useSprite ? (LovyanGFX&)canvas : (LovyanGFX&)Display::lcd;

        auto refreshCounts = [&]() {
            int remCount = reminderService.getPendingCount();
            int ideaCount = static_cast<int>(ideaService.getAll().size());
            int qCount = static_cast<int>(questionService.getAll().size());
            int taskCount = static_cast<int>(dataService.getTaskCount());
            int memCount = static_cast<int>(recordingService.getAll().size());
            return std::array<int, 5>{ remCount, ideaCount, qCount, taskCount, memCount };
        };

        auto counts = refreshCounts();
        int remCount = counts[0];
        int ideaCount = counts[1];
        int qCount = counts[2];
        int taskCount = counts[3];
        int memCount = counts[4];
        uint32_t lastCountRefreshMs = millis();

        ScreenId targetScreen = ScreenId::Home;
        uint32_t lastMs = millis();
        int entryFrame = 0;
        m_gridAnimElapsed = 0.0f;   // reset on HomeScreen entry

        while (targetScreen == ScreenId::Home)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            if (nowMs - lastCountRefreshMs > 5000)
            {
                counts = refreshCounts();
                remCount = counts[0];
                ideaCount = counts[1];
                qCount = counts[2];
                taskCount = counts[3];
                memCount = counts[4];
                lastCountRefreshMs = nowMs;
            }

            // 1. Tick Power Management
            PowerManager::instance().tick();

            // 2. Check physical hardware record button
            if (ButtonService::isDirectRecordRequested())
            {
                targetScreen = ScreenId::Record;
                break;
            }

            // 3. Process touch
            if (entryFrame >= 5 && !QuickPanel::instance().isOpen())
            {
                processTouch(touch, w, h, remCount, ideaCount, qCount, taskCount, memCount, targetScreen);
            }

            // Dimension check
            uint16_t currentW = Display::width();
            uint16_t currentH = Display::height();
            if (currentW != w || currentH != h)
            {
                w = currentW;
                h = currentH;
                float width_f = static_cast<float>(w);
                m_scrollOffset = std::max(0.0f, std::min(width_f, m_scrollOffset));

                if (useSprite)
                {
                    canvas.deleteSprite();
                    useSprite = canvas.createSprite(w, h);
                    if (useSprite) canvas.fillScreen(TFT_BLACK);
                }
                Display::lcd.fillScreen(TFT_BLACK);
            }

            // Radial dial rotation spring interpolation (Page 1)
            if (!m_isRadialDragging)
            {
                m_radialAngle += (m_targetRadialAngle - m_radialAngle) * 18.0f * deltaSecs;
                if (std::abs(m_targetRadialAngle - m_radialAngle) < 0.001f)
                {
                    m_radialAngle = m_targetRadialAngle;
                }
            }

            // Horizontal slide page transition
            float width_f = static_cast<float>(w);
            if (m_isDragging)
            {
                m_scrollOffset = m_page * w - m_swipeOffset;
            }
            else
            {
                float target = m_page * w;
                m_scrollOffset += (target - m_scrollOffset) * 15.0f * deltaSecs;
                if (std::abs(target - m_scrollOffset) < 0.1f)
                {
                    m_scrollOffset = target;
                }
            }

            // Grid stagger-in: advance timer while on page1, reset on page switch
            if (m_page == 1)
            {
                if (m_lastPage != 1) { m_gridAnimElapsed = 0.0f; }  // just switched to page1
                m_gridAnimElapsed += deltaSecs;
            }
            m_lastPage = m_page;

            // Fill pure black background
            target.fillScreen(TFT_BLACK);

            // Draw sliding page contents
            for (int p = 0; p < 2; ++p)
            {
                float drawX = p * width_f - m_scrollOffset;
                if (drawX <= -width_f || drawX >= width_f) continue;

                if (p == 0)
                {
                    renderPage0(target, w, h, drawX, remCount, ideaCount, qCount, taskCount, memCount);
                }
                else
                {
                    renderPage1(target, w, h, remCount, ideaCount, qCount, taskCount, memCount, drawX, m_gridAnimElapsed);
                }
            }

            // Quick Panel Overlay
            ScreenId qpNav = QuickPanel::instance().process(touch, target, w, h);
            if (qpNav != ScreenId::Home)
            {
                targetScreen = qpNav;
            }

            if (targetScreen != ScreenId::Home)
            {
                break;
            }

            // Push render buffer sprite to screen
            if (useSprite)
            {
                if (entryFrame < 6)
                {
                    VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Home), entryFrame, 6);
                    entryFrame++;
                }
                else
                {
                    canvas.pushSprite(0, 0);
                }
            }


            uint32_t frameMs = millis() - nowMs;
            if (frameMs < 16)
            {
                delay(16 - frameMs);
            }
        }

        if (useSprite)
        {
            canvas.deleteSprite();
        }

        return targetScreen;
    }
}
