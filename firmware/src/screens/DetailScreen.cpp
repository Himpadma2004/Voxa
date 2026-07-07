#include "DetailScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/ReminderService.h"
#include "../services/IdeaService.h"
#include "../services/QuestionService.h"
#include "Transition.h"
#include <cmath>

namespace VOXA
{
    extern ReminderService reminderService;
    extern IdeaService ideaService;
    extern QuestionService questionService;

    std::string DetailScreen::s_category = "";
    uint32_t    DetailScreen::s_itemId = 0;
    ScreenId    DetailScreen::s_backRoute = ScreenId::Home;

    void DetailScreen::setItem(const std::string& category, uint32_t id, ScreenId backRoute)
    {
        s_category = category;
        s_itemId = id;
        s_backRoute = backRoute;
    }

    ScreenId DetailScreen::show(Touch& touch)
    {
        int entryFrame = 0;
        float dragStartX = 0.0f;
        float dragStartY = 0.0f;
        bool swipeBackCandidate = false;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            return s_backRoute;
        }

        ScreenId targetScreen = ScreenId::Detail;
        uint32_t lastMs = millis();

        // 1. Retrieve Item Data
        std::string titleStr = "Loading...";
        std::string contentStr = "";
        std::string statusStr = "";
        uint16_t tagColor = 0x79CF;

        if (s_category == "reminders")
        {
            auto reminders = reminderService.getAll();
            for (const auto& r : reminders)
            {
                if (r.id == s_itemId)
                {
                    titleStr = r.title;
                    contentStr = "Due: " + r.dateTime;
                    statusStr = r.completed ? "Status: Completed" : "Status: Pending";
                    tagColor = 0x79CF;
                    break;
                }
            }
        }
        else if (s_category == "ideas")
        {
            auto ideas = ideaService.getAll();
            for (const auto& idea : ideas)
            {
                if (idea.id == s_itemId)
                {
                    titleStr = idea.title;
                    contentStr = idea.content.empty() ? "No description" : idea.content;
                    statusStr = "Timestamp: " + idea.timestamp;
                    tagColor = 0xFD20;
                    break;
                }
            }
        }
        else if (s_category == "questions")
        {
            auto questions = questionService.getAll();
            for (const auto& q : questions)
            {
                if (q.id == s_itemId)
                {
                    titleStr = q.text;
                    contentStr = q.answered ? q.answer : "Awaiting AI answer...";
                    statusStr = q.answered ? "Status: Answered" : "Status: Pending";
                    tagColor = 0x067F;
                    break;
                }
            }
        }

        while (targetScreen == ScreenId::Detail)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            // 2. Touch Input
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
                    // Back button bounds
                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isBackPressed = true;
                    }

                    // Delete button bounds (centered at bottom Y = h - 35, width = 120, height = 26)
                    float delCx = w * 0.5f;
                    float delCy = h - 35.0f;
                    if (tx >= delCx - 60.0f && tx <= delCx + 60.0f &&
                        ty >= delCy - 13.0f && ty <= delCy + 13.0f)
                    {
                        m_isDeletePressed = true;
                    }
                }
                else
                {
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        targetScreen = s_backRoute;
                        swipeBackCandidate = false;
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

                    if (m_isBackPressed)
                    {
                        targetScreen = s_backRoute;
                    }
                    else if (m_isDeletePressed)
                    {
                        // Perform deletion on target service
                        if (s_category == "reminders")
                        {
                            reminderService.remove(s_itemId);
                        }
                        else if (s_category == "ideas")
                        {
                            ideaService.remove(s_itemId);
                        }
                        else if (s_category == "questions")
                        {
                            questionService.remove(s_itemId);
                        }
                        Serial.print("[Detail] Deleted item: ");
                        Serial.println(s_itemId);
                        targetScreen = s_backRoute;
                    }
                    m_isBackPressed = false;
                    m_isDeletePressed = false;
                }
            }

            // Dimensions re-query
            w = Display::width();
            h = Display::height();

            // 3. Render Detail Page
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Detail View", true, false, Icon::Plus, w, h);

            // Back button Y centering (Y = 45.0f)
            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, 
                                              backFill, backColor, w, h);

            // Category tag pill
            std::string tagText = s_category;
            std::transform(tagText.begin(), tagText.end(), tagText.begin(), ::toupper);
            if (!tagText.empty() && tagText.back() == 'S') tagText.pop_back(); // singularize

            float tagX = w * 0.04f;
            float tagY = 74.0f;
            canvas.setFont(&fonts::DejaVu12);
            float tw = canvas.textWidth(tagText.c_str());
            canvas.fillRoundRect((int)tagX, (int)tagY, (int)(tw + 12.0f), 20, 4, tagColor);
            canvas.setTextColor(VoxaTheme::getBackground());
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.drawString(tagText.c_str(), tagX + tw * 0.5f + 6.0f, tagY + 10.0f);

            // Title block
            canvas.setFont(&fonts::DejaVu18);
            canvas.setTextColor(VoxaTheme::getTextPrimary());
            canvas.setTextDatum(textdatum_t::top_left);
            canvas.drawString(titleStr.c_str(), w * 0.04f, 102.0f);

            // Content block
            canvas.setFont(&fonts::DejaVu12);
            canvas.setTextColor(VoxaTheme::getTextSecondary());
            canvas.drawString(contentStr.c_str(), w * 0.04f, 134.0f);

            // Status block
            canvas.setTextColor(VoxaTheme::getPrimary());
            canvas.drawString(statusStr.c_str(), w * 0.04f, 156.0f);

            // Delete button at bottom
            float delCx = w * 0.5f;
            float delCy = h - 35.0f;
            uint16_t delBg = m_isDeletePressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t delBorder = m_isDeletePressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getDivider();
            uint16_t delText = m_isDeletePressed ? VoxaTheme::getBackground() : VoxaTheme::getWarning();

            canvas.fillRoundRect((int)(delCx - 60.0f), (int)(delCy - 13.0f), 120, 26, 6, delBg);
            canvas.drawRoundRect((int)(delCx - 60.0f), (int)(delCy - 13.0f), 120, 26, 6, delBorder);
            
            canvas.setFont(&fonts::DejaVu12);
            canvas.setTextColor(delText);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.drawString("Delete", delCx, delCy);

            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Detail), entryFrame, 10);
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

        return targetScreen;
    }
}
