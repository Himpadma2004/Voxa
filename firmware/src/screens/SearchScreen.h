#pragma once
#include "ScreenCommon.h"
#include "../touch/Touch.h"
#include <string>

namespace VOXA
{
    enum class AiSearchState
    {
        Idle,
        RecordingVoice,
        Searching,
        HasResult
    };

    class SearchScreen
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
        bool  m_isVoiceSearchPressed { false };
        bool  m_isTypeSearchPressed { false };
        bool  m_wasTouched { false };

        float m_lastDragX { 0.0f };

        AiSearchState m_state { AiSearchState::Idle };
        std::string   m_lastQuery;
        std::string   m_lastAnswer;
        std::string   m_searchError;
        uint32_t      m_searchStartMs { 0 };
    };
}
