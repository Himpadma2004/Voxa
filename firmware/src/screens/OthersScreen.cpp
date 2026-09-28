#include "OthersScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/MemoryService.h"
#include "../services/TimeService.h"
#include "DetailScreen.h"
#include "Transition.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace VOXA
{
    extern MemoryService memoryService;

    ScreenId OthersScreen::show(Touch& touch)
    {
        int entryFrame = 0;
        float dragStartX = 0.0f;
        float dragStartY = 0.0f;
        bool swipeBackCandidate = false;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            return ScreenId::Home;
        }

        ScreenId targetScreen = ScreenId::Others;
        uint32_t lastMs = millis();
        int activeTab = 0; // 0: Locked, 1: Biom, 2: Keys

        auto memories = memoryService.getAll();

        while (targetScreen == ScreenId::Others)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            memories = memoryService.getAll();
            int totalCount = memories.size();

            std::vector<Memory> items;
            for (const auto& m : memories)
            {
                items.push_back(m);
            }

            float contentHeight = items.size() * 58.0f + 10.0f;
            float visibleHeight = 190.0f;
            float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            // Process Touch
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
                    m_dragStartY = ty;
                    m_dragStartScrollY = m_targetScrollY;
                    m_lastTouchSampleMs = nowMs;
                    m_isDragging = false;
                    m_scrollVelocity = 0.0f;

                    // Back button / Top left "< Hub"
                    if (tx <= 65 && ty <= 46)
                    {
                        m_isBackPressed = true;
                    }

                    // Tab bar touch (Y = 46 to 68)
                    if (ty >= 46 && ty <= 68)
                    {
                        float tabW = (w - 20.0f) / 3.0f;
                        if (tx >= 10 && tx < 10 + tabW) activeTab = 0;
                        else if (tx >= 10 + tabW && tx < 10 + 2 * tabW) activeTab = 1;
                        else if (tx >= 10 + 2 * tabW && tx <= w - 10) activeTab = 2;
                        m_targetScrollY = 0.0f;
                        m_scrollY = 0.0f;
                    }

                    // Card checks
                    if (ty >= 72 && ty <= 264)
                    {
                        for (std::size_t i = 0; i < items.size(); ++i)
                        {
                            float itemY = 74.0f + i * 58.0f - m_scrollY;
                            if (tx >= 10 && tx <= (w - 10) && ty >= itemY && ty <= (itemY + 52.0f))
                            {
                                m_pressedItemIndex = (int)i;
                            }
                        }
                    }
                }
                else
                {
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        targetScreen = ScreenId::Home;
                        swipeBackCandidate = false;
                    }

                    float dy = ty - m_dragStartY;
                    if (!m_isDragging && std::abs(dy) > 10.0f)
                    {
                        m_isDragging = true;
                        m_isBackPressed = false;
                        m_pressedItemIndex = -1;
                    }

                    if (m_isDragging)
                    {
                        m_targetScrollY = m_dragStartScrollY - dy;
                        m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));

                        uint32_t dt = nowMs - m_lastTouchSampleMs;
                        if (dt > 0)
                        {
                            m_scrollVelocity = -dy / (dt / 1000.0f);
                        }
                        m_lastTouchSampleMs = nowMs;
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;

                    if (m_isDragging)
                    {
                        m_isDragging = false;
                    }
                    else
                    {
                        if (m_isBackPressed)
                        {
                            targetScreen = ScreenId::Home;
                        }
                        else if (m_pressedItemIndex >= 0 && m_pressedItemIndex < (int)items.size())
                        {
                            DetailScreen::setItem("others", items[m_pressedItemIndex].id, ScreenId::Others);
                            targetScreen = ScreenId::Detail;
                        }
                    }
                    m_isBackPressed = false;
                    m_pressedItemIndex = -1;
                }
            }

            // Scroll Inertia
            if (!m_wasTouched && std::abs(m_scrollVelocity) > 0.0f)
            {
                m_targetScrollY += m_scrollVelocity * deltaSecs;
                m_scrollVelocity *= std::pow(0.85f, deltaSecs * 60.0f);
                if (std::abs(m_scrollVelocity) < 5.0f)
                {
                    m_scrollVelocity = 0.0f;
                }
            }

            m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));
            m_scrollY += (m_targetScrollY - m_scrollY) * 15.0f * deltaSecs;
            if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
            {
                m_scrollY = m_targetScrollY;
            }

            // Render Layout - Pure Pitch Black OLED
            canvas.fillScreen(0x0000);

            // 1. Top Status Bar (Y = 10)
            canvas.setFont(&fonts::DejaVu9);
            canvas.setTextDatum(textdatum_t::top_left);
            canvas.setTextColor(0xFDC0); // Amber clock
            std::string timeStr = timeService.getCurrentTime();
            if (timeStr.empty()) timeStr = "14:41";
            if (timeStr.length() > 5) timeStr = timeStr.substr(0, 5);
            canvas.drawString(timeStr.c_str(), 12, 10);

            canvas.setTextDatum(textdatum_t::top_right);
            canvas.setTextColor(0x3DFE); // Cyan tag
            canvas.drawString("AES-256", w - 20, 10);
            // Flash bolt
            canvas.fillTriangle(w - 14, 10, w - 18, 16, w - 13, 16, 0xFDC0);
            canvas.fillTriangle(w - 15, 15, w - 10, 15, w - 14, 21, 0xFDC0);

            // 2. Sub-Header (Y = 28)
            canvas.setTextDatum(textdatum_t::middle_left);
            canvas.setFont(&fonts::DejaVu9);
            canvas.setTextColor(0xFDC0); // Amber < HUB
            canvas.drawString("<  HUB", 12, 34);

            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextDatum(textdatum_t::middle_right);
            canvas.setTextColor(0xFFFF);
            char headerTitle[32];
            snprintf(headerTitle, sizeof(headerTitle), "OTHERS (%d)", totalCount);
            canvas.drawString(headerTitle, w - 14, 33);

            // 3. Segmented Filter Tabs (Y = 48..68)
            canvas.fillRoundRect(10, 48, w - 20, 20, 5, canvas.color565(18, 21, 28));
            canvas.drawRoundRect(10, 48, w - 20, 20, 5, canvas.color565(32, 38, 50));
            float tabW = (w - 20.0f) / 3.0f;

            // Active Tab Pill
            canvas.fillRoundRect(10 + activeTab * tabW, 49, tabW, 18, 4, 0xFDC0);

            canvas.setFont(&fonts::DejaVu9);
            canvas.setTextDatum(textdatum_t::middle_center);

            canvas.setTextColor(activeTab == 0 ? 0x0000 : 0x888888);
            canvas.drawString("Locked", 10 + tabW * 0.5f, 58);

            canvas.setTextColor(activeTab == 1 ? 0x0000 : 0x888888);
            canvas.drawString("Biom", 10 + tabW * 1.5f, 58);

            canvas.setTextColor(activeTab == 2 ? 0x0000 : 0x888888);
            canvas.drawString("Keys", 10 + tabW * 2.5f, 58);

            // 4. Scrollable Card List (Y = 72..264)
            canvas.setClipRect(0, 72, w, 194);

            for (std::size_t i = 0; i < items.size(); ++i)
            {
                float itemY = 74.0f + i * 58.0f - m_scrollY;
                if (itemY + 54.0f < 72.0f || itemY > 264.0f)
                    continue;

                bool isPressed = (m_pressedItemIndex == (int)i);

                // Card container
                uint16_t cardBg = isPressed ? canvas.color565(28, 32, 42) : canvas.color565(18, 21, 28);
                uint16_t cardBorder = isPressed ? 0xFDC0 : canvas.color565(32, 38, 50);
                canvas.fillRoundRect(10, (int)itemY, w - 20, 52, 8, cardBg);
                canvas.drawRoundRect(10, (int)itemY, w - 20, 52, 8, cardBorder);

                // Left Icon (Key / Lock / Dots)
                int icX = 24;
                int icY = (int)itemY + 26;
                if (i % 3 == 0)
                {
                    // Key
                    canvas.drawCircle(icX - 3, icY, 4, 0xFDC0);
                    canvas.drawLine(icX + 1, icY, icX + 7, icY, 0xFDC0);
                    canvas.drawLine(icX + 5, icY, icX + 5, icY + 3, 0xFDC0);
                }
                else if (i % 3 == 1)
                {
                    // Lock
                    canvas.drawRoundRect(icX - 4, icY - 2, 9, 8, 2, 0xFDC0);
                    canvas.drawCircle(icX, icY - 4, 3, 0xFDC0);
                }
                else
                {
                    // Dots
                    canvas.fillCircle(icX - 3, icY, 2, 0x3DFE);
                    canvas.fillCircle(icX, icY, 2, 0x3DFE);
                    canvas.fillCircle(icX + 3, icY, 2, 0x3DFE);
                }

                // Line 1: Title
                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(0xFFFF);

                std::string tTitle = items[i].title;
                if (tTitle.length() > 22) tTitle = tTitle.substr(0, 20) + "..";
                canvas.drawString(tTitle.c_str(), 40, (int)itemY + 8);

                // Line 2: Subtitle / Confidential
                canvas.setTextColor(canvas.color565(194, 155, 80));
                std::string subStr = items[i].content;
                if (subStr.empty()) subStr = "Verified 2d ago";
                if (subStr.length() > 24) subStr = subStr.substr(0, 22) + "..";
                canvas.drawString(subStr.c_str(), 40, (int)itemY + 22);

                // Line 3: Security Badge line
                if (i % 3 == 0)
                {
                    canvas.setTextColor(0x3DFE); // Cyan
                    canvas.drawString("Thumb Auth Required", 40, (int)itemY + 36);
                }
                else if (i % 3 == 1)
                {
                    canvas.setTextColor(0xFDC0); // Amber
                    canvas.drawString("PIN Required", 40, (int)itemY + 36);
                }
                else
                {
                    canvas.setTextColor(0x94A3B8); // Muted
                    canvas.drawString("Touch to Reveal", 40, (int)itemY + 36);
                }
            }

            canvas.clearClipRect();

            // 5. Bottom Footer Text (Y = 284)
            canvas.setFont(&fonts::DejaVu9);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(0xFDC0);
            canvas.drawString("OTHERS ENCLAVE · LOCAL STORAGE", w * 0.5f, 290);

            // Screen Slide Transition or Direct push
            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Others), entryFrame, 10);
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

        canvas.deleteSprite();
        return targetScreen;
    }
}
