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
        /// Recording + upload state machine states.
        enum class RecordState
        {
            Idle,       ///< Mic button visible, idle
            Recording,  ///< Capturing audio (stub or real mic)
            Uploading,  ///< WAV file being uploaded to backend (background task)
            Result,     ///< Transcribed text received — showing result card
            Error,      ///< Upload or server error — showing error card
        };

        HomeScreen();
        ScreenId show(Touch& touch);

    private:
        void renderPage0(LovyanGFX& canvas, uint16_t w, uint16_t h, float offsetX);

        void renderPage1(LovyanGFX& canvas, uint16_t w, uint16_t h,
                         int remCount, int ideaCount, int qCount, int memCount, float offsetX);

        void processTouch(Touch& touch, uint16_t w, uint16_t h,
                          int remCount, int ideaCount, int qCount, int memCount,
                          ScreenId& targetScreen);

        // ── Services ────────────────────────────────────────────────────
        TimeService       m_timeService;



        // ── Recording state machine ──────────────────────────────────────
        RecordState m_recState      { RecordState::Idle };
        std::string m_resultText;     ///< Transcribed text from backend
        std::string m_errorText;      ///< Error message to display
        uint32_t    m_resultShownMs { 0 }; ///< Timestamp when result/error was set

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
