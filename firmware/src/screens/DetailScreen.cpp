#include "DetailScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/ReminderService.h"
#include "../services/IdeaService.h"
#include "../services/QuestionService.h"
#include "../services/MemoryService.h"
#include "../services/DataService.h"
#include "../services/ApiClient.h"
#include "../services/WiFiManager.h"
#include "../services/TimeService.h"
#include "../audio/AudioManager.h"
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
                    y += canvas.fontHeight() + 3.0f;
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
                    y += canvas.fontHeight() + 3.0f;
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
            y += canvas.fontHeight() + 3.0f;
        }
    }
}

namespace VOXA
{
    extern ReminderService reminderService;
    extern IdeaService ideaService;
    extern QuestionService questionService;
    extern MemoryService memoryService;
    extern TimeService timeService;

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
        uint16_t tagColor = 0xFDC0;
        std::string tagLabel = "NOTE";

        if (s_category == "reminders")
        {
            tagLabel = "REMINDER";
            tagColor = 0x79CF; // Cyan/blue
            auto reminders = reminderService.getAll();
            for (const auto& r : reminders)
            {
                if (r.id == s_itemId)
                {
                    titleStr = r.title;
                    contentStr = r.comments.empty() ? "No description" : r.comments;
                    recordedDateStr = "Due: " + (r.dateTime.empty() ? "N/A" : DataService::formatReadableTimestamp(r.dateTime));
                    isDone = r.completed;
                    statusStr = r.completed ? "COMPLETED" : "PENDING";
                    break;
                }
            }
        }
        else if (s_category == "ideas")
        {
            tagLabel = "IDEA";
            tagColor = 0xFD20; // Warm Amber
            auto ideas = ideaService.getAll();
            for (const auto& idea : ideas)
            {
                if (idea.id == s_itemId)
                {
                    titleStr = idea.title;
                    contentStr = idea.content.empty() ? "No description" : idea.content;
                    recordedDateStr = "Recorded: " + (idea.timestamp.empty() ? "Today" : DataService::formatReadableTimestamp(idea.timestamp));
                    statusStr = "AI INSIGHT";
                    break;
                }
            }
        }
        else if (s_category == "questions")
        {
            tagLabel = "QUESTION";
            tagColor = 0x3DFE; // Bright Cyan
            auto questions = questionService.getAll();
            for (const auto& q : questions)
            {
                if (q.id == s_itemId)
                {
                    titleStr = q.text;
                    contentStr = q.answered ? q.answer : "Awaiting AI answer...";
                    recordedDateStr = "Asked: " + (q.timestamp.empty() ? "Today" : DataService::formatReadableTimestamp(q.timestamp));
                    isDone = q.answered;
                    statusStr = q.answered ? "ANSWERED" : "PROCESSING";
                    break;
                }
            }
        }
        else if (s_category == "tasks")
        {
            tagLabel = "TASK";
            tagColor = 0xFDC0; // Yellow/Amber
            auto tasks = dataService.getTasks();
            for (const auto& t : tasks)
            {
                if (t.id == s_itemId)
                {
                    titleStr = t.title;
                    contentStr = t.content.empty() ? "No description" : t.content;
                    recordedDateStr = "Recorded: " + (t.timestamp.empty() ? "Today" : DataService::formatReadableTimestamp(t.timestamp));
                    isDone = t.isDone;
                    statusStr = t.isDone ? "COMPLETED" : "PENDING";
                    break;
                }
            }
        }
        else if (s_category == "memories" || s_category == "others")
        {
            tagLabel = "MEMORY";
            tagColor = 0xA27A; // Soft Purple
            auto memories = memoryService.getAll();
            for (const auto& mem : memories)
            {
                if (mem.id == s_itemId)
                {
                    titleStr = mem.title;
                    contentStr = mem.content.empty() ? "No description" : mem.content;
                    recordedDateStr = "Recorded: " + (mem.timestamp.empty() ? "Today" : DataService::formatReadableTimestamp(mem.timestamp));
                    statusStr = "ARCHIVED";
                    break;
                }
            }
        }

        bool hasCompleteAction = (s_category == "tasks" || s_category == "reminders");
        float totalContentHeight = 220.0f;

        while (targetScreen == ScreenId::Detail)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            float btnY = h - 44.0f;
            float btnH = 34.0f;
            float btnW = (w - 28.0f) * 0.5f;
            float btn1X = 10.0f;
            float btn2X = 10.0f + btnW + 8.0f;

            if (touched && entryFrame >= 5)
            {
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

                    // Header Back button touch
                    if (tx <= 60 && ty >= 16 && ty <= 46)
                    {
                        m_isBackPressed = true;
                    }

                    // Bottom Complete / Left button touch
                    if (tx >= btn1X && tx <= btn1X + btnW && ty >= btnY - 2.0f && ty <= btnY + btnH + 4.0f)
                    {
                        m_isCompletePressed = true;
                    }

                    // Bottom Delete button touch
                    if (tx >= btn2X && tx <= btn2X + btnW && ty >= btnY - 2.0f && ty <= btnY + btnH + 4.0f)
                    {
                        m_isDeletePressed = true;
                    }
                }
                else
                {
                    float totalDeltaY = ty - m_dragStartY;
                    if (!m_isDragging && std::abs(totalDeltaY) > 8.0f)
                    {
                        m_isDragging = true;
                    }
                    if (m_isDragging)
                    {
                        m_targetScrollY = m_dragStartScrollY - totalDeltaY;
                        uint32_t dt = nowMs - m_lastTouchSampleMs;
                        if (dt > 10)
                        {
                            m_scrollVelocity = -((float)(ty - m_lastDragY) / (float)dt) * 1000.0f;
                            m_lastTouchSampleMs = nowMs;
                        }
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;

                    // Swipe right to go back
                    if (swipeBackCandidate && (tx - dragStartX) > 40.0f && std::abs(ty - dragStartY) < 40.0f)
                    {
                        AudioManager::instance().playTapSoundAsync();
                        targetScreen = s_backRoute;
                    }
                    else if (!m_isDragging)
                    {
                        if (m_isBackPressed)
                        {
                            AudioManager::instance().playTapSoundAsync();
                            targetScreen = s_backRoute;
                        }
                        else if (m_isCompletePressed)
                        {
                            AudioManager::instance().playTapSoundAsync();
                            if (s_category == "tasks")
                            {
                                dataService.toggleTaskDone(s_itemId);
                                isDone = !isDone;
                                statusStr = isDone ? "COMPLETED" : "PENDING";

                                auto tasks = dataService.getTasks();
                                std::string sourceId = "";
                                std::string mongoId = "";
                                for (const auto& t : tasks)
                                {
                                    if (t.id == s_itemId)
                                    {
                                        sourceId = t.sourceId;
                                        mongoId  = t.mongoId;
                                        break;
                                    }
                                }

                                if (wifiManager.isConnected())
                                {
                                    std::string idStr = !sourceId.empty() ? sourceId : (!mongoId.empty() ? mongoId : std::to_string(s_itemId));
                                    std::string ep = "/api/notes/" + idStr + "/toggle?category=tasks";
                                    apiClient.post(ep, "{}");
                                }
                            }
                            else if (s_category == "reminders")
                            {
                                reminderService.markComplete(s_itemId);
                                isDone = true;
                                statusStr = "COMPLETED";

                                if (wifiManager.isConnected())
                                {
                                    std::string ep = "/api/notes/" + std::to_string(s_itemId) + "/toggle?category=reminders";
                                    apiClient.post(ep, "{}");
                                }
                            }
                            else
                            {
                                // For ideas/questions/memories: Left button acts as "Back"
                                targetScreen = s_backRoute;
                            }
                        }
                        else if (m_isDeletePressed)
                        {
                            AudioManager::instance().playTapSoundAsync();
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

            // Scroll Inertia calculations
            if (!m_wasTouched && std::abs(m_scrollVelocity) > 0.0f)
            {
                m_targetScrollY += m_scrollVelocity * deltaSecs;
                m_scrollVelocity *= std::pow(0.85f, deltaSecs * 60.0f);
                if (std::abs(m_scrollVelocity) < 5.0f)
                {
                    m_scrollVelocity = 0.0f;
                }
            }

            float visibleHeight = h - 52.0f - 52.0f;
            float maxScrollY = std::max(0.0f, totalContentHeight - visibleHeight);
            m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));
            m_scrollY += (m_targetScrollY - m_scrollY) * 15.0f * deltaSecs;
            if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
            {
                m_scrollY = m_targetScrollY;
            }

            // ═════════════════════════════════════════════════════════════════
            // RENDER LAYOUT — Pure Pitch Black OLED Matching All Other Pages
            // ═════════════════════════════════════════════════════════════════
            canvas.fillScreen(0x0000);

            // 1. Top Status Bar (Y = 10)
            canvas.setFont(&fonts::Font0);
            canvas.setTextDatum(textdatum_t::top_left);
            canvas.setTextColor(0xFFFF);
            std::string timeStr = timeService.getCurrentTime();
            if (timeStr.empty()) timeStr = "10:42";
            if (timeStr.length() > 5) timeStr = timeStr.substr(0, 5);
            canvas.drawString(timeStr.c_str(), 12, 10);

            // Category status tag on top right
            canvas.setTextDatum(textdatum_t::top_right);
            canvas.setTextColor(tagColor);
            canvas.drawString(tagLabel.c_str(), w - 20, 10);

            // Flash bolt icon
            canvas.fillTriangle(w - 14, 10, w - 18, 16, w - 13, 16, tagColor);
            canvas.fillTriangle(w - 15, 15, w - 10, 15, w - 14, 21, tagColor);

            // 2. Sub-Header (Y = 28..42)
            canvas.setTextDatum(textdatum_t::middle_left);
            canvas.setFont(&fonts::Font0);
            canvas.setTextColor(m_isBackPressed ? tagColor : 0x94A3B8);
            canvas.drawString("< Back", 12, 34);

            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextColor(0xFFFF);
            canvas.drawString("DETAIL VIEW", 56, 33);

            // Category Icon on header right (Y ~ 28..40)
            if (s_category == "tasks")
            {
                // Checkbox icon
                int bx = w - 24, by = 28;
                canvas.drawRoundRect(bx, by, 12, 12, 2, tagColor);
                canvas.drawLine(bx + 3, by + 6, bx + 5, by + 9, tagColor);
                canvas.drawLine(bx + 5, by + 9, bx + 9, by + 3, tagColor);
            }
            else if (s_category == "reminders")
            {
                // Bell icon
                canvas.fillCircle(w - 18, 30, 4, tagColor);
                canvas.fillTriangle(w - 23, 36, w - 13, 36, w - 18, 28, tagColor);
                canvas.fillRect(w - 24, 35, 12, 2, tagColor);
                canvas.fillCircle(w - 18, 38, 2, tagColor);
            }
            else if (s_category == "ideas")
            {
                // Lightbulb / spark icon
                canvas.fillCircle(w - 18, 30, 4, tagColor);
                canvas.fillRect(w - 20, 34, 4, 3, tagColor);
                canvas.drawLine(w - 19, 38, w - 17, 38, tagColor);
            }
            else if (s_category == "questions")
            {
                // Question mark icon
                canvas.drawCircle(w - 18, 30, 4, tagColor);
                canvas.fillRect(w - 19, 34, 2, 2, tagColor);
                canvas.fillCircle(w - 18, 38, 1, tagColor);
            }
            else
            {
                // Memory / bookmark icon
                canvas.drawRect(w - 22, 27, 9, 13, tagColor);
                canvas.drawLine(w - 20, 31, w - 15, 31, tagColor);
                canvas.drawLine(w - 20, 35, w - 16, 35, tagColor);
            }

            // Header separator line
            canvas.drawFastHLine(0, 48, w, canvas.color565(26, 30, 38));

            // 3. Scrollable Detail Container Card (Y = 52..h - 52)
            canvas.setClipRect(0, 50, w, (int)(h - 50 - 50));

            float cardX = 10.0f;
            float cardY = 54.0f - m_scrollY;
            float cardW = w - 20.0f;
            float innerPad = 12.0f;
            float contentW = cardW - innerPad * 2.0f;
            float curY = cardY + 12.0f;

            // Render Card Base Background
            uint16_t cardBg = canvas.color565(18, 20, 26);
            uint16_t cardBorder = canvas.color565(34, 38, 48);

            // Compute total height first so we can draw card background properly
            float tempY = curY + 24.0f; // after tags
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            // Title height estimation
            // Draw title text later, for now we will draw the card frame around full height
            float renderedCardH = std::max(195.0f, totalContentHeight + 10.0f);
            canvas.fillRoundRect((int)cardX, (int)cardY, (int)cardW, (int)renderedCardH, 8, cardBg);
            canvas.drawRoundRect((int)cardX, (int)cardY, (int)cardW, (int)renderedCardH, 8, cardBorder);

            // A. Category Tag Pill
            canvas.setFont(&fonts::Font0);
            float tagTextW = canvas.textWidth(tagLabel.c_str());
            float pillW = tagTextW + 12.0f;
            canvas.fillRoundRect((int)(cardX + innerPad), (int)curY, (int)pillW, 16, 4, tagColor);
            canvas.setTextColor(0x0000);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.drawString(tagLabel.c_str(), cardX + innerPad + pillW * 0.5f, curY + 8.0f);

            // B. Status Pill (Next to category pill)
            if (!statusStr.empty())
            {
                float statTextW = canvas.textWidth(statusStr.c_str());
                float statPillW = statTextW + 12.0f;
                float statX = cardX + innerPad + pillW + 6.0f;

                uint16_t statBg = isDone ? canvas.color565(16, 44, 28) : canvas.color565(36, 32, 20);
                uint16_t statBorder = isDone ? 0x07E0 : 0xFDC0;
                uint16_t statTxt = isDone ? 0x07E0 : 0xFDC0;

                canvas.fillRoundRect((int)statX, (int)curY, (int)statPillW, 16, 4, statBg);
                canvas.drawRoundRect((int)statX, (int)curY, (int)statPillW, 16, 4, statBorder);
                canvas.setTextColor(statTxt);
                canvas.drawString(statusStr.c_str(), statX + statPillW * 0.5f, curY + 8.0f);
            }

            curY += 24.0f;

            // C. Title (Bold White)
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            drawWrappedString(canvas, titleStr, cardX + innerPad, curY, contentW, 0xFFFF);
            curY += 6.0f;

            // D. Subtle Divider Line
            canvas.drawFastHLine((int)(cardX + innerPad), (int)curY, (int)contentW, canvas.color565(30, 34, 44));
            curY += 8.0f;

            // E. Recorded / Due Timestamp (Slate-400)
            if (!recordedDateStr.empty())
            {
                canvas.setFont(&fonts::Font0);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(0x94A3B8);
                // Draw small dot icon
                canvas.fillCircle((int)(cardX + innerPad + 3), (int)(curY + 4), 2, tagColor);
                canvas.drawString(recordedDateStr.c_str(), cardX + innerPad + 10, curY);
                curY += 16.0f;
            }

            // F. Body Content String (Light Slate-200)
            if (!contentStr.empty())
            {
                canvas.setFont(&fonts::FreeSans9pt7b);
                drawWrappedString(canvas, contentStr, cardX + innerPad, curY, contentW, canvas.color565(226, 232, 240));
                curY += 8.0f;
            }

            totalContentHeight = (curY + m_scrollY) - 54.0f + 16.0f;
            canvas.clearClipRect();

            // 4. Bottom Action Buttons Bar (Y = h - 48..h)
            // Backdrop to cover scrolling content cleanly
            canvas.fillRect(0, h - 50, w, 50, 0x0000);
            canvas.drawFastHLine(0, h - 50, w, canvas.color565(26, 30, 38));

            // Left Button: Complete / Back
            uint16_t btn1Bg, btn1Border, btn1Text;
            const char* btn1Label;

            if (hasCompleteAction)
            {
                if (isDone)
                {
                    btn1Bg = m_isCompletePressed ? 0x05E0 : canvas.color565(16, 44, 28);
                    btn1Border = 0x07E0;
                    btn1Text = m_isCompletePressed ? 0x0000 : 0x07E0;
                    btn1Label = "[x] Completed";
                }
                else
                {
                    btn1Bg = m_isCompletePressed ? canvas.color565(36, 42, 54) : canvas.color565(24, 28, 36);
                    btn1Border = m_isCompletePressed ? 0x07E0 : canvas.color565(48, 54, 68);
                    btn1Text = 0xFFFF;
                    btn1Label = "[ ] Mark Done";
                }
            }
            else
            {
                btn1Bg = m_isCompletePressed ? canvas.color565(36, 42, 54) : canvas.color565(24, 28, 36);
                btn1Border = m_isCompletePressed ? tagColor : canvas.color565(48, 54, 68);
                btn1Text = 0xFFFF;
                btn1Label = "< Back";
            }

            canvas.fillRoundRect((int)btn1X, (int)btnY, (int)btnW, (int)btnH, 6, btn1Bg);
            canvas.drawRoundRect((int)btn1X, (int)btnY, (int)btnW, (int)btnH, 6, btn1Border);
            canvas.setFont(&fonts::Font0);
            canvas.setTextColor(btn1Text);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.drawString(btn1Label, btn1X + btnW * 0.5f, btnY + btnH * 0.5f);

            // Right Button: Delete
            uint16_t btn2Bg = m_isDeletePressed ? 0xFA20 : canvas.color565(44, 16, 20);
            uint16_t btn2Border = m_isDeletePressed ? 0xFFFF : canvas.color565(100, 24, 30);
            uint16_t btn2Text = m_isDeletePressed ? 0x0000 : 0xF87171;

            canvas.fillRoundRect((int)btn2X, (int)btnY, (int)btnW, (int)btnH, 6, btn2Bg);
            canvas.drawRoundRect((int)btn2X, (int)btnY, (int)btnW, (int)btnH, 6, btn2Border);
            canvas.setTextColor(btn2Text);
            canvas.drawString("Delete", btn2X + btnW * 0.5f, btnY + btnH * 0.5f);

            // Transition: Slide In
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
