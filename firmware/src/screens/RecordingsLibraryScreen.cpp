#include "RecordingsLibraryScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/RecordingService.h"
#include "Transition.h"
#include "AudioPlayerScreen.h"
#include <cmath>
#include <algorithm>

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

        auto recordings = recordingService.getAll();
        float contentHeight = recordings.size() * 50.0f + 10.0f;
        float visibleHeight = h - 70.0f - 18.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);
        uint32_t lastDataRefreshMs = millis();

        while (targetScreen == ScreenId::RecordingsLibrary)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            // Only refresh data every 2 seconds or after deletion (not every 16ms frame)
            if (nowMs - lastDataRefreshMs > 2000)
            {
                recordings = recordingService.getAll();
                contentHeight = recordings.size() * 50.0f + 10.0f;
                maxScrollY = std::max(0.0f, contentHeight - visibleHeight);
                lastDataRefreshMs = nowMs;
            }

            // 1. Process Touch
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

                    // If deletion dialog is NOT active
                    if (m_selectedDeleteIndex == -1)
                    {
                        // Back button bounds Y = 45
                        if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                        {
                            m_isBackPressed = true;
                        }

                        // Card checks
                        if (ty >= 70.0f && ty <= (h - 18.0f))
                        {
                            float leftX = w * 0.04f;
                            float cardW = w * 0.92f;
                            for (std::size_t i = 0; i < recordings.size(); ++i)
                            {
                                float itemY = 72.0f + i * 50.0f - m_scrollY;
                                if (tx >= leftX && tx <= (leftX + cardW) &&
                                    ty >= itemY && ty <= (itemY + 44.0f))
                                {
                                    m_pressedItemIndex = (int)i;
                                }
                            }
                        }
                    }
                    else
                    {
                        // Deletion dialog is active
                        float cardW = w * 0.88f;
                        float cardH = 90.0f;
                        float cardX = w * 0.06f;
                        float cardY = h * 0.55f;

                        // Confirm button bounds: right side
                        float yesX = cardX + cardW * 0.55f;
                        float yesY = cardY + 50.0f;
                        if (tx >= yesX && tx <= (yesX + cardW * 0.38f) &&
                            ty >= yesY && ty <= (yesY + 28.0f))
                        {
                            m_isConfirmDeletePressed = true;
                        }

                        // Cancel button bounds: left side
                        float noX = cardX + cardW * 0.07f;
                        float noY = cardY + 50.0f;
                        if (tx >= noX && tx <= (noX + cardW * 0.38f) &&
                            ty >= noY && ty <= (noY + 28.0f))
                        {
                            m_isCancelDeletePressed = true;
                        }
                    }
                }
                else
                {
                    // Dragging & scrolling
                    if (m_selectedDeleteIndex == -1)
                    {
                        float dy = ty - m_dragStartY;
                        if (!m_isDragging && std::abs(dy) > 5.0f)
                        {
                            m_isDragging = true;
                            m_pressedItemIndex = -1;
                            m_isBackPressed = false;
                        }

                        if (m_isDragging)
                        {
                            m_targetScrollY = m_dragStartScrollY - dy;
                            if (m_targetScrollY < -20.0f) m_targetScrollY = -20.0f + (m_targetScrollY + 20.0f) * 0.3f;
                            if (m_targetScrollY > maxScrollY + 20.0f) m_targetScrollY = maxScrollY + 20.0f + (m_targetScrollY - maxScrollY - 20.0f) * 0.3f;

                            uint32_t dt = nowMs - m_lastTouchSampleMs;
                            if (dt > 0)
                            {
                                m_scrollVelocity = -dy / (dt / 1000.0f);
                                m_lastTouchSampleMs = nowMs;
                            }
                        }
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;
                    
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        targetScreen = ScreenId::Home;
                        swipeBackCandidate = false;
                    }

                    if (m_isBackPressed)
                    {
                        targetScreen = ScreenId::Home;
                        m_isBackPressed = false;
                    }

                    if (m_pressedItemIndex != -1)
                    {
                        if (m_pressedItemIndex >= 0 && m_pressedItemIndex < (int)recordings.size())
                        {
                            AudioPlayerScreen::setRecording(recordings[m_pressedItemIndex].id, ScreenId::RecordingsLibrary);
                            targetScreen = ScreenId::AudioPlayer;
                        }
                        m_pressedItemIndex = -1;
                    }

                    if (m_isConfirmDeletePressed)
                    {
                        m_isConfirmDeletePressed = false;
                        if (m_selectedDeleteIndex >= 0 && m_selectedDeleteIndex < (int)recordings.size())
                        {
                            recordingService.remove(recordings[m_selectedDeleteIndex].id);
                            recordings = recordingService.getAll();
                            contentHeight = recordings.size() * 50.0f + 10.0f;
                            maxScrollY = std::max(0.0f, contentHeight - visibleHeight);
                        }
                        m_selectedDeleteIndex = -1;
                    }

                    if (m_isCancelDeletePressed)
                    {
                        m_isCancelDeletePressed = false;
                        m_selectedDeleteIndex = -1;
                    }
                }

                // Scroll rebound inertia physics
                if (m_selectedDeleteIndex == -1)
                {
                    if (m_targetScrollY < 0.0f)
                    {
                        m_targetScrollY += (0.0f - m_targetScrollY) * 12.0f * deltaSecs;
                    }
                    else if (m_targetScrollY > maxScrollY)
                    {
                        m_targetScrollY += (maxScrollY - m_targetScrollY) * 12.0f * deltaSecs;
                    }

                    if (std::abs(m_scrollVelocity) > 0.0f)
                    {
                        m_targetScrollY += m_scrollVelocity * deltaSecs;
                        m_scrollVelocity *= std::pow(0.88f, deltaSecs * 60.0f);
                        if (std::abs(m_scrollVelocity) < 10.0f) m_scrollVelocity = 0.0f;
                    }
                }
            }

            m_scrollY += (m_targetScrollY - m_scrollY) * 15.0f * deltaSecs;

            // Dimensions re-query
            w = Display::width();
            h = Display::height();

            // 2. Render UI Surface
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Voice Library", true, false, Icon::Mic, w, h);

            // Render Back button
            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, 
                                              backFill, backColor, w, h);

            float leftX = w * 0.04f;
            float cardW = w * 0.92f;

            canvas.setClipRect(0, 70, w, h - 70 - 18);

            if (recordings.empty())
            {
                // Empty state card
                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextColor(VoxaTheme::getTextSecondary());
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.drawString("No Voice Recordings", w * 0.5f, h * 0.48f);
                canvas.drawString("Hold physical button to record", w * 0.5f, h * 0.58f);
            }
            else
            {
                for (std::size_t i = 0; i < recordings.size(); ++i)
                {
                    float itemY = 72.0f + i * 50.0f - m_scrollY;
                    if (itemY + 44.0f < 70.0f || itemY > (h - 18.0f))
                        continue;

                    bool isPressed = (m_pressedItemIndex == (int)i);
                    uint16_t cardBg = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                    uint16_t cardBorder = isPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getDivider();
                    uint16_t labelColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
                uint16_t subColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextSecondary();

                canvas.fillRoundRect((int)leftX, (int)itemY, (int)cardW, 44, 8, cardBg);
                canvas.drawRoundRect((int)leftX, (int)itemY, (int)cardW, 44, 8, cardBorder);

                float cy = itemY + 22.0f;
                float iconCx = leftX + 22.0f;

                // Color based on status (Pending gets Amber/Yellow badge, Completed gets Green)
                bool isPending = (recordings[i].timestamp == "Pending");
                uint16_t statusBadgeColor = isPending ? 0xFBE0 : 0x2508; // Yellow vs Green

                canvas.fillCircle((int)iconCx, (int)cy, 12, statusBadgeColor);
                ScreenCommon::drawIcon(canvas, Icon::Mic, iconCx - 6.0f, cy - 6.0f, 12.0f, VoxaTheme::getBackground());

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextDatum(textdatum_t::middle_left);
                
                canvas.setTextColor(labelColor);
                std::string titleStr = recordings[i].title;
                if (titleStr.length() > 14) titleStr = titleStr.substr(0, 12) + "...";
                canvas.drawString(titleStr.c_str(), leftX + 42.0f, cy - 8.0f);

                canvas.setTextColor(subColor);
                std::string timeStr = isPending ? "Pending sync" : recordings[i].timestamp;
                canvas.drawString(timeStr.c_str(), leftX + 42.0f, cy + 8.0f);

                float chevX = leftX + cardW - 16.0f;
                ScreenCommon::drawIcon(canvas, Icon::ChevronRight, chevX - 5.0f, cy - 5.0f, 10.0f, subColor);
            }
            }

            canvas.clearClipRect();

            // 3. Render Deletion Modal overlay if selected
            if (m_selectedDeleteIndex != -1)
            {
                // Darken / dim the background
                canvas.fillScreen(canvas.color565(5, 3, 10));

                float dW = w * 0.88f;
                float dH = 95.0f;
                float dX = w * 0.06f;
                float dY = h * 0.50f;

                canvas.fillRoundRect((int)dX, (int)dY, (int)dW, (int)dH, 12, VoxaTheme::getSurface());
                canvas.drawRoundRect((int)dX, (int)dY, (int)dW, (int)dH, 12, VoxaTheme::getDivider());

                canvas.setFont(&fonts::FreeSansBold12pt7b);
                canvas.setTextDatum(textdatum_t::top_center);
                canvas.setTextColor(TFT_WHITE);
                canvas.drawString("Delete voice memo?", dX + dW*0.5f, dY + 12.0f);

                // Cancel Button (grey)
                uint16_t btnCancelCol = m_isCancelDeletePressed ? 0x5B5F : 0x2965;
                float cancelX = dX + dW * 0.07f;
                float cancelY = dY + 50.0f;
                float btnW = dW * 0.38f;
                canvas.fillRoundRect((int)cancelX, (int)cancelY, (int)btnW, 28, 6, btnCancelCol);
                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(TFT_WHITE);
                canvas.drawString("Cancel", cancelX + btnW*0.5f, cancelY + 14.0f);

                // Delete Button (red)
                uint16_t btnDelCol = m_isConfirmDeletePressed ? 0xF800 : 0xB800;
                float delX = dX + dW * 0.55f;
                float delY = dY + 50.0f;
                canvas.fillRoundRect((int)delX, (int)delY, (int)btnW, 28, 6, btnDelCol);
                canvas.drawString("Delete", delX + btnW*0.5f, delY + 14.0f);
            }

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
