#include "IdeasScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/IdeaService.h"
#include "../services/TimeService.h"
#include "DetailScreen.h"
#include "Transition.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace VOXA
{
    extern IdeaService ideaService;

    ScreenId IdeasScreen::show(Touch& touch)
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

        ScreenId targetScreen = ScreenId::Ideas;
        uint32_t lastMs = millis();
        int activeTab = 0; // 0: Recent, 1: Dumps, 2: AI Synth


        auto ideas = ideaService.getAll();

        while (targetScreen == ScreenId::Ideas)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            ideas = ideaService.getAll();
            int totalCount = ideas.size();

            std::vector<Idea> items;
            for (const auto& idea : ideas)
            {
                items.push_back(idea);
            }

            float contentHeight = items.size() * 56.0f + 10.0f;
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
                            DetailScreen::setItem("ideas", items[m_pressedItemIndex].id, ScreenId::Ideas);
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
            canvas.drawString("NPU READY", w - 20, 10);
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
            snprintf(headerTitle, sizeof(headerTitle), "IDEAS (%d)", totalCount);
            canvas.drawString(headerTitle, 56, 33);

            // Spark / Lightbulb icon on right
            uint16_t icColor = 0xFDC0;
            canvas.fillCircle(w - 18, 30, 4, icColor);
            canvas.fillRect(w - 20, 34, 4, 3, icColor);
            canvas.drawLine(w - 19, 38, w - 17, 38, icColor);

            // 3. Segmented Filter Tabs (Y = 48..68)
            canvas.fillRoundRect(10, 48, w - 20, 20, 5, canvas.color565(20, 22, 28));
            float tabW = (w - 20.0f) / 3.0f;

            // Active Tab Pill
            canvas.fillRoundRect(10 + activeTab * tabW, 49, tabW, 18, 4, 0xFDC0);

            canvas.setFont(&fonts::Font0);
            canvas.setTextDatum(textdatum_t::middle_center);

            canvas.setTextColor(activeTab == 0 ? 0x0000 : 0x888888);
            canvas.drawString("Recent", 10 + tabW * 0.5f, 58);

            canvas.setTextColor(activeTab == 1 ? 0x0000 : 0x888888);
            canvas.drawString("Dumps", 10 + tabW * 1.5f, 58);

            canvas.setTextColor(activeTab == 2 ? 0x0000 : 0x888888);
            canvas.drawString("AI Synth", 10 + tabW * 2.5f, 58);

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

                // Line 1: Title
                canvas.setFont(&fonts::Font0);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(0xFFFF);

                std::string tTitle = items[i].title;
                if (tTitle.length() > 22) tTitle = tTitle.substr(0, 20) + "..";
                canvas.drawString(tTitle.c_str(), 18, (int)itemY + 10);

                // Line 2: Content / Summary
                canvas.setTextColor(0x94A3B8);
                std::string subStr = items[i].content;
                if (subStr.empty()) subStr = "Synthesized idea note";
                if (subStr.length() > 28) subStr = subStr.substr(0, 26) + "..";
                canvas.drawString(subStr.c_str(), 18, (int)itemY + 26);

                // Right Badge Pill (e.g. 43s Note, In AI, Idea)
                canvas.fillRoundRect(w - 58, (int)itemY + 14, 42, 18, 4, canvas.color565(26, 30, 38));
                canvas.drawRoundRect(w - 58, (int)itemY + 14, 42, 18, 4, canvas.color565(44, 50, 64));
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(0xCBD5E1);
                const char* badgeLabels[] = { "43s Note", "In AI", "Idea", "Spark" };
                canvas.drawString(badgeLabels[i % 4], w - 37, (int)itemY + 23);
            }

            canvas.clearClipRect();

            // 5. Bottom Action Button (Y = 272..306)
            uint16_t btnBg = m_isAddPressed ? 0xFFE0 : 0xFDC0;
            canvas.fillRoundRect(10, 272, w - 20, 34, 17, btnBg);

            canvas.setFont(&fonts::Font0);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(0x0000);
            canvas.drawString("+ VOICE SPARK", w * 0.5f, 289);

            // Screen Slide Transition or Direct push
            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Ideas), entryFrame, 10);
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
