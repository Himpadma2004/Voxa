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
        { SDL_Color { 130, 92, 255, 255 }, SDL_Color { 130, 92, 255, 255 } },
        { SDL_Color { 255, 92, 146, 255 }, SDL_Color { 255, 92, 146, 255 } },
        { SDL_Color { 255, 168, 32, 255 }, SDL_Color { 255, 168, 32, 255 } },
        { SDL_Color { 200, 92, 255, 255 }, SDL_Color { 150, 104, 255, 255 } }
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
            std::strftime(buf, sizeof(buf), "%b %d, %Y, %I:%M %p", &tm);
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
            std::strftime(buf, sizeof(buf), "%b %d, %Y", &tm);
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
            if (Rect { 44.0f, 34.0f, 56.0f, 56.0f }.contains(point.x, point.y))
            {
                app.navigateTo(ScreenId::Home);
            }
            else if (Rect { 300.0f, 228.0f, 1000.0f, 532.0f }.contains(point.x, point.y))
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
            m_isDragging = false;
        }
        else if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            float mx = 0.0f, my = 0.0f;
            SDL_GetMouseState(&mx, &my);
            const SDL_FPoint mPt = app.windowToCanvas(mx, my);
            if (Rect { 300.0f, 140.0f, 1000.0f, 640.0f }.contains(mPt.x, mPt.y))
            {
                m_targetScrollY -= event.wheel.y * 38.0f;
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

        float contentHeight = std::max(0.0f, static_cast<float>(numReminders) * 114.0f - 30.0f);
        float visibleHeight = 522.0f;
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

        // Center glass card container
        Card container(Rect { 300.0f, 140.0f, 1000.0f, 640.0f }, Colors::Card, 32.0f);
        container.setShadow(Colors::Shadow, 8);
        container.setBorder(Colors::GlassBorder);
        container.render(renderer);

        // Category selection buttons inside the card container
        Button(Rect { 340.0f, 170.0f, 120.0f, 40.0f }, "All", true).render(renderer);
        Button(Rect { 480.0f, 170.0f, 130.0f, 40.0f }, "Today", false).render(renderer);
        Button(Rect { 630.0f, 170.0f, 150.0f, 40.0f }, "Upcoming", false).render(renderer);

        // Retrieve reminders from the backend service
        std::vector<Reminder> reminders;
        if (app.services().reminders)
        {
            reminders = app.services().reminders->getAll();
        }

        // Set clipping region to prevent scroll overlap with container borders
        renderer.setClipRect(300.0f, 228.0f, 1000.0f, 532.0f);

        for (std::size_t i = 0; i < reminders.size(); ++i)
        {
            const auto& r = reminders[i];
            const ReminderStyle& style = styles[i % styles.size()];
            Icon icon = (r.dateTime.find(':') != std::string::npos) ? Icon::Bell : Icon::Calendar;
            std::string formattedDate = formatDateTime(r.dateTime);

            ListTile tile(Rect { 340.0f, 238.0f + i * 114.0f - m_scrollY, 920.0f, 84.0f }, 
                          icon, 
                          r.title.c_str(), 
                          formattedDate.c_str(), 
                          style.iconColor, 
                          style.dotColor, 
                          false);
            tile.render(renderer);
        }

        renderer.clearClipRect();
    }
}
