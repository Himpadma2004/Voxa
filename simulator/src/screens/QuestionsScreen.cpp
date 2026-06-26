#include "QuestionsScreen.h"

#include <vector>
#include <cmath>
#include <algorithm>

#include "../core/Application.h"
#include "../core/services/QuestionService.h"
#include "../core/models/Question.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Card.h"
#include "../widgets/ListTile.h"
#include "ScreenCommon.h"

namespace VOXA
{
    ScreenId QuestionsScreen::id() const
    {
        return ScreenId::Questions;
    }

    void QuestionsScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
            // Header back button hit area centered at (18, 28) with radius 11
            if (Rect { 5.0f, 15.0f, 26.0f, 26.0f }.contains(point.x, point.y))
            {
                app.navigateTo(ScreenId::Home);
            }
            else if (Rect { 10.0f, 54.0f, 300.0f, 175.0f }.contains(point.x, point.y))
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
                    auto questions = app.services().questions->getAll();
                    for (std::size_t i = 0; i < questions.size(); ++i)
                    {
                        Rect tileRect { 15.0f, 58.0f + i * 42.0f - m_scrollY, 290.0f, 38.0f };
                        if (tileRect.contains(point.x, point.y))
                        {
                            app.setSelectedItem("questions", questions[i].id);
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
            if (Rect { 10.0f, 52.0f, 300.0f, 180.0f }.contains(mPt.x, mPt.y))
            {
                m_targetScrollY -= event.wheel.y * 20.0f;
            }
        }
    }

    void QuestionsScreen::update(Application& app, float deltaSeconds)
    {
        std::size_t numQuestions = 0;
        if (app.services().questions)
        {
            numQuestions = app.services().questions->getAll().size();
        }

        float contentHeight = std::max(0.0f, static_cast<float>(numQuestions) * 42.0f - 10.0f);
        float visibleHeight = 170.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        m_targetScrollY = std::clamp(m_targetScrollY, 0.0f, maxScrollY);
        m_scrollY += (m_targetScrollY - m_scrollY) * 12.0f * deltaSeconds;
        if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
        {
            m_scrollY = m_targetScrollY;
        }
    }

    void QuestionsScreen::render(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Questions", true, true, Icon::Plus);

        // Center glass card container
        Card container(Rect { 10.0f, 52.0f, 300.0f, 180.0f }, Colors::Card, 16.0f);
        container.setShadow(Colors::Shadow, 4);
        container.setBorder(Colors::GlassBorder);
        container.render(renderer);

        std::vector<Question> questions;
        if (app.services().questions)
        {
            questions = app.services().questions->getAll();
        }

        renderer.setClipRect(10.0f, 54.0f, 300.0f, 175.0f);

        for (std::size_t i = 0; i < questions.size(); ++i)
        {
            ListTile tile(Rect { 15.0f, 58.0f + i * 42.0f - m_scrollY, 290.0f, 38.0f }, 
                          Icon::Question, 
                          questions[i].text.c_str(), 
                          questions[i].timestamp.c_str(), 
                          SDL_Color { 176, 84, 255, 255 }, 
                          SDL_Color { 0, 0, 0, 0 }, 
                          true);
            tile.render(renderer);
        }

        renderer.clearClipRect();
    }
}
