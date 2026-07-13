#pragma once

#include "ScreenCommon.h"
#include "../touch/Touch.h"

namespace VOXA
{
    class RecordingsLibraryScreen
    {
    public:
        ScreenId show(Touch& touch);

    private:
        float    m_scrollY           { 0.0f };
        float    m_targetScrollY     { 0.0f };
        float    m_dragStartY        { 0.0f };
        float    m_dragStartScrollY  { 0.0f };
        uint32_t m_lastTouchSampleMs { 0 };
        float    m_scrollVelocity    { 0.0f };
        float    m_lastDragX         { 0.0f };
        float    m_lastDragY         { 0.0f };
        bool     m_isDragging        { false };
        bool     m_wasTouched        { false };
        bool     m_isBackPressed     { false };
        int      m_pressedItemIndex  { -1 };

        // Deletion popup state
        int      m_selectedDeleteIndex    { -1 };
        bool     m_isConfirmDeletePressed { false };
        bool     m_isCancelDeletePressed  { false };
    };
}
