#include "OthersScreen.h"

#include <vector>
#include <cmath>
#include <algorithm>

#include "../core/Application.h"
#include "../core/services/StorageService.h"
#include "../core/models/Memory.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Card.h"
#include "../widgets/ListTile.h"
#include "ScreenCommon.h"

namespace VOXA
{
    ScreenId OthersScreen::id() const
    {
        return ScreenId::Others;
    }

    void OthersScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
            if (Rect { 44.0f, 34.0f, 56.0f, 56.0f }.contains(point.x, point.y))
            {
                app.navigateTo(ScreenId::Home);
            }
            else if (Rect { 300.0f, 180.0f, 1000.0f, 580.0f }.contains(point.x, point.y))
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

    void OthersScreen::update(Application& app, float deltaSeconds)
    {
        std::size_t numMemories = 0;
        if (app.services().storage)
        {
            numMemories = app.services().storage->loadAllMemories().size();
        }

        float contentHeight = std::max(0.0f, static_cast<float>(numMemories) * 114.0f - 30.0f);
        float visibleHeight = 560.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        m_targetScrollY = std::clamp(m_targetScrollY, 0.0f, maxScrollY);
        m_scrollY += (m_targetScrollY - m_scrollY) * 12.0f * deltaSeconds;
        if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
        {
            m_scrollY = m_targetScrollY;
        }
    }

    void OthersScreen::render(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Others", true, true, Icon::Plus);

        // Center glass card container
        Card container(Rect { 300.0f, 140.0f, 1000.0f, 640.0f }, Colors::Card, 32.0f);
        container.setShadow(Colors::Shadow, 8);
        container.setBorder(Colors::GlassBorder);
        container.render(renderer);

        // Load memories from storage
        std::vector<Memory> memories;
        if (app.services().storage)
        {
            memories = app.services().storage->loadAllMemories();
        }

        // Set clipping region to prevent scroll overlap with container borders
        renderer.setClipRect(300.0f, 180.0f, 1000.0f, 580.0f);

        for (std::size_t i = 0; i < memories.size(); ++i)
        {
            const auto& m = memories[i];
            
            // Map category to icon and color
            Icon icon = Icon::Note;
            SDL_Color color = SDL_Color { 222, 92, 255, 255 }; // default purple
            if (m.category == "voice")
            {
                icon = Icon::Mic;
                color = SDL_Color { 68, 192, 122, 255 }; // green for voice note
            }
            else if (m.title.find("Thoughts") != std::string::npos || m.title.find("Thoughts") != std::string::npos)
            {
                icon = Icon::Chat;
                color = SDL_Color { 56, 168, 255, 255 }; // blue
            }
            else if (m.title.find("Daily") != std::string::npos)
            {
                icon = Icon::Note;
                color = SDL_Color { 62, 152, 255, 255 }; // light blue
            }

            ListTile tile(Rect { 380.0f, 200.0f + i * 114.0f - m_scrollY, 840.0f, 84.0f },
                          icon, 
                          m.title.c_str(), 
                          m.timestamp.c_str(), 
                          color,
                          SDL_Color { 0, 0, 0, 0 }, 
                          false);
            tile.render(renderer);
        }

        renderer.clearClipRect();

        ScreenCommon::renderPageDots(renderer, 0);
    }
}
