#include "HomeScreen.h"
#include "../ui/Theme.h"
#include <cmath>
#include <algorithm>
#include <ctime>

namespace VOXA
{
    HomeScreen::HomeScreen()
    {
    }

    void HomeScreen::renderPage0(LGFX_Sprite& canvas, uint16_t w, uint16_t h, float offsetX)
    {
        float cx = w * 0.5f + offsetX;
        
        // Brand Title (DejaVu18)
        canvas.setFont(&fonts::DejaVu18);
        canvas.setTextSize(1);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(VoxaTheme::getPrimaryLight());
        canvas.drawString("VOXA", cx, h * 0.125f);
        
        // Greeting based on RTC time (DejaVu12)
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

        canvas.setFont(&fonts::DejaVu12);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        canvas.setTextSize(2);
        canvas.drawString(greeting.c_str(), cx, h * 0.23f);
        
        // Subtitle (DejaVu12)
        canvas.setTextColor(VoxaTheme::getTextSecondary());
        canvas.setTextSize(1);
        canvas.drawString("How can I help today?", cx, h * 0.33f);

        // Pulsating voice assistant microphone button
        float micCx = w * 0.5f + offsetX;
        float micCy = h * 0.54f;
        
        // Pressed button animation: scales down slightly when pressed
        float micR = m_isMicPressed ? 26.0f : 30.0f;
        float pulse = std::sin(m_elapsed * 4.0f) * 0.5f + 0.5f;

        // Concentric ambient halos (using pre-blended colors)
        uint16_t bg = VoxaTheme::getBackground();
        uint8_t bg_r = (bg >> 11) << 3;
        uint8_t bg_g = ((bg >> 5) & 0x3F) << 2;
        uint8_t bg_b = (bg & 0x1F) << 3;
        
        float alpha1 = (30.0f + pulse * 15.0f) / 255.0f;
        float alpha2 = (8.0f + pulse * 8.0f) / 255.0f;
        
        uint16_t halo1 = canvas.color565(
            (uint8_t)((1.0f - alpha1) * bg_r + alpha1 * 124),
            (uint8_t)((1.0f - alpha1) * bg_g + alpha1 * 92),
            (uint8_t)((1.0f - alpha1) * bg_b + alpha1 * 255)
        );
        uint16_t halo2 = canvas.color565(
            (uint8_t)((1.0f - alpha2) * bg_r + alpha2 * 124),
            (uint8_t)((1.0f - alpha2) * bg_g + alpha2 * 92),
            (uint8_t)((1.0f - alpha2) * bg_b + alpha2 * 255)
        );

        canvas.drawCircle((int)micCx, (int)micCy, (int)(micR + 6.0f + pulse * 4.0f), halo1);
        canvas.drawCircle((int)micCx, (int)micCy, (int)(micR + 12.0f + pulse * 8.0f), halo2);

        // Core button circle (darkens on press)
        uint16_t btnColor = m_isMicPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getPrimary();
        canvas.fillCircle((int)micCx, (int)micCy, (int)micR, btnColor);
        canvas.drawCircle((int)micCx, (int)micCy, (int)micR, VoxaTheme::getTextPrimary());

        // Mic icon glyph
        ScreenCommon::drawMicShape(canvas, micCx, micCy, micR * 0.85f * 2.0f, VoxaTheme::getTextPrimary(), btnColor);

        // Helper label
        canvas.setFont(&fonts::DejaVu12);
        canvas.setTextColor(VoxaTheme::getPrimary());
        canvas.setTextSize(1);
        canvas.drawString("Tap to Record", cx, h * 0.74f);

        // Chevron navigation button (scales down or highlights on press)
        float btnCx = w * 0.90f + offsetX;
        float btnCy = h * 0.54f;
        uint16_t chevFill = m_isChevronPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
        uint16_t chevColor = m_isChevronPressed ? VoxaTheme::getBackground() : VoxaTheme::getPrimary();
        ScreenCommon::renderCircularButton(canvas, btnCx, btnCy, Icon::ChevronRight, 
                                          chevFill, chevColor, w, h);
    }

    void HomeScreen::renderPage1(LGFX_Sprite& canvas, uint16_t w, uint16_t h, 
                                 int remCount, int ideaCount, int qCount, int memCount, float offsetX)
    {
        // Title Header shifted to Y = 45.0f to prevent overlapping
        canvas.setFont(&fonts::DejaVu18);
        canvas.setTextSize(1);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextColor(VoxaTheme::getTextPrimary());
        canvas.drawString("Menu", w * 0.5f + offsetX, 45.0f);

        // Header Back button at Y = 45.0f
        uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
        uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
        ScreenCommon::renderCircularButton(canvas, 20.0f + offsetX, 45.0f, Icon::Back, 
                                          backFill, backColor, w, h);

        // Header Rotation toggle button at Y = 45.0f
        uint16_t rotFill = m_isRotatePressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
        uint16_t rotColor = m_isRotatePressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
        ScreenCommon::renderCircularButton(canvas, w - 20.0f + offsetX, 45.0f, Icon::Plus, 
                                          rotFill, rotColor, w, h);

        struct MenuItem
        {
            Icon icon;
            const char* label;
            uint16_t color;
            int badgeCount;
        };

        MenuItem menuItems[7] = {
            { Icon::Bell,       "Reminders",  0x79CF, remCount },
            { Icon::Lightbulb,  "Ideas",      0xFD20, ideaCount },
            { Icon::Question,   "Questions",  0x067F, qCount },
            { Icon::Search,     "Search",     0x266C, 0 },
            { Icon::Mic,        "Recordings", 0xFAC0, memCount },
            { Icon::Folder,     "Others",     0xAD55, 0 },
            { Icon::Settings,   "Settings",   0x52AA, 0 }
        };

        float leftX = w * 0.04f + offsetX;
        float cardW = w * 0.92f;

        // Clip scrollable cards to viewport (middle region Y = 70 to h - 18)
        canvas.setClipRect(0, 70, w, h - 70 - 18);

        for (int i = 0; i < 7; ++i)
        {
            float itemY = 72.0f + i * 50.0f - m_menuScrollY;

            // Clip boundaries optimization
            if (itemY + 44.0f < 70.0f || itemY > (h - 18.0f))
            {
                continue;
            }

            // Pressed card feedback animation: card is filled with primary color
            bool isPressed = (m_pressedItemIndex == i);
            uint16_t cardBg = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t cardBorder = isPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getDivider();
            uint16_t labelColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            uint16_t chevColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextSecondary();

            // Card background rounded rectangle
            canvas.fillRoundRect((int)leftX, (int)itemY, (int)cardW, 44, 8, cardBg);
            canvas.drawRoundRect((int)leftX, (int)itemY, (int)cardW, 44, 8, cardBorder);

            // Icon circle container
            float cy = itemY + 22.0f;
            float iconCx = leftX + 22.0f;
            canvas.fillCircle((int)iconCx, (int)cy, 12, menuItems[i].color);
            ScreenCommon::drawIcon(canvas, menuItems[i].icon, iconCx - 6.0f, cy - 6.0f, 12.0f, VoxaTheme::getBackground());

            // Label text (DejaVu12)
            canvas.setFont(&fonts::DejaVu12);
            canvas.setTextDatum(textdatum_t::middle_left);
            canvas.setTextColor(labelColor);
            canvas.setTextSize(1);
            canvas.drawString(menuItems[i].label, leftX + 42.0f, cy);

            // Badge Count (if active)
            if (menuItems[i].badgeCount > 0)
            {
                float badgeCx = leftX + cardW - 32.0f;
                uint16_t badgeBg = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getPrimaryLight();
                uint16_t badgeText = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getTextPrimary();
                
                canvas.fillCircle((int)badgeCx, (int)cy, 8, badgeBg);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(badgeText);
                canvas.setTextSize(1);
                char badgeStr[8];
                itoa(menuItems[i].badgeCount, badgeStr, 10);
                canvas.drawString(badgeStr, badgeCx, cy);
            }

            // Navigation chevron
            float chevX = leftX + cardW - 16.0f;
            ScreenCommon::drawIcon(canvas, Icon::ChevronRight, chevX - 5.0f, cy - 5.0f, 10.0f, chevColor);
        }

        canvas.clearClipRect();
    }

    void HomeScreen::processTouch(Touch& touch, uint16_t w, uint16_t h, 
                                  int remCount, int ideaCount, int qCount, int memCount, 
                                  ScreenId& targetScreen)
    {
        uint16_t tx = 0, ty = 0;
        bool touched = touch.getPoint(tx, ty);

        float contentHeight = 7.0f * 50.0f + 10.0f;
        float visibleHeight = h - 70.0f - 18.0f; // safe viewport Y bounds Y=70..h-18
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
                    // Microphone button bounds (Hit circle radius 42)
                    float micCx = w * 0.5f;
                    float micCy = h * 0.54f;
                    if (std::sqrt((tx - micCx)*(tx - micCx) + (ty - micCy)*(ty - micCy)) <= 42.0f)
                    {
                        m_isMicPressed = true;
                    }
                    
                    // Page chevron button bounds
                    float btnCx = w * 0.90f;
                    float btnCy = h * 0.54f;
                    if (std::sqrt((tx - btnCx)*(tx - btnCx) + (ty - btnCy)*(ty - btnCy)) <= 20.0f)
                    {
                        m_isChevronPressed = true;
                    }
                }
                else if (m_page == 1)
                {
                    // Header Back button bounds centered at Y = 45.0f
                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isBackPressed = true;
                    }
                    
                    // Header Rotate button bounds centered at Y = 45.0f
                    if (std::sqrt((tx - (w - 20.0f))*(tx - (w - 20.0f)) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isRotatePressed = true;
                    }

                    // List items bounds check Y = 70..h-18
                    if (ty >= 70.0f && ty <= (h - 18.0f))
                    {
                        float leftX = w * 0.04f;
                        float cardW = w * 0.92f;
                        for (int i = 0; i < 7; ++i)
                        {
                            float itemY = 72.0f + i * 50.0f - m_menuScrollY;
                            if (tx >= leftX && tx <= (leftX + cardW) &&
                                ty >= itemY && ty <= (itemY + 44.0f))
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
                        if (rx >= 5 && rx <= 60 && ry >= 2 && ry <= 20)
                        {
                            VoxaTheme::ThemeMode nextTheme = (VoxaTheme::getThemeMode() == VoxaTheme::ThemeMode::Dark)
                                ? VoxaTheme::ThemeMode::Light : VoxaTheme::ThemeMode::Dark;
                            VoxaTheme::setThemeMode(nextTheme);
                            Serial.print("[Theme] Toggled to: ");
                            Serial.println(nextTheme == VoxaTheme::ThemeMode::Dark ? "Dark" : "Light");
                        }

                        // Microphone button tap
                        if (m_isMicPressed)
                        {
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
                                case 0: targetScreen = ScreenId::Reminders; break;
                                case 1: targetScreen = ScreenId::Ideas;     break;
                                case 2: targetScreen = ScreenId::Questions; break;
                                case 3: targetScreen = ScreenId::Search;    break;
                                case 4: targetScreen = ScreenId::Record;    break;
                                case 5: targetScreen = ScreenId::Others;    break;
                                case 6: targetScreen = ScreenId::Settings;  break;
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
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            Serial.println("[HomeScreen] Error creating canvas double-buffer!");
            return ScreenId::Home;
        }

        // Mock badge counts
        int remCount = 3;
        int ideaCount = 4;
        int qCount = 4;
        int memCount = 6;

        ScreenId targetScreen = ScreenId::Home;
        uint32_t lastMs = millis();

        while (targetScreen == ScreenId::Home)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            m_elapsed += deltaSecs;

            // 1. Process touch gestures and pressed feedback updates
            processTouch(touch, w, h, remCount, ideaCount, qCount, memCount, targetScreen);

            // Re-query dimensions inside the loop since rotation changes width and height on-the-fly!
            w = Display::width();
            h = Display::height();

            // 2. Perform vertical scroll inertia calculations
            float contentHeight = 7.0f * 50.0f + 10.0f;
            float visibleHeight = h - 70.0f - 18.0f; // Y=70..h-18 viewport bounds
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
            ScreenCommon::renderSurface(canvas, w, h);

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
                    renderPage0(canvas, w, h, drawX);
                }
                else
                {
                    renderPage1(canvas, w, h, remCount, ideaCount, qCount, memCount, drawX);
                }
            }

            // 6. Draw page dot indicators (stays static at bottom center)
            int dotActive = (int)round(m_scrollOffset / width_f);
            dotActive = std::max(0, std::min(1, dotActive));
            ScreenCommon::renderPageDots(canvas, dotActive, 2, w, h);

            // 7. Push render buffer sprite to screen
            canvas.pushSprite(0, 0);

            // Throttle to roughly 60 FPS
            uint32_t frameMs = millis() - nowMs;
            if (frameMs < 16)
            {
                delay(16 - frameMs);
            }
        }

        return targetScreen;
    }
}
