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
        std::string recordedDateStr = "";
        std::string statusStr = "";
        bool isDone = false;
        uint16_t tagColor = 0x79CF;

        if (s_category == "reminders")
        {
            auto reminders = reminderService.getAll();
            for (const auto& r : reminders)
            {
                if (r.id == s_itemId)
                {
                    titleStr = r.title;
                    contentStr = r.comments.empty() ? "No description" : r.comments;
                    recordedDateStr = "Due: " + (r.dateTime.empty() ? "N/A" : DataService::formatReadableTimestamp(r.dateTime));
                    isDone = r.completed;
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
                    recordedDateStr = "Recorded: " + (idea.timestamp.empty() ? "N/A" : DataService::formatReadableTimestamp(idea.timestamp));
                    statusStr = "Type: Idea";
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
                    recordedDateStr = "Asked: " + (q.timestamp.empty() ? "N/A" : DataService::formatReadableTimestamp(q.timestamp));
                    isDone = q.answered;
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
                    recordedDateStr = "Recorded: " + (t.timestamp.empty() ? "N/A" : DataService::formatReadableTimestamp(t.timestamp));
                    isDone = t.isDone;
                    statusStr = t.isDone ? "Status: Completed" : "Status: Pending";
                    tagColor = 0xFA20;
                    break;
                }
            }
        }
        else if (s_category == "memories" || s_category == "others")
        {
            auto memories = memoryService.getAll();
            for (const auto& mem : memories)
            {
                if (mem.id == s_itemId)
                {
                    titleStr = mem.title;
                    contentStr = mem.content.empty() ? "No description" : mem.content;
                    recordedDateStr = "Recorded: " + (mem.timestamp.empty() ? "N/A" : DataService::formatReadableTimestamp(mem.timestamp));
                    statusStr = "Category: Other";
                    tagColor = 0xA27A;
                    break;
                }
            }
        }

        float totalContentHeight = h;

        while (targetScreen == ScreenId::Detail)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            float compCx = w * 0.28f;
            float delCx = w * 0.72f;
            float btnCy = h - 35.0f;

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

                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isBackPressed = true;
                    }

                    if (tx >= compCx - 52.0f && tx <= compCx + 52.0f &&
                        ty >= btnCy - 14.0f && ty <= btnCy + 14.0f)
                    {
                        m_isCompletePressed = true;
                    }

                    if (tx >= delCx - 52.0f && tx <= delCx + 52.0f &&
                        ty >= btnCy - 14.0f && ty <= btnCy + 14.0f)
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

                    if (!m_isDragging && std::abs(dy) > 8.0f && !m_isBackPressed && !m_isDeletePressed && !m_isCompletePressed)
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
                        else if (m_isCompletePressed)
                        {
                            if (s_category == "tasks")
                            {
                                dataService.toggleTaskDone(s_itemId);
                                for (const auto& t : dataService.getTasks())
                                {
                                    if (t.id == s_itemId)
                                    {
                                        isDone = t.isDone;
                                        statusStr = t.isDone ? "Status: Completed" : "Status: Pending";
                                        break;
                                    }
                                }
                            }
                            else if (s_category == "reminders")
                            {
                                reminderService.markComplete(s_itemId);
                                isDone = true;
                                statusStr = "Status: Completed";
                            }
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
                            else if (s_category == "memories" || s_category == "others")
                            {
                                memoryService.remove(s_itemId);
                            }
                            targetScreen = s_backRoute;
                        }
                    }
                    m_isBackPressed = false;
                    m_isCompletePressed = false;
                    m_isDeletePressed = false;
                }
            }

            w = Display::width();
            h = Display::height();

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

            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Detail View", true, false, Icon::Plus, w, h);

            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, 
                                              backFill, backColor, w, h);

            canvas.setClipRect(0, 70, w, h - 70 - 55);

            float currentY = 76.0f - m_scrollY;
            float maxW = w * 0.92f;
            float startX = w * 0.04f;

            std::string tagText = s_category;
            std::transform(tagText.begin(), tagText.end(), tagText.begin(), ::toupper);
            if (!tagText.empty() && tagText.back() == 'S') tagText.pop_back();

            canvas.setFont(&fonts::FreeSans9pt7b);
            float tw = canvas.textWidth(tagText.c_str());
            canvas.fillRoundRect((int)startX, (int)currentY, (int)(tw + 12.0f), 20, 4, tagColor);
            canvas.setTextColor(VoxaTheme::getBackground());
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.drawString(tagText.c_str(), startX + tw * 0.5f + 6.0f, currentY + 10.0f);
            
            currentY += 28.0f;

            canvas.setFont(&fonts::FreeSansBold12pt7b);
            drawWrappedString(canvas, titleStr, startX, currentY, maxW, VoxaTheme::getTextPrimary());
            currentY += 8.0f;

            if (!recordedDateStr.empty())
            {
                canvas.setFont(&fonts::FreeSans9pt7b);
                drawWrappedString(canvas, recordedDateStr, startX, currentY, maxW, VoxaTheme::getPrimaryLight());
                currentY += 8.0f;
            }

            canvas.setFont(&fonts::FreeSans9pt7b);
            drawWrappedString(canvas, contentStr, startX, currentY, maxW, VoxaTheme::getTextSecondary());
            currentY += 8.0f;

            uint16_t statusColor = isDone ? 0x07E0 : VoxaTheme::getPrimary();
            drawWrappedString(canvas, statusStr, startX, currentY, maxW, statusColor);
            
            totalContentHeight = (currentY + m_scrollY) - 70.0f + 10.0f;
            canvas.clearClipRect();

            uint16_t compBg = m_isCompletePressed ? 0x05E0 : (isDone ? 0x03E0 : VoxaTheme::getSurface());
            uint16_t compBorder = isDone ? 0x07E0 : VoxaTheme::getDivider();
            uint16_t compText = isDone ? VoxaTheme::getBackground() : 0x07E0;

            canvas.fillRoundRect((int)(compCx - 52.0f), (int)(btnCy - 13.0f), 104, 26, 6, compBg);
            canvas.drawRoundRect((int)(compCx - 52.0f), (int)(btnCy - 13.0f), 104, 26, 6, compBorder);
            canvas.setFont(&fonts::FreeSans9pt7b);
            canvas.setTextColor(compText);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.drawString(isDone ? "Completed" : "Complete", compCx, btnCy);

            uint16_t delBg = m_isDeletePressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t delBorder = m_isDeletePressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getDivider();
            uint16_t delText = m_isDeletePressed ? VoxaTheme::getBackground() : VoxaTheme::getWarning();

            canvas.fillRoundRect((int)(delCx - 52.0f), (int)(btnCy - 13.0f), 104, 26, 6, delBg);
            canvas.drawRoundRect((int)(delCx - 52.0f), (int)(btnCy - 13.0f), 104, 26, 6, delBorder);
            canvas.setTextColor(delText);
            canvas.drawString("Delete", delCx, btnCy);

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
