#include "ReminderScreen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

#include "../core/Application.h"
#include "../core/services/ReminderService.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Button.h"
#include "../widgets/Card.h"
#include "../widgets/ListTile.h"
#include "ScreenCommon.h"

namespace
{
    struct ReminderStyle
    {
        SDL_Color iconColor;
        SDL_Color dotColor;
    };

    const std::array<ReminderStyle, 4> styles { {
        { SDL_Color { 145, 105, 255, 255 }, SDL_Color { 145, 105, 255, 255 } },
        { SDL_Color { 255, 95, 150, 255 }, SDL_Color { 255, 95, 150, 255 } },
        { SDL_Color { 255, 180, 40, 255 }, SDL_Color { 255, 180, 40, 255 } },
        { SDL_Color { 80, 185, 255, 255 }, SDL_Color { 80, 185, 255, 255 } }
    } };

    std::string formatDateTime(const std::string& raw)
    {
        int y = 0, m = 0, d = 0, hr = 0, min = 0;
        if (std::sscanf(raw.c_str(), "%d-%d-%d %d:%d", &y, &m, &d, &hr, &min) == 5)
        {
            std::tm tm {};
            tm.tm_year = y - 1900;
            tm.tm_mon = m - 1;
            tm.tm_mday = d;
            tm.tm_hour = hr;
            tm.tm_min = min;
            char buf[64];
            std::strftime(buf, sizeof(buf), "%I:%M %p", &tm);
            std::string s(buf);
            if (!s.empty() && s[0] == '0') s = s.substr(1);
            return s;
        }
        else if (std::sscanf(raw.c_str(), "%d-%d-%d", &y, &m, &d) == 3)
        {
            std::tm tm {};
            tm.tm_year = y - 1900;
            tm.tm_mon = m - 1;
            tm.tm_mday = d;
            char buf[64];
            std::strftime(buf, sizeof(buf), "%b %d", &tm);
            return std::string(buf);
        }
        return raw;
    }
}

namespace VOXA
{
    ScreenId ReminderScreen::id() const
    {
        return ScreenId::Reminders;
    }

    void ReminderScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
            // Header back button hit area centered at (18, 28) with radius 11
            if (Rect { 0.0f, 0.0f, 40.0f, 40.0f }.contains(point.x, point.y))
            {
                app.navigateBack();
            }
            else if (Rect { 10.0f, 68.0f, 300.0f, 162.0f }.contains(point.x, point.y))
            {
                m_isDragging = true;
                m_dragStartY = point.y;
                m_dragStartScrollY = m_targetScrollY;
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            if (m_isDragging)
            {
                const SDL_FPoint point = app.windowToCanvas(event.motion.x, event.motion.y);
                float diffY = point.y - m_dragStartY;
                m_targetScrollY = m_dragStartScrollY - diffY;
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            if (m_isDragging)
            {
                const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
                float diffY = std::abs(point.y - m_dragStartY);
                if (diffY < 6.0f)
                {
                    auto reminders = app.services().reminders->getAll();
                    for (std::size_t i = 0; i < reminders.size(); ++i)
                    {
                        Rect tileRect { 10.0f, 68.0f + i * 54.0f - m_scrollY, 300.0f, 48.0f };
                        if (tileRect.contains(point.x, point.y) && point.y >= 68.0f && point.y <= 230.0f)
                        {
                            app.setSelectedItem("reminders", reminders[i].id);
                            app.navigateTo(ScreenId::Detail);
                            break;
                        }
                    }
                }
                m_isDragging = false;
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            float mx = 0.0f, my = 0.0f;
            SDL_GetMouseState(&mx, &my);
            const SDL_FPoint mPt = app.windowToCanvas(mx, my);
            if (Rect { 10.0f, 68.0f, 300.0f, 162.0f }.contains(mPt.x, mPt.y))
            {
                m_targetScrollY -= event.wheel.y * 20.0f;
            }
        }
    }

    void ReminderScreen::update(Application& app, float deltaSeconds)
    {
        std::size_t numReminders = 0;
        if (app.services().reminders)
        {
            numReminders = app.services().reminders->getAll().size();
        }

        float contentHeight = std::max(0.0f, static_cast<float>(numReminders) * 54.0f);
        float visibleHeight = 162.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        m_targetScrollY = std::clamp(m_targetScrollY, 0.0f, maxScrollY);
        m_scrollY += (m_targetScrollY - m_scrollY) * 12.0f * deltaSeconds;
        if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
        {
            m_scrollY = m_targetScrollY;
        }
    }

    void ReminderScreen::render(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Reminders", true, true, Icon::Plus);

        // Header sub-category label
        renderer.drawText("Today", 12.0f, 50.0f, SDL_Color { 145, 105, 255, 255 }, 11);

        // Retrieve reminders from the backend service
        std::vector<Reminder> reminders;
        if (app.services().reminders)
        {
            reminders = app.services().reminders->getAll();
        }

        // Set clipping region to prevent scroll overlap with status bar or footer
        renderer.setClipRect(5.0f, 68.0f, 310.0f, 162.0f);

        for (std::size_t i = 0; i < reminders.size(); ++i)
        {
            const auto& r = reminders[i];
            const ReminderStyle& style = styles[i % styles.size()];
            Icon icon = (r.dateTime.find(':') != std::string::npos) ? Icon::Bell : Icon::Calendar;
            std::string formattedDate = formatDateTime(r.dateTime);

            ListTile tile(Rect { 10.0f, 68.0f + i * 54.0f - m_scrollY, 300.0f, 48.0f }, 
                          icon, 
                          r.title.c_str(), 
                          formattedDate.c_str(), 
                          style.iconColor, 
                          SDL_Color { 0, 0, 0, 0 }, 
                          false);
            tile.render(renderer);
        }

        renderer.clearClipRect();
    }
}
