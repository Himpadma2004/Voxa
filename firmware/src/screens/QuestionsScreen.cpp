#include "QuestionsScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/QuestionService.h"
#include "DetailScreen.h"
#include <cmath>
#include <algorithm>

namespace VOXA
{
    extern QuestionService questionService;

    ScreenId QuestionsScreen::show(Touch& touch)
    {
        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            return ScreenId::Home;
        }

        ScreenId targetScreen = ScreenId::Questions;
        uint32_t lastMs = millis();

        auto questions = questionService.getAll();
        float contentHeight = questions.size() * 50.0f + 10.0f;
        float visibleHeight = h - 70.0f - 18.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        while (targetScreen == ScreenId::Questions)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            questions = questionService.getAll();
            contentHeight = questions.size() * 50.0f + 10.0f;

            // 1. Process Touch
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            if (touched)
            {
                m_lastDragX = tx;
                m_lastDragY = ty;

                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    m_dragStartY = ty;
                    m_dragStartScrollY = m_targetScrollY;
                    m_lastTouchSampleMs = nowMs;
                    m_isDragging = false;
                    m_scrollVelocity = 0.0f;

                    // Back button bounds Y = 45
                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isBackPressed = true;
                    }

                    // Add button bounds Y = 45
                    if (std::sqrt((tx - (w - 20.0f))*(tx - (w - 20.0f)) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isAddPressed = true;
                    }

                    // Card checks
                    if (ty >= 70.0f && ty <= (h - 18.0f))
                    {
                        float leftX = w * 0.04f;
                        float cardW = w * 0.92f;
                        for (std::size_t i = 0; i < questions.size(); ++i)
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
                else
                {
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
                    float rx = m_lastDragX;
                    float ry = m_lastDragY;

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
                        else if (m_isAddPressed)
                        {
                            // Add a new placeholder question
                            int count = questions.size() + 1;
                            std::string text = "New Question " + std::to_string(count);
                            questionService.add(text, "No answer yet.", "Jul 07");
                            Serial.println("[Questions] Added new question");
                        }
                        else if (m_pressedItemIndex >= 0 && m_pressedItemIndex < (int)questions.size())
                        {
                            DetailScreen::setItem("questions", questions[m_pressedItemIndex].id, ScreenId::Questions);
                            targetScreen = ScreenId::Detail;
                        }
                    }
                    m_isBackPressed = false;
                    m_isAddPressed = false;
                    m_pressedItemIndex = -1;
                }
            }

            // Dimensions re-query
            w = Display::width();
            h = Display::height();
            visibleHeight = h - 70.0f - 18.0f;
            maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            // 2. Perform Scroll Inertia
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

            // 3. Render Questions Screen
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Questions", true, true, Icon::Plus, w, h);

            // Header highlights
            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, 
                                              backFill, backColor, w, h);

            uint16_t addFill = m_isAddPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t addColor = m_isAddPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, w - 20.0f, 45.0f, Icon::Plus, 
                                              addFill, addColor, w, h);

            float leftX = w * 0.04f;
            float cardW = w * 0.92f;

            canvas.setClipRect(0, 70, w, h - 70 - 18);

            for (std::size_t i = 0; i < questions.size(); ++i)
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
                canvas.fillCircle((int)iconCx, (int)cy, 12, 0x067F);
                ScreenCommon::drawIcon(canvas, Icon::Question, iconCx - 6.0f, cy - 6.0f, 12.0f, VoxaTheme::getBackground());

                canvas.setFont(&fonts::DejaVu12);
                canvas.setTextDatum(textdatum_t::middle_left);
                
                canvas.setTextColor(labelColor);
                canvas.drawString(questions[i].text.c_str(), leftX + 42.0f, cy - 8.0f);

                canvas.setTextColor(subColor);
                canvas.drawString(questions[i].timestamp.c_str(), leftX + 42.0f, cy + 8.0f);

                float chevX = leftX + cardW - 16.0f;
                ScreenCommon::drawIcon(canvas, Icon::ChevronRight, chevX - 5.0f, cy - 5.0f, 10.0f, subColor);
            }

            canvas.clearClipRect();
            canvas.pushSprite(0, 0);

            uint32_t frameMs = millis() - nowMs;
            if (frameMs < 16)
            {
                delay(16 - frameMs);
            }
        }

        return targetScreen;
    }
}
