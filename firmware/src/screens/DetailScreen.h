#pragma once
#include "ScreenCommon.h"
#include "../touch/Touch.h"
#include <string>

namespace VOXA
{
    class DetailScreen
    {
    public:
        ScreenId show(Touch& touch);
        
        static void setItem(const std::string& category, uint32_t id, ScreenId backRoute);

    private:
        static std::string s_category;
        static uint32_t    s_itemId;
        static ScreenId    s_backRoute;

        bool m_isBackPressed { false };
        bool m_isDeletePressed { false };
        bool m_wasTouched { false };
        float m_lastDragX { 0.0f };
        float m_lastDragY { 0.0f };
    };
}
