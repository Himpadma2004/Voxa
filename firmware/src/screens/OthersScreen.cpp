#include "OthersScreen.h"

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

#include "../core/Application.h"
#include "../core/services/StorageService.h"
#include "../core/services/MemoryService.h"
#include "../core/models/Memory.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Card.h"
#include "../widgets/ListTile.h"
#include "ScreenCommon.h"

namespace
{
    const std::array<SDL_Color, 4> colors { {
        SDL_Color { 255, 105, 150, 255 },  // Pink
        SDL_Color { 145, 105, 255, 255 },  // Purple
        SDL_Color { 80, 185, 255, 255 },  // Blue
        SDL_Color { 255, 180, 40, 255 }   // Yellow
    } };
}

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
            // Header back button hit area centered at (18, 28) with radius 11
            if (Rect { 0.0f, 0.0f, 40.0f, 40.0f }.contains(point.x, point.y))
            {
                app.navigateBack();
            }
            else if (Rect { 10.0f, 48.0f, 300.0f, 182.0f }.contains(point.x, point.y))
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
                    auto memories = app.services().memoryService->getAll();
                    for (std::size_t i = 0; i < memories.size(); ++i)
                    {
                        Rect tileRect { 10.0f, 50.0f + i * 54.0f - m_scrollY, 300.0f, 48.0f };
                        if (tileRect.contains(point.x, point.y) && point.y >= 48.0f && point.y <= 230.0f)
                        {
                            app.setSelectedItem("others", memories[i].id);
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
            if (Rect { 10.0f, 48.0f, 300.0f, 182.0f }.contains(mPt.x, mPt.y))
            {
                m_targetScrollY -= event.wheel.y * 20.0f;
            }
        }
    }

    void OthersScreen::update(Application& app, float deltaSeconds)
    {
        std::size_t numMemories = 0;
        if (app.services().memoryService)
        {
            numMemories = app.services().memoryService->getAll().size();
        }

        float contentHeight = std::max(0.0f, static_cast<float>(numMemories) * 54.0f);
        float visibleHeight = 182.0f;
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

        std::vector<Memory> memories;
        if (app.services().memoryService)
        {
            memories = app.services().memoryService->getAll();
        }

        renderer.setClipRect(5.0f, 48.0f, 310.0f, 182.0f);

        for (std::size_t i = 0; i < memories.size(); ++i)
        {
            const auto& mem = memories[i];
            ListTile tile(Rect { 10.0f, 50.0f + i * 54.0f - m_scrollY, 300.0f, 48.0f }, 
                          Icon::Mic, 
                          mem.title.c_str(), 
                          mem.timestamp.c_str(), 
                          colors[i % colors.size()], 
                          SDL_Color { 0, 0, 0, 0 }, 
                          true);
            tile.render(renderer);
        }

        renderer.clearClipRect();
    }
}
