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


#include <array>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ── Upload background task shared state ──────────────────────────────────────
namespace
{
    volatile bool s_uploadDone  = false;
    volatile bool s_uploadOk    = false;
    char          s_uploadText [256] = {};
    char          s_uploadError[128] = {};
    std::string   s_uploadPath;

    void uploadTaskFn(void* /*param*/)
    {
        VOXA::ApiResult res = VOXA::apiClient.uploadVoice(s_uploadPath);
        s_uploadOk = res.success;
        strncpy(s_uploadText,  res.text.c_str(),  255);
        strncpy(s_uploadError, res.error.c_str(), 127);
        s_uploadDone = true;
        vTaskDelete(nullptr);
    }
}

namespace VOXA
{
    extern ReminderService reminderService;
    extern IdeaService ideaService;
    extern QuestionService questionService;
    extern RecordingService recordingService;

    HomeScreen::HomeScreen()
    {
    }

    void HomeScreen::begin()
    {
    }

    void HomeScreen::renderPage0(LovyanGFX& canvas, uint16_t w, uint16_t h, float offsetX)
    {
        float cx = w * 0.5f + offsetX;
        bool dark = VoxaTheme::isDarkMode();

        // 0. Quick Theme Indicator & Button (Top-Right Bezel)
        uint16_t themeIcoColor = dark ? 0xFEE0 : 0xFD20;
        ScreenCommon::drawIcon(canvas, dark ? Icon::Sun : Icon::Moon, w - 22.0f + offsetX, 6.0f, 15.0f, themeIcoColor);

        // 1. VOXA Intelligence Frosted Glass Pill Badge (Centered)
        float pillW = 126.0f;
        float pillH = 20.0f;
        float pillX = cx - pillW * 0.5f;
        float pillY = 36.0f; // Sits with clean breathing room below Dynamic Island
        uint16_t pillBg = dark ? canvas.color565(20, 22, 32) : 0xFFFF;
        uint16_t pillBorder = dark ? VoxaTheme::getGlassBorder() : VoxaTheme::getDivider();
        uint16_t pillHighlight = dark ? VoxaTheme::getGlassHighlight() : 0xFFFF;

        canvas.fillRoundRect((int)pillX, (int)pillY, (int)pillW, (int)pillH, 10, pillBg);
        canvas.drawRoundRect((int)pillX, (int)pillY, (int)pillW, (int)pillH, 10, pillBorder);
        canvas.drawFastHLine((int)pillX + 10, (int)pillY + 1, (int)pillW - 20, pillHighlight);

        // Center badge text
        canvas.setFont(&fonts::Font0);
        canvas.setTextSize(1);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(VoxaTheme::getPrimary());
        canvas.drawString("VOXA INTELLIGENCE", cx, pillY + pillH * 0.5f);

        // 2. Greeting based on RTC time (Sleek Apple SF Typography - Centered)
        std::string greeting = "Good Morning";
        std::time_t tNow = std::time(nullptr);
        std::tm local_tm;
#if defined(_MSC_VER)
        localtime_s(&local_tm, &tNow);
#else
        localtime_r(&tNow, &local_tm);
#endif
        int hour = local_tm.tm_hour;
        if (hour >= 12 && hour < 17) greeting = "Good Afternoon";
        else if (hour >= 17 && hour < 21) greeting = "Good Evening";
        else if (hour >= 21 || hour < 5) greeting = "Good Night";

        float greetY = 74.0f;
        canvas.setFont(&fonts::FreeSansBold12pt7b);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        canvas.setTextSize(1);
        canvas.drawString(greeting.c_str(), cx, greetY);

        // Subtitle (Centered)
        float subY = 96.0f;
        canvas.setFont(&fonts::FreeSans9pt7b);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(VoxaTheme::getTextSecondary());
        canvas.setTextSize(1);
        canvas.drawString("Ready when you are", cx, subY);

        // 3. iOS 26 Siri Chromatic Fluid Glass Orb (Centered)
        float micCx = cx;
        float micCy = 172.0f;
        float baseR = m_isMicPressed ? 30.0f : 34.0f;
        float t = m_elapsed * 3.2f;

        // Chromatic multi-tone breathing rings
        float rCyan  = baseR + 13.0f + std::sin(t) * 3.5f;
        float rPurp  = baseR + 8.5f  + std::cos(t * 1.3f) * 2.5f;
        float rFlame = baseR + 4.0f  + std::sin(t * 1.8f) * 1.8f;

        if (dark)
        {
            // Dark Mode Luminous Rings
            canvas.drawCircle((int)micCx, (int)micCy, (int)rCyan, canvas.color565(14, 55, 115));
            canvas.drawCircle((int)micCx, (int)micCy, (int)(rCyan - 1.0f), canvas.color565(0, 80, 160));

            canvas.drawCircle((int)micCx, (int)micCy, (int)rPurp, canvas.color565(95, 25, 115));
            canvas.drawCircle((int)micCx, (int)micCy, (int)(rPurp - 1.0f), canvas.color565(135, 35, 145));

            canvas.drawCircle((int)micCx, (int)micCy, (int)rFlame, canvas.color565(210, 85, 0));
            canvas.drawCircle((int)micCx, (int)micCy, (int)(rFlame - 1.0f), canvas.color565(255, 120, 0));
        }
        else
        {
            // Light Mode Soft Pastel Rings (Never dirty or dark)
            canvas.drawCircle((int)micCx, (int)micCy, (int)rCyan, canvas.color565(180, 220, 255));
            canvas.drawCircle((int)micCx, (int)micCy, (int)(rCyan - 1.0f), canvas.color565(150, 205, 255));

            canvas.drawCircle((int)micCx, (int)micCy, (int)rPurp, canvas.color565(230, 195, 250));
            canvas.drawCircle((int)micCx, (int)micCy, (int)(rPurp - 1.0f), canvas.color565(215, 175, 245));

            canvas.drawCircle((int)micCx, (int)micCy, (int)rFlame, canvas.color565(255, 215, 170));
            canvas.drawCircle((int)micCx, (int)micCy, (int)(rFlame - 1.0f), canvas.color565(255, 195, 140));
        }

        // Glass Sphere Core
        uint16_t orbCore = m_isMicPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getPrimary();
        canvas.fillCircle((int)micCx, (int)micCy, (int)baseR, orbCore);
        canvas.drawCircle((int)micCx, (int)micCy, (int)baseR, 0xFFFF);

        // Specular Optical Glass Highlight (White gloss crescent top-left)
        canvas.fillCircle((int)(micCx - baseR * 0.35f), (int)(micCy - baseR * 0.35f), (int)(baseR * 0.32f), canvas.color565(255, 210, 170));
        canvas.fillCircle((int)(micCx - baseR * 0.40f), (int)(micCy - baseR * 0.40f), (int)(baseR * 0.18f), 0xFFFF);

        // Mic icon glyph centered in pure white
        ScreenCommon::drawMicShape(canvas, micCx, micCy, baseR * 1.5f, 0xFFFF, orbCore);

        // 4. "Tap to Record" iOS Frosted Glass Capsule Button (100% Centered)
        float actionW = 150.0f;
        float actionH = 34.0f;
        float actionX = cx - actionW * 0.5f;
        float actionY = 244.0f;

        uint16_t actionBg = dark ? canvas.color565(20, 22, 32) : 0xFFFF;
        uint16_t actionBorder = dark ? VoxaTheme::getGlassBorder() : VoxaTheme::getDivider();
        uint16_t actionText = VoxaTheme::getTextPrimary();

        canvas.fillRoundRect((int)actionX, (int)actionY, (int)actionW, (int)actionH, 17, actionBg);
        canvas.drawRoundRect((int)actionX, (int)actionY, (int)actionW, (int)actionH, 17, actionBorder);
        canvas.drawFastHLine((int)actionX + 17, (int)actionY + 1, (int)actionW - 34, 
            dark ? VoxaTheme::getGlassHighlight() : 0xFFFF);

        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextColor(actionText);
        canvas.setTextSize(1);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.drawString("Tap to Record", cx, actionY + actionH * 0.5f);
    }


    void HomeScreen::renderPage1(LovyanGFX& canvas, uint16_t w, uint16_t h, 
                                 int remCount, int ideaCount, int qCount, int taskCount, int memCount, float offsetX)
    {
        // iOS Navigation Bar Header at Y = 46.0f
        ScreenCommon::renderHeader(canvas, "App Library", true, true, Icon::Rotate, w, h);

        // Header Back button
        uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getGlassSurface();
        uint16_t backColor = m_isBackPressed ? 0xFFFF : VoxaTheme::getTextPrimary();
        ScreenCommon::renderCircularButton(canvas, 22.0f + offsetX, 46.0f, Icon::Back, 
                                          backFill, backColor, w, h);

        // Header Rotation toggle button
        uint16_t rotFill = m_isRotatePressed ? VoxaTheme::getPrimary() : VoxaTheme::getGlassSurface();
        uint16_t rotColor = m_isRotatePressed ? 0xFFFF : VoxaTheme::getTextPrimary();
        ScreenCommon::renderCircularButton(canvas, w - 22.0f + offsetX, 46.0f, Icon::Rotate, 
                                          rotFill, rotColor, w, h);

        struct MenuItem
        {
            Icon icon;
            const char* label;
            uint16_t color;
            int badgeCount;
        };

        // Authentic iOS System Category Colors
        MenuItem menuItems[9] = {
            { Icon::Bell,       "Reminders",  VoxaTheme::getSystemAmber(),   m_visitedReminders ? 0 : remCount },
            { Icon::Lightbulb,  "Ideas",      0xFEE0,                        m_visitedIdeas ? 0 : ideaCount },
            { Icon::Question,   "Questions",  VoxaTheme::getSystemBlue(),    m_visitedQuestions ? 0 : qCount },
            { Icon::Note,       "Tasks",      VoxaTheme::getSystemGreen(),   m_visitedTasks ? 0 : taskCount },
            { Icon::Play,       "Voxa Music", VoxaTheme::getSystemPurple(),  0 },
            { Icon::Search,     "Search",     VoxaTheme::getSystemIndigo(),  0 },
            { Icon::Mic,        "Recordings", VoxaTheme::getSystemRed(),     memCount },
            { Icon::Folder,     "Others",     0x52AA,                        m_visitedOthers ? 0 : 0 },
            { Icon::Settings,   "Settings",   0x7BEF,                        0 }
        };

        float leftX = w * 0.04f + offsetX;
        float cardW = w * 0.92f;

        // Clip scrollable cards to viewport below navigation header
        canvas.setClipRect(0, 68, w, h - 68);

        for (int i = 0; i < 9; ++i)
        {
            float itemY = 72.0f + i * 52.0f - m_menuScrollY;

            // Clip boundaries optimization
            if (itemY + 46.0f < 68.0f || itemY > (h + 10.0f))
            {
                continue;
            }

            bool isPressed = (m_pressedItemIndex == i);

            // iOS 26 Glassmorphic Card Container with Specular Top Highlight
            ScreenCommon::drawGlassCard(canvas, leftX, itemY, cardW, 46.0f, 12.0f, isPressed, menuItems[i].color);

            uint16_t labelColor = isPressed ? 0xFFFF : VoxaTheme::getTextPrimary();
            uint16_t chevColor = isPressed ? 0xFFFF : VoxaTheme::getTextSecondary();

            // iOS Squircle App Icon Container (32x32 rounded rectangle, radius 8)
            float cy = itemY + 23.0f;
            float iconX = leftX + 8.0f;
            float iconY = itemY + 7.0f;
            canvas.fillRoundRect((int)iconX, (int)iconY, 32, 32, 8, menuItems[i].color);
            // Specular gloss along top edge of icon
            canvas.drawFastHLine((int)iconX + 6, (int)iconY + 1, 20, 0xFFFF);

            // Draw icon inside squircle
            ScreenCommon::drawIcon(canvas, menuItems[i].icon, iconX + 6.0f, iconY + 6.0f, 20.0f, 0xFFFF);

            // Label text in Clean SF Typography
            canvas.setFont(&fonts::FreeSans9pt7b);
            canvas.setTextDatum(textdatum_t::middle_left);
            canvas.setTextColor(labelColor);
            canvas.setTextSize(1);
            canvas.drawString(menuItems[i].label, leftX + 48.0f, cy);

            // Badge Count — Authentic iOS Crimson Notification Capsule
            if (menuItems[i].badgeCount > 0)
            {
                float badgeRight = leftX + cardW - 30.0f;
                uint16_t badgeBg = VoxaTheme::getSystemRed();
                uint16_t badgeText = 0xFFFF;
                
                canvas.setFont(&fonts::Font0);
                char badgeStr[8];
                itoa(menuItems[i].badgeCount, badgeStr, 10);
                int textW = canvas.textWidth(badgeStr);
                int pillW = std::max(18, textW + 10);

                canvas.fillRoundRect((int)(badgeRight - pillW), (int)(cy - 8.0f), pillW, 16, 8, badgeBg);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(badgeText);
                canvas.drawString(badgeStr, badgeRight - pillW * 0.5f, cy);
            }

            // Navigation chevron (End 8px from card right edge)
            float chevBmpX = leftX + cardW - 22.0f;
            float chevBmpY = cy - 10.0f;
            ScreenCommon::drawIcon(canvas, Icon::ChevronRight, chevBmpX, chevBmpY, 20.0f, chevColor);
        }

        canvas.clearClipRect();
    }

    void HomeScreen::processTouch(Touch& touch, uint16_t w, uint16_t h, 
                                  int remCount, int ideaCount, int qCount, int taskCount, int memCount, 
                                  ScreenId& targetScreen)
    {
        uint16_t tx = 0, ty = 0;
        bool touched = touch.getPoint(tx, ty);

        float contentHeight = 72.0f + 9.0f * 52.0f + 30.0f;
        float visibleHeight = h - 68.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

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
                m_isScrollDragging = false;
                m_swipeOffset = 0.0f;
                m_scrollVelocity = 0.0f;

                // Determine pressed triggers based on touch coordinate regions
                if (m_page == 0)
                {
                    // Microphone orb bounds OR "Tap to Record" pill bounds
                    float micCx = w * 0.5f;
                    float micCy = 172.0f;
                    bool hitOrb = (std::sqrt((tx - micCx)*(tx - micCx) + (ty - micCy)*(ty - micCy)) <= 46.0f);
                    bool hitPill = (tx >= (micCx - 80.0f) && tx <= (micCx + 80.0f) && ty >= 238.0f && ty <= 286.0f);
                    if (hitOrb || hitPill)
                    {
                        m_isMicPressed = true;
                    }
                    
                    // Tapping page dots / bottom area navigates to App Library
                    if (ty >= h - 36.0f)
                    {
                        m_isChevronPressed = true;
                    }
                }
                else if (m_page == 1)
                {
                    // Header Back button bounds centered at Y = 46.0f
                    if (std::sqrt((tx - 22.0f)*(tx - 22.0f) + (ty - 46.0f)*(ty - 46.0f)) <= 22.0f)
                    {
                        m_isBackPressed = true;
                    }
                    
                    // Header Rotate button bounds centered at Y = 46.0f
                    if (std::sqrt((tx - (w - 22.0f))*(tx - (w - 22.0f)) + (ty - 46.0f)*(ty - 46.0f)) <= 22.0f)
                    {
                        m_isRotatePressed = true;
                    }

                    // List items bounds check
                    if (ty >= 68.0f && ty <= (h - 6.0f))
                    {
                        float leftX = w * 0.04f;
                        float cardW = w * 0.92f;
                        for (int i = 0; i < 9; ++i)
                        {
                            float itemY = 72.0f + i * 52.0f - m_menuScrollY;
                            if (tx >= leftX && tx <= (leftX + cardW) &&
                                ty >= itemY && ty <= (itemY + 48.0f))
                            {
                                m_pressedItemIndex = i;
                            }
                        }
                    }
                }
            }
            else
            {
                // Touch Move (Dragging)
                float dx = tx - m_dragStartX;
                float dy = ty - m_dragStartY;

                if (!m_isDragging && !m_isScrollDragging)
                {
                    // Swiping: must be primarily horizontal and exceed threshold
                    if (std::abs(dx) > 1.5f * std::abs(dy) && std::abs(dx) > 15.0f)
                    {
                        m_isDragging = true;
                        
                        // Cancel any click pressed highlights immediately
                        m_isMicPressed = false;
                        m_isChevronPressed = false;
                        m_isBackPressed = false;
                        m_isRotatePressed = false;
                        m_pressedItemIndex = -1;
                    }
                    // Scrolling: must be primarily vertical and exceed threshold
                    else if (m_page == 1 && std::abs(dy) > std::abs(dx) && std::abs(dy) > 10.0f)
                    {
                        m_isScrollDragging = true;
                        
                        m_isMicPressed = false;
                        m_isChevronPressed = false;
                        m_isBackPressed = false;
                        m_isRotatePressed = false;
                        m_pressedItemIndex = -1;
                    }
                }

                if (m_isDragging)
                {
                    m_swipeOffset = dx;
                }
                else if (m_isScrollDragging)
                {
                    float dragDeltaY = ty - m_lastDragY;
                    m_menuTargetScrollY -= dragDeltaY;
                    // Clamp immediately to prevent excessive scrolling bounds overflow
                    m_menuTargetScrollY = std::max(0.0f, std::min(maxScrollY, m_menuTargetScrollY));
                    
                    // Track touch velocity for scroll inertia
                    uint32_t dt = nowMs - m_lastTouchSampleMs;
                    if (dt > 0)
                    {
                        m_scrollVelocity = -dragDeltaY / (dt / 1000.0f);
                    }
                    
                    m_lastTouchSampleMs = nowMs;
                }

                // Preserve last valid coordinates (both X and Y) at the end of Move frame
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

                // CRITICAL FIX: evaluate release actions using preserved last valid coordinates
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
                else if (m_isScrollDragging)
                {
                    m_isScrollDragging = false;
                }
                else
                {
                    // Tap Event (Action Triggers) using preserved last coordinates
                    if (m_page == 0)
                    {
                        // Status Bar Brand Tap -> Dynamic Dark/Light theme toggle
                        if ((rx >= 0 && rx <= 60 && ry >= 0 && ry <= 35) || (rx >= (w - 60) && rx <= w && ry >= 0 && ry <= 35))
                        {
                            VoxaTheme::ThemeMode nextTheme = (VoxaTheme::getThemeMode() == VoxaTheme::ThemeMode::Dark)
                                ? VoxaTheme::ThemeMode::Light : VoxaTheme::ThemeMode::Dark;
                            VoxaTheme::setThemeMode(nextTheme);
                            AudioManager::instance().playTapSoundAsync();
                            Serial.print("[Theme] Toggled to: ");
                            Serial.println(nextTheme == VoxaTheme::ThemeMode::Dark ? "Dark" : "Light");
                        }

                        // Microphone button tap — navigate to Record Screen
                        if (m_isMicPressed)
                        {
                            AudioManager::instance().playTone(1200, 80);
                            targetScreen = ScreenId::Record;
                        }


                        // Chevron navigation button tap
                        if (m_isChevronPressed)
                        {
                            m_page = 1;
                        }
                    }
                    else if (m_page == 1)
                    {
                        // Header Back button tap centered at Y = 45.0f (returns to Page 0)
                        if (m_isBackPressed)
                        {
                            m_page = 0;
                        }

                        // Header Rotation toggle button tap centered at Y = 45.0f
                        if (m_isRotatePressed)
                        {
                            uint8_t nextRot = (Display::getRotation() == 1) ? 3 : 1;
                            Display::setRotation(nextRot);
                            touch.setRotation(nextRot);
                            Serial.print("[Rotation] Toggled. New Rotation: ");
                            Serial.println(nextRot);
                        }

                        // Card item tap triggers using preserved last coordinates
                        if (m_pressedItemIndex >= 0)
                        {
                            switch (m_pressedItemIndex)
                            {
                                case 0: m_visitedReminders = true; targetScreen = ScreenId::Reminders; break;
                                case 1: m_visitedIdeas     = true; targetScreen = ScreenId::Ideas;     break;
                                case 2: m_visitedQuestions = true; targetScreen = ScreenId::Questions; break;
                                case 3: m_visitedTasks     = true; targetScreen = ScreenId::Tasks;     break;
                                case 4: targetScreen = ScreenId::Music; break;
                                case 5: targetScreen = ScreenId::Search;    break;
                                case 6: targetScreen = ScreenId::RecordingsLibrary; break;
                                case 7: m_visitedOthers    = true; targetScreen = ScreenId::Others;    break;
                                case 8: targetScreen = ScreenId::Settings;  break;
                            }
                        }

                    }
                }

                // Reset all button pressed states
                m_isMicPressed = false;
                m_isChevronPressed = false;
                m_isBackPressed = false;
                m_isRotatePressed = false;
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
        canvas.setPsram(true); // Allocate from 8MB PSRAM
        canvas.setColorDepth(16);
        bool useSprite = canvas.createSprite(w, h);
        if (useSprite) { canvas.fillScreen(0); }
        if (!useSprite)
        {
            Serial.println("[HomeScreen] WARN: sprite allocation failed, drawing directly to LCD.");
        }
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

            // 1. Tick Power Management (auto-sleep inactivity check)
            PowerManager::instance().tick();

            // 2. Check physical hardware record button
            if (ButtonService::isDirectRecordRequested())
            {
                targetScreen = ScreenId::Record;
                break;
            }

            // 3. Process touch gestures and pressed feedback updates (bypassed if QuickPanel is active)
            if (entryFrame >= 10 && !QuickPanel::instance().isOpen())
            {
                processTouch(touch, w, h, remCount, ideaCount, qCount, taskCount, memCount, targetScreen);
            }


            // Re-query dimensions inside the loop since rotation changes width and height on-the-fly!
            uint16_t currentW = Display::width();
            uint16_t currentH = Display::height();

            if (currentW != w || currentH != h)
            {
                w = currentW;
                h = currentH;
                float width_f = static_cast<float>(w);
                m_scrollOffset = std::max(0.0f, std::min(width_f, m_scrollOffset));

                // Re-create PSRAM sprite buffer to match new rotated resolution (240x320 vs 320x240)
                if (useSprite)
                {
                    canvas.deleteSprite();
                    useSprite = canvas.createSprite(w, h);
                    if (useSprite)
                    {
                        canvas.fillScreen(0);
                    }
                }
                Display::lcd.fillScreen(TFT_BLACK);
            }



            // 2. Perform vertical scroll inertia calculations
            float contentHeight = 72.0f + 9.0f * 52.0f + 30.0f;
            float visibleHeight = h - 68.0f;
            float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            if (!m_wasTouched && std::abs(m_scrollVelocity) > 0.0f)
            {
                m_menuTargetScrollY += m_scrollVelocity * deltaSecs;
                m_scrollVelocity *= std::pow(0.85f, deltaSecs * 60.0f); // decel rate
                if (std::abs(m_scrollVelocity) < 5.0f)
                {
                    m_scrollVelocity = 0.0f;
                }
            }

            m_menuTargetScrollY = std::max(0.0f, std::min(maxScrollY, m_menuTargetScrollY));
            m_menuScrollY += (m_menuTargetScrollY - m_menuScrollY) * 15.0f * deltaSecs;
            if (std::abs(m_menuTargetScrollY - m_menuScrollY) < 0.1f)
            {
                m_menuScrollY = m_menuTargetScrollY;
            }

            // 3. Perform horizontal sliding page transition offset updates
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

            // 4. Draw static dark mode background linear gradient and status bar clock
            ScreenCommon::renderSurface(target, w, h);

            // 5. Draw sliding page contents
            for (int p = 0; p < 2; ++p)
            {
                float drawX = p * width_f - m_scrollOffset;
                if (drawX <= -width_f || drawX >= width_f)
                {
                    continue;
                }

                if (p == 0)
                {
                    renderPage0(target, w, h, drawX);
                }
                else
                {
                    renderPage1(target, w, h, remCount, ideaCount, qCount, taskCount, memCount, drawX);
                }
            }

            // 6. Draw page dot indicators (stays static at bottom center)
            int dotActive = (int)round(m_scrollOffset / width_f);
            dotActive = std::max(0, std::min(1, dotActive));
            ScreenCommon::renderPageDots(target, dotActive, 2, w, h);

            // 6b. Process & Render Quick Panel Overlay (Pull-Down Control Center)
            ScreenId qpNav = QuickPanel::instance().process(touch, target, w, h);
            if (qpNav != ScreenId::Home)
            {
                targetScreen = qpNav;
            }


            // 7. Push render buffer sprite to screen

            if (useSprite)
            {
                if (entryFrame < 10)
                {
                    VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Home), entryFrame, 10);
                    entryFrame++;
                }
                else
                {
                    canvas.pushSprite(0, 0);
                }
            }

            // Throttle to roughly 60 FPS
            uint32_t frameMs = millis() - nowMs;
            if (frameMs < 16)
            {
                delay(16 - frameMs);
            }
        }

        // Free sprite memory before navigating so the next screen can allocate its own.
        if (useSprite)
        {
            canvas.deleteSprite();
        }

        return targetScreen;
    }
}
