#include "OthersScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/MemoryService.h"
#include "Transition.h"
#include "../services/WiFiManager.h"
#include "../services/DataService.h"
#include "DetailScreen.h"
#include <cmath>
#include <algorithm>

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

        auto memories = memoryService.getAll();
        float contentHeight = memories.size() * 50.0f + 10.0f;
        float visibleHeight = h - 70.0f - 18.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        // Selection & Action states
        bool isSelectMode = false;
        std::vector<bool> selectedItems;
        uint32_t pressStartMs = 0;
        bool longPressTriggered = false;

        bool isMovePopupActive = false;
        int moveTargetIndex = -1;

        bool m_isPinPressed = false;
        bool m_isMovePressed = false;
        bool m_isDeleteModePressed = false;
        bool isAddPressed = false;

        while (targetScreen == ScreenId::Others)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            // Handle background sync update
            memories = memoryService.getAll();
            contentHeight = memories.size() * 50.0f + 10.0f;

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
                    pressStartMs = nowMs;
                    longPressTriggered = false;

                    if (isMovePopupActive)
                    {
                        float startPopupY = h * 0.38f;
                        float optionH = 26.0f;
                        for (int opt = 0; opt < 3; ++opt)
                        {
                            float optY = startPopupY + opt * optionH;
                            if (tx >= w * 0.1f && tx <= w * 0.9f && ty >= optY - 8.0f && ty <= optY + 18.0f)
                            {
                                moveTargetIndex = opt;
                            }
                        }
                    }
                    else
                    {
                        // Back button bounds
                        if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                        {
                            m_isBackPressed = true;
                        }

                        // Add button bounds
                        if (std::sqrt((tx - (w - 20.0f))*(tx - (w - 20.0f)) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                        {
                            isAddPressed = true;
                        }

                        // Action Bar bounds (bottom Y = h - 28)
                        if (isSelectMode && ty >= h - 55.0f)
                        {
                            float actY = h - 28.0f;
                            if (std::sqrt((tx - w*0.22f)*(tx - w*0.22f) + (ty - actY)*(ty - actY)) <= 18.0f)
                            {
                                m_isPinPressed = true;
                            }
                            else if (std::sqrt((tx - w*0.50f)*(tx - w*0.50f) + (ty - actY)*(ty - actY)) <= 18.0f)
                            {
                                m_isMovePressed = true;
                            }
                            else if (std::sqrt((tx - w*0.78f)*(tx - w*0.78f) + (ty - actY)*(ty - actY)) <= 18.0f)
                            {
                                m_isDeleteModePressed = true;
                            }
                        }

                        // Card checks
                        if (ty >= 70.0f && ty <= (h - 18.0f) && (!isSelectMode || ty < h - 55.0f))
                        {
                            float leftX = w * 0.04f;
                            float cardW = w * 0.92f;
                            for (std::size_t i = 0; i < memories.size(); ++i)
                            {
                                float itemY = 72.0f + i * 50.0f - m_scrollY;
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
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        targetScreen = ScreenId::Home;
                        swipeBackCandidate = false;
                    }

                    float dy = ty - m_dragStartY;
                    if (!m_isDragging && std::abs(dy) > 10.0f && !isMovePopupActive)
                    {
                        m_isDragging = true;
                        m_isBackPressed = false;
                        isAddPressed = false;
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

                    // Long press detection to enter select mode
                    if (!isSelectMode && m_pressedItemIndex != -1 && !m_isDragging && !longPressTriggered && (nowMs - pressStartMs > 500))
                    {
                        isSelectMode = true;
                        longPressTriggered = true;
                        selectedItems.clear();
                        selectedItems.resize(memories.size(), false);
                        if (m_pressedItemIndex >= 0 && m_pressedItemIndex < (int)selectedItems.size())
                        {
                            selectedItems[m_pressedItemIndex] = true;
                        }
                        m_pressedItemIndex = -1;
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
                        if (m_scrollY < -50.0f && wifiManager.isConnected() && !isSelectMode)
                        {
                            Serial.println("[Others] Triggering manual pull-to-refresh sync...");
                            xTaskCreatePinnedToCore([](void*){
                                VOXA::dataService.syncAll();
                                vTaskDelete(NULL);
                            }, "ManualSync", 4096, NULL, 1, NULL, 0);
                        }
                    }
                    else
                    {
                        if (isMovePopupActive)
                        {
                            if (moveTargetIndex != -1)
                            {
                                std::string targets[3] = {"reminders", "ideas", "questions"};
                                std::string targetCat = targets[moveTargetIndex];
                                for (size_t i = 0; i < selectedItems.size(); ++i)
                                {
                                    if (selectedItems[i] && i < memories.size())
                                    {
                                        dataService.moveItem("others", targetCat, memories[i].id);
                                    }
                                }
                                isSelectMode = false;
                                isMovePopupActive = false;
                                selectedItems.clear();
                                memories = memoryService.getAll();
                            }
                            else
                            {
                                isMovePopupActive = false;
                            }
                            moveTargetIndex = -1;
                        }
                        else if (isSelectMode)
                        {
                            if (m_isBackPressed)
                            {
                                isSelectMode = false;
                                selectedItems.clear();
                            }
                            else if (isAddPressed)
                            {
                                bool anyUnselected = std::any_of(selectedItems.begin(), selectedItems.end(), [](bool val){ return !val; });
                                std::fill(selectedItems.begin(), selectedItems.end(), anyUnselected);
                            }
                            else if (m_pressedItemIndex >= 0 && m_pressedItemIndex < (int)selectedItems.size())
                            {
                                selectedItems[m_pressedItemIndex] = !selectedItems[m_pressedItemIndex];
                            }
                            else if (m_isPinPressed)
                            {
                                for (size_t i = 0; i < selectedItems.size(); ++i)
                                {
                                    if (selectedItems[i] && i < memories.size())
                                    {
                                        dataService.togglePin("others", memories[i].id);
                                    }
                                }
                                isSelectMode = false;
                                selectedItems.clear();
                                memories = memoryService.getAll();
                            }
                            else if (m_isMovePressed)
                            {
                                isMovePopupActive = true;
                            }
                            else if (m_isDeleteModePressed)
                            {
                                for (size_t i = 0; i < selectedItems.size(); ++i)
                                {
                                    if (selectedItems[i] && i < memories.size())
                                    {
                                        memoryService.remove(memories[i].id);
                                    }
                                }
                                isSelectMode = false;
                                selectedItems.clear();
                                memories = memoryService.getAll();
                            }

                            m_isBackPressed = false;
                            isAddPressed = false;
                            m_pressedItemIndex = -1;
                            m_isPinPressed = false;
                            m_isMovePressed = false;
                            m_isDeleteModePressed = false;
                        }
                        else
                        {
                            if (m_isBackPressed)
                            {
                                targetScreen = ScreenId::Home;
                            }
                            else if (isAddPressed)
                            {
                                int count = memories.size() + 1;
                                Memory m;
                                m.title = "New Memory " + std::to_string(count);
                                m.content = "Memory details go here.";
                                m.timestamp = "Jul 07";
                                m.category = "note";
                                memoryService.add(m);
                                Serial.println("[Others] Added new memory");
                                memories = memoryService.getAll();
                            }
                            else if (m_pressedItemIndex >= 0 && m_pressedItemIndex < (int)memories.size())
                            {
                                DetailScreen::setItem("memories", memories[m_pressedItemIndex].id, ScreenId::Others);
                                targetScreen = ScreenId::Detail;
                            }
                            m_isBackPressed = false;
                            isAddPressed = false;
                            m_pressedItemIndex = -1;
                        }
                    }
                }
            }

            // Dimensions re-query & scrolling inertia
            w = Display::width();
            h = Display::height();
            visibleHeight = h - 70.0f - (isSelectMode ? 55.0f : 18.0f);
            maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

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

            // Render
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, isSelectMode ? "Select Items" : "Others", true, true, isSelectMode ? Icon::Plus : Icon::Plus, w, h);

            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, 
                                              backFill, backColor, w, h);

            uint16_t addFill = isAddPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t addColor = isAddPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, w - 20.0f, 45.0f, isSelectMode ? Icon::Plus : Icon::Plus, 
                                              addFill, addColor, w, h);

            float leftX = w * 0.04f;
            float cardW = w * 0.92f;

            canvas.setClipRect(0, 70, w, visibleHeight);

            if (m_scrollY < 0.0f && !isSelectMode)
            {
                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(VoxaTheme::getTextSecondary());
                const char* msg = (m_scrollY < -50.0f) ? "Release to sync" : "Pull to sync";
                canvas.drawString(msg, w * 0.5f, 72.0f - m_scrollY - 22.0f);
            }

            for (std::size_t i = 0; i < memories.size(); ++i)
            {
                float itemY = 72.0f + i * 50.0f - m_scrollY;
                if (itemY + 44.0f < 70.0f || itemY > (70.0f + visibleHeight))
                    continue;

                bool isPressed = (m_pressedItemIndex == (int)i);
                bool isItemChecked = isSelectMode && i < selectedItems.size() && selectedItems[i];

                uint16_t cardBg = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                uint16_t cardBorder = isPressed ? VoxaTheme::getPrimaryLight() : (isItemChecked ? VoxaTheme::getPrimary() : VoxaTheme::getDivider());
                uint16_t labelColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
                uint16_t subColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextSecondary();

                canvas.fillRoundRect((int)leftX, (int)itemY, (int)cardW, 44, 8, cardBg);
                canvas.drawRoundRect((int)leftX, (int)itemY, (int)cardW, 44, 8, cardBorder);

                float cy = itemY + 22.0f;
                float iconCx = leftX + 22.0f;
                float textOffset = 42.0f;

                if (isSelectMode)
                {
                    // Draw selection checkbox circle
                    canvas.drawCircle((int)iconCx, (int)cy, 8, isItemChecked ? VoxaTheme::getPrimary() : VoxaTheme::getDivider());
                    if (isItemChecked)
                    {
                        canvas.fillCircle((int)iconCx, (int)cy, 5, VoxaTheme::getPrimary());
                    }
                    textOffset = 38.0f;
                }
                else
                {
                    canvas.fillCircle((int)iconCx, (int)cy, 12, 0xAD55);
                    ScreenCommon::drawIcon(canvas, Icon::Folder, iconCx - 6.0f, cy - 6.0f, 12.0f, VoxaTheme::getBackground());
                }

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextDatum(textdatum_t::middle_left);
                canvas.setTextColor(labelColor);

                std::string drawTitle = memories[i].title;
                if (drawTitle.length() > 15) drawTitle = drawTitle.substr(0, 13) + "...";

                canvas.drawString(drawTitle.c_str(), leftX + textOffset, cy - 8.0f);

                canvas.setTextColor(subColor);
                canvas.drawString(memories[i].timestamp.c_str(), leftX + textOffset, cy + 8.0f);

                // Draw Pin indicator if pinned
                if (memories[i].pinned)
                {
                    float pinX = leftX + cardW - 28.0f;
                    ScreenCommon::drawIcon(canvas, Icon::Star, pinX - 5.0f, cy - 5.0f, 10.0f, 0xFD20);
                }
                else if (!isSelectMode)
                {
                    float chevX = leftX + cardW - 16.0f;
                    ScreenCommon::drawIcon(canvas, Icon::ChevronRight, chevX - 5.0f, cy - 5.0f, 10.0f, subColor);
                }
            }

            canvas.clearClipRect();

            // Bottom Action Bar
            if (isSelectMode)
            {
                float actY = h - 28.0f;
                canvas.fillRect(0, h - 56, w, 56, VoxaTheme::getSurface());
                canvas.drawFastHLine(0, h - 56, w, VoxaTheme::getDivider());

                // 1. PIN button
                uint16_t pinBg = m_isPinPressed ? VoxaTheme::getPrimary() : VoxaTheme::getBackground();
                uint16_t pinColor = m_isPinPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
                canvas.fillCircle((int)(w * 0.22f), (int)actY, 18, pinBg);
                canvas.drawCircle((int)(w * 0.22f), (int)actY, 18, VoxaTheme::getDivider());
                ScreenCommon::drawIcon(canvas, Icon::Star, w * 0.22f - 9.0f, actY - 9.0f, 18.0f, pinColor);

                // 2. MOVE button
                uint16_t movBg = m_isMovePressed ? VoxaTheme::getPrimary() : VoxaTheme::getBackground();
                uint16_t movColor = m_isMovePressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
                canvas.fillCircle((int)(w * 0.50f), (int)actY, 18, movBg);
                canvas.drawCircle((int)(w * 0.50f), (int)actY, 18, VoxaTheme::getDivider());
                ScreenCommon::drawIcon(canvas, Icon::Folder, w * 0.50f - 9.0f, actY - 9.0f, 18.0f, movColor);

                // 3. DELETE button
                uint16_t delBg = m_isDeleteModePressed ? VoxaTheme::getPrimary() : VoxaTheme::getBackground();
                uint16_t delColor = m_isDeleteModePressed ? VoxaTheme::getBackground() : VoxaTheme::getWarning();
                canvas.fillCircle((int)(w * 0.78f), (int)actY, 18, delBg);
                canvas.drawCircle((int)(w * 0.78f), (int)actY, 18, VoxaTheme::getDivider());
                
                // Draw simple dustbin shape
                canvas.fillRect((int)(w * 0.78f - 5.0f), (int)(actY - 5.0f), 10, 2, delColor);
                canvas.fillRect((int)(w * 0.78f - 4.0f), (int)(actY - 2.0f), 8, 8, delColor);
            }

            // Move target selection modal dialog
            if (isMovePopupActive)
            {
                canvas.fillRect(0, 0, w, h, canvas.color565(8, 6, 15));

                float dW = w * 0.88f;
                float dH = 120.0f;
                float dX = w * 0.06f;
                float dY = h * 0.30f;

                canvas.fillRoundRect((int)dX, (int)dY, (int)dW, (int)dH, 12, VoxaTheme::getSurface());
                canvas.drawRoundRect((int)dX, (int)dY, (int)dW, (int)dH, 12, VoxaTheme::getDivider());

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::top_center);
                canvas.setTextColor(TFT_WHITE);
                canvas.drawString("Move selected items to:", dX + dW*0.5f, dY + 12.0f);

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextDatum(textdatum_t::middle_center);
                
                // Target categories
                std::string targetLabels[3] = {"Reminders", "Ideas", "Questions"};
                float startPopupY = dY + 45.0f;
                for (int opt = 0; opt < 3; ++opt)
                {
                    uint16_t rowCol = (moveTargetIndex == opt) ? VoxaTheme::getPrimary() : VoxaTheme::getTextPrimary();
                    canvas.setTextColor(rowCol);
                    canvas.drawString(targetLabels[opt].c_str(), dX + dW * 0.5f, startPopupY + opt * 24.0f);
                }
            }

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
