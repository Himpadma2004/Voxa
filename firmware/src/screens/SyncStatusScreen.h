#pragma once
#include "ScreenCommon.h"
#include "../touch/Touch.h"

namespace VOXA
{
    class SyncStatusScreen
    {
    public:
        ScreenId show(Touch& touch);

    private:
        bool m_isBackPressed { false };
        bool m_wasTouched { false };
        float m_lastDragX { 0.0f };
        float m_lastDragY { 0.0f };
    };
}
