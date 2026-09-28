#pragma once
#include "ScreenCommon.h"
#include "../touch/Touch.h"

namespace VOXA
{
    class SettingsScreen
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
        bool  m_wasTouched { false };

        enum class ViewMode {
            Main,
            Network,
            Storage,
            About,
            Zeroize
        };
        ViewMode m_viewMode { ViewMode::Main };

        bool  m_torProxyEnabled { false };
        bool  m_isSyncPressed { false };
        bool  m_isCancelZeroizePressed { false };
        bool  m_isConfirmZeroizePressed { false };

        float m_lastDragX { 0.0f };
    };
}
