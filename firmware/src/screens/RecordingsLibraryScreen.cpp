#include "RecordingsLibraryScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/RecordingService.h"
#include "../services/TimeService.h"
#include "Transition.h"
#include "AudioPlayerScreen.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace VOXA
{
    extern RecordingService recordingService;

    ScreenId RecordingsLibraryScreen::show(Touch& touch)
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

        ScreenId targetScreen = ScreenId::RecordingsLibrary;
        uint32_t lastMs = millis();
        int activeTab = 0; // 0: All, 1: Star, 2: Meet, 3: Memo
        bool isRecTriggerPressed = false;

        auto recordings = recordingService.getAll();
        uint32_t lastDataRefreshMs = millis();

        while (targetScreen == ScreenId::RecordingsLibrary)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            if (nowMs - lastDataRefreshMs > 2000)
            {
                recordings = recordingService.getAll();
                lastDataRefreshMs = nowMs;
            }

            int totalCount = recordings.size();

            std::vector<Recording> items;
            for (const auto& rec : recordings)
            {
                items.push_back(rec);
            }

            float contentHeight = items.size() * 56.0f + 10.0f;
            float visibleHeight = (float)h - 72.0f - 8.0f;
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
                        float tabW = (w - 20.0f) / 4.0f;
                        if (tx >= 10 && tx < 10 + tabW) activeTab = 0;
                        else if (tx >= 10 + tabW && tx < 10 + 2 * tabW) activeTab = 1;
                        else if (tx >= 10 + 2 * tabW && tx < 10 + 3 * tabW) activeTab = 2;
                        else if (tx >= 10 + 3 * tabW && tx <= w - 10) activeTab = 3;
                        m_targetScrollY = 0.0f;
                        m_scrollY = 0.0f;
                    }

                    // Card checks
                    if (ty >= 72 && ty <= h - 8)
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
                        isRecTriggerPressed = false;
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
                            AudioPlayerScreen::setRecording(items[m_pressedItemIndex].id, ScreenId::RecordingsLibrary);
                            targetScreen = ScreenId::AudioPlayer;
                        }
                    }
                    m_isBackPressed = false;
                    isRecTriggerPressed = false;
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
            canvas.drawString("SD: 12.4GB", w - 20, 10);
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
            snprintf(headerTitle, sizeof(headerTitle), "RECS (%d)", totalCount);
            canvas.drawString(headerTitle, w - 34, 33);

            // Sliders / list icon on right
            uint16_t iconColor = 0xFDC0;
            canvas.drawLine(w - 24, 28, w - 12, 28, iconColor);
            canvas.drawLine(w - 24, 34, w - 15, 34, iconColor);
            canvas.drawLine(w - 24, 40, w - 18, 40, iconColor);

            // 3. Segmented Filter Tabs (Y = 48..68)
            canvas.fillRoundRect(10, 48, w - 20, 20, 5, canvas.color565(18, 21, 28));
            canvas.drawRoundRect(10, 48, w - 20, 20, 5, canvas.color565(32, 38, 50));
            float tabW = (w - 20.0f) / 4.0f;

            // Active Tab Pill
            canvas.fillRoundRect(10 + activeTab * tabW, 49, tabW, 18, 4, 0xFDC0);

            canvas.setFont(&fonts::DejaVu9);
            canvas.setTextDatum(textdatum_t::middle_center);

            const char* tabNames[] = { "All", "Star", "Meet", "Memo" };
            for (int t = 0; t < 4; ++t)
            {
                canvas.setTextColor(activeTab == t ? 0x0000 : 0x888888);
                canvas.drawString(tabNames[t], 10 + tabW * (t + 0.5f), 58);
            }

            // 4. Scrollable Card List
            canvas.setClipRect(0, 72, w, h - 72 - 4);

            for (std::size_t i = 0; i < items.size(); ++i)
            {
                float itemY = 74.0f + i * 56.0f - m_scrollY;
                if (itemY + 52.0f < 72.0f || itemY > (float)h)
                    continue;

                bool isPressed = (m_pressedItemIndex == (int)i);

                // Card container
                uint16_t cardBg = isPressed ? canvas.color565(28, 32, 42) : canvas.color565(18, 21, 28);
                uint16_t cardBorder = isPressed ? 0xFDC0 : canvas.color565(32, 38, 50);
                canvas.fillRoundRect(10, (int)itemY, w - 20, 50, 8, cardBg);
                canvas.drawRoundRect(10, (int)itemY, w - 20, 50, 8, cardBorder);

                // Waveform / Mic icon on left
                int icX = 24;
                int icY = (int)itemY + 25;
                canvas.drawFastVLine(icX - 4, icY - 3, 6, 0xFDC0);
                canvas.drawFastVLine(icX - 2, icY - 6, 12, 0xFDC0);
                canvas.drawFastVLine(icX, icY - 8, 16, 0xFDC0);
                canvas.drawFastVLine(icX + 2, icY - 5, 10, 0xFDC0);
                canvas.drawFastVLine(icX + 4, icY - 2, 4, 0xFDC0);

                // Line 1: Filename / Title
                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(0xFFFF);

                std::string tTitle = items[i].title;
                if (tTitle.find('.') == std::string::npos) tTitle += ".wav";
                if (tTitle.length() > 22) tTitle = tTitle.substr(0, 20) + "..";
                canvas.drawString(tTitle.c_str(), 40, (int)itemY + 10);

                // Line 2: Meta / Duration / Status (Muted Gold)
                canvas.setTextColor(canvas.color565(194, 155, 80));
                char metaBuf[48];
                uint32_t dur = items[i].durationSeconds;
                snprintf(metaBuf, sizeof(metaBuf), "%02d:%02d · 18.4MB · Today", dur / 60, dur % 60);
                canvas.drawString(metaBuf, 40, (int)itemY + 26);

                // Right Play Button Circle (Amber / Dark)
                int playCx = w - 30;
                int playCy = (int)itemY + 25;
                canvas.drawCircle(playCx, playCy, 10, 0xFDC0);
                canvas.fillTriangle(playCx - 2, playCy - 4, playCx + 4, playCy, playCx - 2, playCy + 4, 0xFDC0);
            }

            canvas.clearClipRect();

            // Screen Slide Transition or Direct push
            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::RecordingsLibrary), entryFrame, 10);
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
