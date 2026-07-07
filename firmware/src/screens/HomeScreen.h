#ifndef VOXA_HOMESCREEN_H
#define VOXA_HOMESCREEN_H

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "../display/Display.h"
#include "../touch/Touch.h"
#include "../services/TimeService.h"
#include "ScreenCommon.h"

namespace VOXA
{
    class HomeScreen
    {
    public:
        HomeScreen();

        ScreenId show(Touch& touch);

    private:
        void renderPage0(LGFX_Sprite& canvas, uint16_t w, uint16_t h, float offsetX);
        
        void renderPage1(LGFX_Sprite& canvas, uint16_t w, uint16_t h, 
                         int remCount, int ideaCount, int qCount, int memCount, float offsetX);
                         
        void processTouch(Touch& touch, uint16_t w, uint16_t h, 
                          int remCount, int ideaCount, int qCount, int memCount, 
                          ScreenId& targetScreen);

        TimeService m_timeService;

        float m_elapsed { 0.0f };
        int   m_page { 0 };               // 0 = Assistant Home, 1 = Menu list

        // Swipe & transition page state (horizontal)
        bool  m_isDragging { false };
        float m_dragStartX { 0.0f };
        float m_dragStartY { 0.0f };
        float m_swipeOffset { 0.0f };
        float m_scrollOffset { 0.0f };  // Horizontal visual page offset for transitions

        // Menu scroll state (vertical)
        bool  m_isScrollDragging { false };
        float m_menuScrollY { 0.0f };
        float m_menuTargetScrollY { 0.0f };
        float m_scrollVelocity { 0.0f }; // Vertical scroll inertia velocity
        float m_lastDragX { 0.0f };       // Previous touch X coordinate for release tracking
        float m_lastDragY { 0.0f };       // Previous touch Y coordinate for scroll delta
        uint32_t m_lastTouchSampleMs { 0 }; // Timestamp for drag velocity calculation

        // Touch feedback tracking
        int   m_pressedItemIndex { -1 };  // Pressed index of menu list items (-1 if none)
        bool  m_isMicPressed { false };
        bool  m_isChevronPressed { false };
        bool  m_isBackPressed { false };
        bool  m_isRotatePressed { false }; // Action button on page 1 header

        // Touch tracking state
        bool  m_wasTouched { false };
    };
}

#endif // VOXA_HOMESCREEN_H
