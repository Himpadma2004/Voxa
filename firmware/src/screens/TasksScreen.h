#pragma once
#include "ScreenCommon.h"
#include "../touch/Touch.h"

namespace VOXA
{
    class TasksScreen
    {
    public:
        ScreenId show(Touch& touch);

    private:
        float m_scrollY { 0.0f };
        float m_targetScrollY { 0.0f };
        bool  m_isDragging { false };
        float m_dragStartY { 0.0f };
        float m_dragStartScrollY { 0.0f };

        float m_scrollVelocity { 0.0f };
        float m_lastDragY { 0.0f };
        uint32_t m_lastTouchSampleMs { 0 };

        int   m_pressedItemIndex { -1 };
        bool  m_isBackPressed { false };
        bool  m_isSearchPressed { false };
        bool  m_isAddPressed { false };
        bool  m_wasTouched { false };

        float m_lastDragX { 0.0f };
    };
}
