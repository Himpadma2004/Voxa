#include "RecordScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/RecordingService.h"
#include "../services/MicrophoneService.h"
#include "../services/ApiClient.h"
#include "../services/WiFiManager.h"
#include "../services/DataService.h"
#include "../audio/AudioManager.h"
#include "../services/ButtonService.h"
#include "../services/PowerManager.h"
#include "Transition.h"

#include <cmath>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace
{
    volatile bool s_recUploadDone  = false;
    volatile bool s_recUploadOk    = false;
    char s_recUploadText[256]  = {};
    char s_recUploadError[128] = {};
    std::string s_recUploadPath;
}

namespace VOXA
{
    extern RecordingService recordingService;

    enum class UIState
    {
        Idle,
        Recording,
        Paused,
        Uploading,
        Result,
        Error
    };

    ScreenId RecordScreen::show(Touch &touch)
    {
        int entryFrame = 0;
        float dragStartX = 0.0f;
        float dragStartY = 0.0f;
        bool swipeBackCandidate = false;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            Serial.println("[RecordScreen] Failed to create PSRAM canvas sprite!");
            return ScreenId::Home;
        }

        ScreenId targetScreen = ScreenId::Record;
        uint32_t lastMs = millis();
        float elapsed = 0.0f;

        UIState uiState = UIState::Idle;
        uint32_t stateChangeMs = 0;

        std::string resultText = "";
        std::string errorText = "";

        // Auto-start recording on entering screen if not already recording
        if (!microphoneService.isRecording())
        {
            static uint32_t s_recSeq = 0;
            s_recSeq++;
            char titleBuf[48];
            snprintf(titleBuf, sizeof(titleBuf), "Voice Note #%u", (unsigned)s_recSeq);
            s_recUploadPath = titleBuf;

            AudioManager::instance().playTone(1200, 80);
            if (microphoneService.startRecording(s_recUploadPath, "RecordScreen::autoStart"))
            {
                uiState = UIState::Recording;
            }
        }
        else
        {
            uiState = microphoneService.isPaused() ? UIState::Paused : UIState::Recording;
        }

        while (targetScreen == ScreenId::Record)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            elapsed += deltaSecs;

            // ── Hardware Physical Button Handling ───────────────────────────
            if (uiState == UIState::Idle && (ButtonService::isDirectRecordRequested() || buttonService.hasPendingRecordTrigger()))
            {
                ButtonService::clearDirectRecordRequest();
                buttonService.clearPendingRecordTrigger();
                static uint32_t s_recSeq = 0;
                s_recSeq++;
                char titleBuf[48];
                snprintf(titleBuf, sizeof(titleBuf), "Voice Note #%u", (unsigned)s_recSeq);
                s_recUploadPath = titleBuf;

                if (microphoneService.startRecording(s_recUploadPath, "RecordScreen::hardwareButtonStart"))
                {
                    AudioManager::instance().playTone(1200, 80);
                    uiState = UIState::Recording;
                }
            }
            else if ((uiState == UIState::Recording || uiState == UIState::Paused) && buttonService.hasPendingStopTrigger())
            {
                buttonService.clearPendingStopTrigger();
                buttonService.clearPendingRecordTrigger();
                Serial.println("[RecordScreen] Hardware Button Stop -> Saving recording...");
                
                AudioManager::instance().playTone(800, 80);
                bool stopOk = microphoneService.stopRecording("RecordScreen::hwButtonStop", "hw_button_press");

                if (stopOk)
                {
                    s_recUploadDone  = false;
                    s_recUploadOk    = stopOk;
                    strncpy(s_recUploadText, microphoneService.getLastAudioId().c_str(), 255);
                    memset(s_recUploadError, 0, sizeof(s_recUploadError));
                    s_recUploadDone  = true;
                    uiState = UIState::Uploading;
                }
                else
                {
                    resultText = "Empty recording — please hold button to record.";
                    uiState = UIState::Result;
                    stateChangeMs = millis();
                }
            }

            // ── Upload Result Handling ───────────────────────────────────────
            if (uiState == UIState::Uploading && s_recUploadDone)
            {
                s_recUploadDone = false;
                uint32_t durS = (microphoneService.getDurationMs() + 500) / 1000;
                if (durS == 0) durS = 1;

                if (s_recUploadOk)
                {
                    resultText = s_recUploadText;
                    uiState = UIState::Result;
                    stateChangeMs = millis();

                    recordingService.add(resultText, resultText, durS, "Uploaded");
                    Serial.printf("[RecordScreen] Cloud upload success: audio_id=%s\n", resultText.c_str());

                    xTaskCreate([](void*) {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        dataService.syncAll();
                        vTaskDelete(nullptr);
                    }, "sync_task", 4096, nullptr, 1, nullptr);
                }
                else
                {
                    recordingService.add("Pending Note", s_recUploadText, durS, "Pending");
                    resultText = "Upload failed — will retry";
                    uiState = UIState::Result;
                    stateChangeMs = millis();
                }
            }

            // Auto-dismiss result card back to Home after 3 seconds
            if ((uiState == UIState::Result || uiState == UIState::Error) &&
                (millis() - stateChangeMs) > 3000)
            {
                targetScreen = ScreenId::Home;
            }

            // Auto-stop recording after 60 seconds
            if (uiState == UIState::Recording && microphoneService.getDurationMs() >= 60000)
            {
                Serial.println("[RecordScreen] Auto-stop: 60s limit reached.");
                uiState = UIState::Uploading;
                s_recUploadDone  = false;
                s_recUploadOk    = false;
                memset(s_recUploadText,  0, sizeof(s_recUploadText));
                memset(s_recUploadError, 0, sizeof(s_recUploadError));

                bool stopOk = microphoneService.stopRecording("RecordScreen::autoStop", "60s_timeout");
                s_recUploadOk = stopOk;
                strncpy(s_recUploadText, microphoneService.getLastAudioId().c_str(), 255);
                s_recUploadDone = true;
            }

            // 1. Process Touch
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            if (touched)
            {
                m_lastDragX = tx;
                m_lastDragY = ty;
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    dragStartX = tx;
                    dragStartY = ty;
                    swipeBackCandidate = (tx < 50);

                    // 1. Bottom STOP & SAVE Button (Hitbox Y: 250..320)
                    if (ty >= 245)
                    {
                        m_pressedButton = 1; // STOP & SAVE
                    }
                    // 2. Top-Left Back / Cancel (Hitbox X: 0..80, Y: 0..45)
                    else if (tx <= 80 && ty <= 45)
                    {
                        m_isBackPressed = true;
                    }
                    // 3. Center Waveform Tap to toggle Pause / Resume
                    else if (ty >= 110 && ty <= 210)
                    {
                        m_pressedButton = 0; // Pause / Resume
                    }
                }
                else
                {
                    // Swipe back check
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        if (microphoneService.isRecording())
                        {
                            microphoneService.stopRecording("RecordScreen::swipeBack", "swipe_back");
                        }
                        targetScreen = ScreenId::Home;
                        swipeBackCandidate = false;
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;

                    // Back button release action
                    if (m_isBackPressed)
                    {
                        m_isBackPressed = false;
                        AudioManager::instance().playTapSoundAsync();
                        if (microphoneService.isRecording())
                        {
                            microphoneService.stopRecording("RecordScreen::backButton", "back_button");
                        }
                        targetScreen = ScreenId::Home;
                    }

                    // Action buttons release execution
                    if (m_pressedButton == 0) // Pause / Resume
                    {
                        AudioManager::instance().playTapSoundAsync();
                        if (microphoneService.isRecording() && !microphoneService.isPaused())
                        {
                            microphoneService.pauseRecording("RecordScreen::pauseBtn");
                            uiState = UIState::Paused;
                        }
                        else if (microphoneService.isPaused())
                        {
                            microphoneService.resumeRecording("RecordScreen::resumeBtn");
                            uiState = UIState::Recording;
                        }
                    }
                    else if (m_pressedButton == 1) // STOP & SAVE
                    {
                        AudioManager::instance().playTone(1000, 80);
                        Serial.println("[RecordScreen] STOP & SAVE tapped -> stopping and uploading");
                        bool stopOk = microphoneService.stopRecording("RecordScreen::saveBtn", "user_save");
                        if (stopOk)
                        {
                            s_recUploadDone  = false;
                            s_recUploadOk    = stopOk;
                            strncpy(s_recUploadText, microphoneService.getLastAudioId().c_str(), 255);
                            memset(s_recUploadError, 0, sizeof(s_recUploadError));
                            s_recUploadDone  = true;
                            uiState = UIState::Uploading;
                        }
                        else
                        {
                            resultText = "Empty recording — please hold to record.";
                            uiState = UIState::Result;
                            stateChangeMs = millis();
                        }
                    }

                    m_pressedButton = -1;
                }
            }

            // Dimensions re-query
            w = Display::width();
            h = Display::height();

            // ── 2. RENDER MINIMAL RECORDING SCREEN ───────────────────────────
            canvas.fillScreen(TFT_BLACK);

            // ── Top Header ──────────────────────────────────────────
            float topY = 16.0f;
            bool isPaused = (uiState == UIState::Paused || microphoneService.isPaused());

            // Left: Amber Dot + RECORDING
            uint16_t amberColor = canvas.color565(245, 160, 40); // Warm gold/amber
            if (isPaused)
            {
                canvas.fillCircle(18, (int)(topY + 5.0f), 3, canvas.color565(140, 100, 30));
                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(canvas.color565(180, 140, 60));
                canvas.drawString("PAUSED", 26.0f, topY);
            }
            else
            {
                float blink = std::sin(elapsed * 6.0f) * 0.35f + 0.65f;
                uint8_t r = (uint8_t)(245 * blink);
                uint8_t g = (uint8_t)(160 * blink);
                uint8_t b = (uint8_t)(40 * blink);
                uint16_t dotCol = canvas.color565(r, g, b);

                canvas.fillCircle(18, (int)(topY + 5.0f), 3, dotCol);
                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(amberColor);
                canvas.drawString("RECORDING", 26.0f, topY);
            }

            // Right: RAW in subtle muted gray
            canvas.setFont(&fonts::DejaVu9);
            canvas.setTextDatum(textdatum_t::top_right);
            canvas.setTextColor(canvas.color565(120, 130, 145));
            canvas.drawString("RAW", w - 18.0f, topY);

            // ── Center Timer Display (04:18.42) ─────────────────────
            uint32_t durMs = microphoneService.getDurationMs();
            int mins = (int)(durMs / 60000);
            int secs = (int)((durMs % 60000) / 1000);
            int cs = (int)((durMs % 1000) / 10);

            char timeBuf[24];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d.%02d", mins, secs, cs);

            canvas.setFont(&fonts::FreeSansBold18pt7b);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(0xFFFF); // Clean high-contrast white
            float timerY = 126.0f;
            canvas.drawString(timeBuf, w * 0.5f, timerY);

            // ── Center Live Waveform ─────────────────────────────────
            // 5 vertical rounded white bars matching the reference design
            float waveCenterY = 170.0f;
            float waveCenterX = w * 0.5f;

            int micDb = microphoneService.getInputLevelDb();
            float normVol = std::max(0.08f, std::min(1.0f, (micDb + 55.0f) / 50.0f));

            const float barOffsets[5] = { -22.0f, -11.0f, 0.0f, 11.0f, 22.0f };
            const float barBaseH[5]   = { 14.0f,  24.0f,  34.0f, 24.0f, 14.0f };
            const float barW          = 4.5f;

            for (int i = 0; i < 5; ++i)
            {
                float bx = waveCenterX + barOffsets[i];
                float animScale = 1.0f;
                if (!isPaused && uiState == UIState::Recording)
                {
                    float waveAnim = std::sin(elapsed * 12.0f + i * 1.1f);
                    animScale = 0.55f + 0.45f * waveAnim * normVol;
                }
                else if (isPaused)
                {
                    animScale = 0.5f;
                }

                float barH = std::max(6.0f, barBaseH[i] * animScale);
                float barY = waveCenterY - barH * 0.5f;
                canvas.fillRoundRect((int)(bx - barW * 0.5f), (int)barY, (int)barW, (int)barH, 2, 0xFFFF);
            }

            // ── Uploading / Saving Feedback (if any) ─────────────────
            if (uiState == UIState::Uploading)
            {
                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(canvas.color565(160, 175, 195));
                canvas.drawString("SAVING & SYNCING...", w * 0.5f, 218.0f);
            }
            else if (uiState == UIState::Result)
            {
                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(canvas.color565(52, 211, 153));
                canvas.drawString("RECORDING SAVED", w * 0.5f, 218.0f);
            }

            // ── Bottom Action Button: STOP & SAVE ────────────────────
            float btnX = 14.0f;
            float btnW = w - 28.0f;
            float btnY = h - 46.0f;
            float btnH = 34.0f;

            bool isBtnPressed = (m_pressedButton == 1);
            uint16_t btnBg = isBtnPressed ? canvas.color565(200, 200, 200) : 0xFFFF;

            canvas.fillRoundRect((int)btnX, (int)btnY, (int)btnW, (int)btnH, 4, btnBg);

            canvas.setFont(&fonts::DejaVu9);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(canvas.color565(40, 45, 55)); // Crisp dark uppercase
            canvas.drawString("STOP & SAVE", w * 0.5f, btnY + btnH * 0.5f);

            // Push Sprite to screen
            if (entryFrame < 6)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Record), entryFrame, 6);
                entryFrame++;
            }
            else
            {
                canvas.pushSprite(0, 0);
            }

            uint32_t frameMs = millis() - nowMs;
            if (frameMs < 16)
            {
                delay(16 - frameMs);
            }
        }

        canvas.deleteSprite();
        return targetScreen;
    }
}