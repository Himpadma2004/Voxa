#include "DetailScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/ReminderService.h"
#include "../services/IdeaService.h"
#include "../services/QuestionService.h"
#include "../services/MemoryService.h"
#include "../services/DataService.h"
#include "Transition.h"
#include <cmath>
#include <algorithm>

namespace
{
    void drawWrappedString(LGFX_Sprite& canvas, const std::string& text, float x, float& y, float maxW, uint16_t color)
    {
        canvas.setTextColor(color);
        canvas.setTextDatum(textdatum_t::top_left);

        std::string currentLine = "";
        std::string word = "";
        
        for (size_t i = 0; i <= text.size(); i++)
        {
            char c = (i < text.size()) ? text[i] : '\0';
            if (c == ' ' || c == '\n' || c == '\0')
            {
                std::string testLine = currentLine.empty() ? word : (currentLine + " " + word);
                if (canvas.textWidth(testLine.c_str()) > maxW && !currentLine.empty())
                {
                    canvas.drawString(currentLine.c_str(), x, y);
                    y += canvas.fontHeight() + 2.0f;
                    currentLine = word;
                }
                else
                {
                    currentLine = testLine;
                }
                word = "";
                if (c == '\n')
                {
                    canvas.drawString(currentLine.c_str(), x, y);
                    y += canvas.fontHeight() + 2.0f;
                    currentLine = "";
                }
            }
            else
            {
                word += c;
            }
        }

        if (!currentLine.empty())
        {
            canvas.drawString(currentLine.c_str(), x, y);
            y += canvas.fontHeight() + 2.0f;
        }
    }
}

namespace VOXA
{
    extern ReminderService reminderService;
    extern IdeaService ideaService;
    extern QuestionService questionService;
    extern MemoryService memoryService;

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

        // Scroll state variables
        float m_scrollY = 0.0f;
        float m_targetScrollY = 0.0f;
        float m_scrollVelocity = 0.0f;
        float m_lastDragX = 0.0f;
        float m_lastDragY = 0.0f;
        float m_dragStartY = 0.0f;
        float m_dragStartScrollY = 0.0f;
        bool  m_wasTouched = false;
        bool  m_isDragging = false;
        uint32_t m_lastTouchSampleMs = 0;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
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
        else if (s_category == "tasks")
        {
            auto tasks = dataService.getTasks();
            for (const auto& t : tasks)
            {
                if (t.id == s_itemId)
                {
                    titleStr = t.title;
                    contentStr = t.content.empty() ? "No description" : t.content;
                    statusStr = t.isDone ? "Status: Completed" : "Status: Pending";
                    tagColor = 0xFA20;
                    break;
                }
            }
        }
        else if (s_category == "memories")
        {
            auto memories = memoryService.getAll();
            for (const auto& mem : memories)
            {
                if (mem.id == s_itemId)
                {
                    titleStr = mem.title;
                    contentStr = mem.content.empty() ? "No description" : mem.content;
                    statusStr = "Timestamp: " + mem.timestamp;
                    tagColor = 0xA27A;
                    break;
                }
            }
        }

        float totalContentHeight = h; // Default to screen height

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
                    m_dragStartY = ty;
                    m_dragStartScrollY = m_targetScrollY;
                    m_lastTouchSampleMs = nowMs;
                    m_isDragging = false;
                    m_scrollVelocity = 0.0f;

                    // Back button bounds
                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isBackPressed = true;
                    }

                    // Delete button bounds
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
                    float dy = ty - dragStartY;
                    
                    if (swipeBackCandidate && dx > 60 && std::abs(dy) < 40)
                    {
                        targetScreen = s_backRoute;
                        swipeBackCandidate = false;
                    }

                    if (!m_isDragging && std::abs(dy) > 8.0f && !m_isBackPressed && !m_isDeletePressed)
                    {
                        m_isDragging = true;
                        dragStartY = ty;
                        m_dragStartScrollY = m_targetScrollY;
                    }

                    if (m_isDragging)
                    {
                        m_targetScrollY = m_dragStartScrollY - dy;
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
                            targetScreen = s_backRoute;
                        }
                        else if (m_isDeletePressed)
                        {
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
                            else if (s_category == "tasks")
                            {
                                dataService.removeTaskLocal(s_itemId);
                            }
                            else if (s_category == "memories")
                            {
                                memoryService.remove(s_itemId);
                            }
                            Serial.print("[Detail] Deleted item: ");
                            Serial.println(s_itemId);
                            targetScreen = s_backRoute;
                        }
                    }
                    m_isBackPressed = false;
                    m_isDeletePressed = false;
                }
            }

            // Dimensions re-query
            w = Display::width();
            h = Display::height();

            // Perform Scroll Inertia
            if (!m_wasTouched && std::abs(m_scrollVelocity) > 0.0f)
            {
                m_targetScrollY += m_scrollVelocity * deltaSecs;
                m_scrollVelocity *= std::pow(0.85f, deltaSecs * 60.0f);
                if (std::abs(m_scrollVelocity) < 5.0f)
                {
                    m_scrollVelocity = 0.0f;
                }
            }

            float visibleHeight = h - 70.0f - 55.0f;
            float maxScrollY = std::max(0.0f, totalContentHeight - visibleHeight);
            m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));
            m_scrollY += (m_targetScrollY - m_scrollY) * 15.0f * deltaSecs;
            if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
            {
                m_scrollY = m_targetScrollY;
            }

            // 3. Render Detail Page
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Detail View", true, false, Icon::Plus, w, h);

            // Back button
            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, 
                                              backFill, backColor, w, h);

            // Scrollable Content area (70 to h - 55)
            canvas.setClipRect(0, 70, w, h - 70 - 55);

            float currentY = 76.0f - m_scrollY;
            float maxW = w * 0.92f;
            float startX = w * 0.04f;

            // Category tag pill
            std::string tagText = s_category;
            std::transform(tagText.begin(), tagText.end(), tagText.begin(), ::toupper);
            if (!tagText.empty() && tagText.back() == 'S') tagText.pop_back(); // singularize

            canvas.setFont(&fonts::FreeSans9pt7b);
            float tw = canvas.textWidth(tagText.c_str());
            canvas.fillRoundRect((int)startX, (int)currentY, (int)(tw + 12.0f), 20, 4, tagColor);
            canvas.setTextColor(VoxaTheme::getBackground());
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.drawString(tagText.c_str(), startX + tw * 0.5f + 6.0f, currentY + 10.0f);
            
            currentY += 28.0f; // Gap after tag

            // Title block
            canvas.setFont(&fonts::FreeSansBold12pt7b);
            drawWrappedString(canvas, titleStr, startX, currentY, maxW, VoxaTheme::getTextPrimary());
            currentY += 10.0f; // Gap after title

            // Content block
            canvas.setFont(&fonts::FreeSans9pt7b);
            drawWrappedString(canvas, contentStr, startX, currentY, maxW, VoxaTheme::getTextSecondary());
            currentY += 10.0f; // Gap after content

            // Status block
            canvas.setFont(&fonts::FreeSans9pt7b);
            drawWrappedString(canvas, statusStr, startX, currentY, maxW, VoxaTheme::getPrimary());
            
            // Calculate total content height (relative to start of scrollable area)
            totalContentHeight = (currentY + m_scrollY) - 70.0f + 10.0f;

            canvas.clearClipRect();

            // Delete button at bottom
            float delCx = w * 0.5f;
            float delCy = h - 35.0f;
            uint16_t delBg = m_isDeletePressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t delBorder = m_isDeletePressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getDivider();
            uint16_t delText = m_isDeletePressed ? VoxaTheme::getBackground() : VoxaTheme::getWarning();

            canvas.fillRoundRect((int)(delCx - 60.0f), (int)(delCy - 13.0f), 120, 26, 6, delBg);
            canvas.drawRoundRect((int)(delCx - 60.0f), (int)(delCy - 13.0f), 120, 26, 6, delBorder);
            
            canvas.setFont(&fonts::FreeSans9pt7b);
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

        canvas.deleteSprite();
        return targetScreen;
    }
}
