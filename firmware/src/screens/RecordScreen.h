#pragma once
#include "ScreenCommon.h"
#include "../touch/Touch.h"

namespace VOXA
{
    class RecordScreen
    {
    public:
        ScreenId show(Touch& touch);

    private:
        bool  m_isBackPressed   { false };
        int   m_pressedButton   { -1 }; // 0 = Pause, 1 = SAVE, 2 = Trash
        bool  m_wasTouched      { false };
        float m_lastDragX       { 0.0f };
        float m_lastDragY       { 0.0f };
    };
}
