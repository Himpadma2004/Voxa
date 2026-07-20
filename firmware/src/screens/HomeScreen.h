#ifndef VOXA_HOMESCREEN_H
#define VOXA_HOMESCREEN_H

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "../display/Display.h"
#include "../touch/Touch.h"
#include "../services/TimeService.h"
#include "../services/MicrophoneService.h"
#include "ScreenCommon.h"
#include <string>

namespace VOXA
{
    class HomeScreen
    {
    public:
        HomeScreen();
        void begin();
        ScreenId show(Touch& touch);

    private:
        void renderPage0(LovyanGFX& canvas, uint16_t w, uint16_t h, float offsetX);

        void renderPage1(LovyanGFX& canvas, uint16_t w, uint16_t h,
                         int remCount, int ideaCount, int qCount, int taskCount, int memCount, float offsetX);

        void processTouch(Touch& touch, uint16_t w, uint16_t h,
                          int remCount, int ideaCount, int qCount, int taskCount, int memCount,
                          ScreenId& targetScreen);

        // ── Services ────────────────────────────────────────────────────
        TimeService       m_timeService;

        // ── Animation ───────────────────────────────────────────────────
        float m_elapsed { 0.0f };
        int   m_page    { 0 };    // 0 = Assistant Home, 1 = Menu list

        // ── Swipe & horizontal page transition ───────────────────────────
        bool  m_isDragging   { false };
        float m_dragStartX   { 0.0f };
        float m_dragStartY   { 0.0f };
        float m_swipeOffset  { 0.0f };
        float m_scrollOffset { 0.0f };

        // ── Menu vertical scroll ─────────────────────────────────────────
        bool     m_isScrollDragging    { false };
        float    m_menuScrollY         { 0.0f };
        float    m_menuTargetScrollY   { 0.0f };
        float    m_scrollVelocity      { 0.0f };
        float    m_lastDragX           { 0.0f };
        float    m_lastDragY           { 0.0f };
        uint32_t m_lastTouchSampleMs   { 0 };

        // ── Touch feedback ───────────────────────────────────────────────
        int  m_pressedItemIndex  { -1 };
        bool m_isMicPressed      { false };
        bool m_isChevronPressed  { false };
        bool m_isBackPressed     { false };
        bool m_isRotatePressed   { false };
        bool m_wasTouched        { false };
    };
}

#endif // VOXA_HOMESCREEN_H
