#include "TasksScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/DataService.h"
#include "../services/TimeService.h"
#include "DetailScreen.h"
#include "Transition.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace VOXA
{
    ScreenId TasksScreen::show(Touch& touch)
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

        ScreenId targetScreen = ScreenId::Tasks;
        uint32_t lastMs = millis();
        int activeTab = 0; // 0: All, 1: Pend, 2: Done


        auto allItems = dataService.getTasks();

        while (targetScreen == ScreenId::Tasks)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            allItems = dataService.getTasks();

            // Filter tasks based on activeTab
            std::vector<TaskItem> items;
            int totalCount = allItems.size();
            int pendCount = 0;
            int doneCount = 0;
            for (const auto& t : allItems)
            {
                if (t.isDone) doneCount++;
                else pendCount++;
            }

            for (const auto& t : allItems)
            {
                if (activeTab == 0) items.push_back(t);
                else if (activeTab == 1 && !t.isDone) items.push_back(t);
                else if (activeTab == 2 && t.isDone) items.push_back(t);
            }

            float contentHeight = items.size() * 56.0f + 10.0f;
            float visibleHeight = 190.0f; // from y=72 to y=262
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
                            float itemY = 74.0f + i * 56.0f - m_scrollY;
                            if (tx >= 10 && tx <= (w - 10) && ty >= itemY && ty <= (itemY + 50.0f))
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
                        m_isAddPressed = false;
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
                            DetailScreen::setItem("tasks", items[m_pressedItemIndex].id, ScreenId::Tasks);
                            targetScreen = ScreenId::Detail;
                        }
                    }
                    m_isBackPressed = false;
                    m_isAddPressed = false;
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
            canvas.setFont(&fonts::Font0);
            canvas.setTextDatum(textdatum_t::top_left);
            canvas.setTextColor(0xFFFF);
            std::string timeStr = timeService.getCurrentTime();
            if (timeStr.empty()) timeStr = "10:42";
            if (timeStr.length() > 5) timeStr = timeStr.substr(0, 5);
            canvas.drawString(timeStr.c_str(), 12, 10);

            canvas.setTextDatum(textdatum_t::top_right);
            canvas.setTextColor(0xFDC0); // Amber tag
            canvas.drawString("TASKS", w - 20, 10);
            // Flash bolt
            canvas.fillTriangle(w - 14, 10, w - 18, 16, w - 13, 16, 0xFDC0);
            canvas.fillTriangle(w - 15, 15, w - 10, 15, w - 14, 21, 0xFDC0);

            // 2. Sub-Header (Y = 28)
            canvas.setTextDatum(textdatum_t::middle_left);
            canvas.setFont(&fonts::Font0);
            canvas.setTextColor(0x94A3B8);
            canvas.drawString("< Hub", 12, 34);

            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextColor(0xFFFF);
            char headerTitle[32];
            snprintf(headerTitle, sizeof(headerTitle), "TASKS (%d)", totalCount);
            canvas.drawString(headerTitle, 56, 33);

            // Filter/Sliders icon on right
            uint16_t iconColor = 0xFDC0;
            canvas.drawLine(w - 24, 28, w - 12, 28, iconColor);
            canvas.fillRect(w - 20, 26, 3, 5, iconColor);
            canvas.drawLine(w - 24, 34, w - 12, 34, iconColor);
            canvas.fillRect(w - 15, 32, 3, 5, iconColor);
            canvas.drawLine(w - 24, 40, w - 12, 40, iconColor);
            canvas.fillRect(w - 22, 38, 3, 5, iconColor);

            // 3. Segmented Filter Tabs (Y = 48..68)
            canvas.fillRoundRect(10, 48, w - 20, 20, 5, canvas.color565(20, 22, 28));
            float tabW = (w - 20.0f) / 3.0f;

            // Active Tab Pill
            canvas.fillRoundRect(10 + activeTab * tabW, 49, tabW, 18, 4, 0xFDC0);

            canvas.setFont(&fonts::Font0);
            canvas.setTextDatum(textdatum_t::middle_center);

            char tAll[20], tPend[20], tDone[20];
            snprintf(tAll, sizeof(tAll), "All (%d)", totalCount);
            snprintf(tPend, sizeof(tPend), "Pend (%d)", pendCount);
            snprintf(tDone, sizeof(tDone), "Done (%d)", doneCount);

            canvas.setTextColor(activeTab == 0 ? 0x0000 : 0x888888);
            canvas.drawString(tAll, 10 + tabW * 0.5f, 58);

            canvas.setTextColor(activeTab == 1 ? 0x0000 : 0x888888);
            canvas.drawString(tPend, 10 + tabW * 1.5f, 58);

            canvas.setTextColor(activeTab == 2 ? 0x0000 : 0x888888);
            canvas.drawString(tDone, 10 + tabW * 2.5f, 58);

            // 4. Scrollable Card List (Y = 72..264)
            canvas.setClipRect(0, 72, w, 194);

            for (std::size_t i = 0; i < items.size(); ++i)
            {
                float itemY = 74.0f + i * 56.0f - m_scrollY;
                if (itemY + 52.0f < 72.0f || itemY > 264.0f)
                    continue;

                bool isPressed = (m_pressedItemIndex == (int)i);

                // Card container
                uint16_t cardBg = isPressed ? canvas.color565(28, 32, 42) : canvas.color565(18, 20, 26);
                uint16_t cardBorder = isPressed ? 0xFDC0 : canvas.color565(34, 38, 48);
                canvas.fillRoundRect(10, (int)itemY, w - 20, 50, 8, cardBg);
                canvas.drawRoundRect(10, (int)itemY, w - 20, 50, 8, cardBorder);

                // Checkbox on left (x = 22, y = itemY + 18)
                int boxX = 20;
                int boxY = (int)itemY + 12;
                int boxS = 14;
                if (items[i].isDone)
                {
                    canvas.fillRoundRect(boxX, boxY, boxS, boxS, 3, 0xFDC0);
                    // Checkmark
                    canvas.drawLine(boxX + 3, boxY + 7, boxX + 6, boxY + 10, 0x0000);
                    canvas.drawLine(boxX + 6, boxY + 10, boxX + 11, boxY + 3, 0x0000);
                }
                else
                {
                    canvas.drawRoundRect(boxX, boxY, boxS, boxS, 3, canvas.color565(100, 116, 139));
                }

                // Line 1: Title (Bold)
                canvas.setFont(&fonts::Font0);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(0xFFFF);

                std::string tTitle = items[i].title;
                if (tTitle.length() > 24) tTitle = tTitle.substr(0, 22) + "..";
                canvas.drawString(tTitle.c_str(), 42, (int)itemY + 8);

                // Line 2: Content/Submemo
                canvas.setTextColor(0x94A3B8);
                std::string subStr = items[i].content;
                if (subStr.empty()) subStr = "Audio summary note";
                if (subStr.length() > 28) subStr = subStr.substr(0, 26) + "..";
                canvas.drawString(subStr.c_str(), 42, (int)itemY + 22);

                // Line 3: Timestamp · Status
                canvas.setTextColor(0x64748B);
                std::string metaStr = items[i].timestamp.empty() ? "Today · Synced" : (items[i].timestamp + " · Synced");
                if (metaStr.length() > 30) metaStr = metaStr.substr(0, 28) + "..";
                canvas.drawString(metaStr.c_str(), 42, (int)itemY + 35);
            }

            canvas.clearClipRect();

            // 5. Bottom Action Button (Y = 272..306)
            uint16_t btnFill = m_isAddPressed ? 0xFFE0 : 0xFDC0;
            canvas.fillRoundRect(10, 272, w - 20, 34, 17, btnFill);
            canvas.setFont(&fonts::Font0);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(0x0000);
            canvas.drawString("+ + QUICK VOICE TASK", w * 0.5f, 289);

            // Screen Slide Transition or Direct push
            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Tasks), entryFrame, 10);
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
