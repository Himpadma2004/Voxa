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

// NOTE: SDCardService, SPIFFS, SD.h removed — SD card adapter unplugged.
// Upload now happens inside MicrophoneService::stopRecording() (PSRAM → HTTP POST).
// No SPIFFS file staging. No background upload task needed for the primary path.

#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace
{
    // Shared state for the upload result (populated by stopRecording which now uploads inline)
    volatile bool s_recUploadDone  = false;
    volatile bool s_recUploadOk    = false;
    char s_recUploadText[256]  = {};
    char s_recUploadError[128] = {};
    std::string s_recUploadPath;  // repurposed as recording title/label
}

namespace VOXA
{
    extern RecordingService recordingService;

    enum class UIState
    {
        Idle,
        Recording,
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

        while (targetScreen == ScreenId::Record)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            elapsed += deltaSecs;

            // ── Hardware Physical Button Handling ───────────────────────────
            // Auto-start recording if entered via physical button or pending trigger
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
                    Serial.println("[RecordScreen] Direct Hardware Button Recording Started!");
                }
            }
            // Stop recording when hardware button is released or pressed
            else if (uiState == UIState::Recording && buttonService.hasPendingStopTrigger())
            {
                buttonService.clearPendingStopTrigger();
                buttonService.clearPendingRecordTrigger();
                Serial.println("[RecordScreen] Hardware Button Release/Stop -> Stopping recording and uploading...");
                if (microphoneService.getDurationMs() < 300)
                {
                    vTaskDelay(pdMS_TO_TICKS(150));
                }
                AudioManager::instance().playTone(800, 80);
                bool stopOk = microphoneService.stopRecording("RecordScreen::hwButtonStop", "hw_button_release");

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
            else if (uiState == UIState::Uploading || uiState == UIState::Result)
            {
                // Clear any stray button triggers while uploading or viewing result
                buttonService.clearPendingRecordTrigger();
                buttonService.clearPendingStopTrigger();
            }

            // ── Upload Result Handling ───────────────────────────────────────
            // stopRecording() already completed the upload synchronously.
            // s_recUploadDone is set immediately after the call returns.
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

                    // audio_id from backend becomes the filePath / title in the cloud record
                    recordingService.add(resultText, resultText, durS, "Uploaded");
                    Serial.printf("[RecordScreen] Cloud upload success: audio_id=%s\n", resultText.c_str());

                    // Asynchronously trigger cloud sync without blocking the UI thread
                    xTaskCreate([](void*) {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        dataService.syncAll();
                        vTaskDelete(nullptr);
                    }, "sync_task", 4096, nullptr, 1, nullptr);
                    Serial.println("[RecordScreen] Async DataSync scheduled.");
                }
                else
                {
                    // Upload failed — mark as Pending for retry monitor
                    recordingService.add("Pending Note", s_recUploadText, durS, "Pending");
                    resultText = "Upload failed — will retry";
                    uiState = UIState::Result;
                    stateChangeMs = millis();
                    Serial.printf("[RecordScreen] Upload failed: %s\n", s_recUploadError);
                }
            }

            // Auto-dismiss result card back to Idle after 8 seconds
            if ((uiState == UIState::Result || uiState == UIState::Error) &&
                (millis() - stateChangeMs) > 8000)
            {
                uiState = UIState::Idle;
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

                // stopRecording() blocks briefly then posts WAV to cloud
                bool stopOk = microphoneService.stopRecording("RecordScreen::autoStop", "60s_timeout");
                s_recUploadOk = stopOk;
                strncpy(s_recUploadText, microphoneService.getLastAudioId().c_str(), 255);
                s_recUploadDone = true;
            }

            // 1. Process Touch (Immediate zero-latency response)
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

                    // Back button bounds (top-left)
                    if (std::sqrt((tx - 20.0f) * (tx - 20.0f) + (ty - 45.0f) * (ty - 45.0f)) <= 24.0f)
                    {
                        m_isBackPressed = true;
                    }

                    // Center mic/stop button coordinates (aligned with rendered mic at 0.48f)
                    float micCx = w * 0.5f;
                    float micCy = h * 0.48f;
                    float dist = std::sqrt((tx - micCx) * (tx - micCx) + (ty - micCy) * (ty - micCy));

                    if (!m_isBackPressed)
                    {
                        // Action depends on current state
                        if (uiState == UIState::Idle && dist <= 60.0f)
                        {
                            // Use a timestamped title — no SPIFFS path needed
                            static uint32_t s_recSeq = 0;
                            s_recSeq++;
                            char titleBuf[48];
                            snprintf(titleBuf, sizeof(titleBuf), "Voice Note #%u", (unsigned)s_recSeq);
                            s_recUploadPath = titleBuf;

                            AudioManager::instance().playTone(1200, 80);
                            vTaskDelay(pdMS_TO_TICKS(90));
                            if (microphoneService.startRecording(s_recUploadPath, "RecordScreen::micButtonPressed"))
                            {
                                uiState = UIState::Recording;
                            }
                        }
                        else if (uiState == UIState::Recording)
                        {
                            // Any tap on screen while recording stops recording and starts upload
                            const uint32_t MIN_RECORDING_MS = 250;
                            if (microphoneService.getDurationMs() >= MIN_RECORDING_MS)
                            {
                                Serial.println("[RecordScreen] Screen tap -> Executing stopRecording()");
                                AudioManager::instance().playTone(800, 80);
                                bool stopOk = microphoneService.stopRecording("RecordScreen::micButtonToggle", "user_screen_tap");

                                if (stopOk)
                                {
                                    // Upload happens synchronously inside stopRecording()
                                    s_recUploadDone  = false;
                                    s_recUploadOk    = stopOk;
                                    strncpy(s_recUploadText,
                                            microphoneService.getLastAudioId().c_str(), 255);
                                    memset(s_recUploadError, 0, sizeof(s_recUploadError));
                                    s_recUploadDone  = true;

                                    uiState = UIState::Uploading; // triggers result block next frame
                                }
                                else
                                {
                                    resultText = "Empty transcript — please record again.";
                                    uiState = UIState::Result;
                                    stateChangeMs = millis();
                                }
                            }
                        }
                        else if (uiState == UIState::Error || uiState == UIState::Result)
                        {
                            // Dismiss result / error back to idle
                            uiState = UIState::Idle;
                        }
                    }
                }
                else
                {
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        if (uiState == UIState::Recording && microphoneService.getState() == RecordingState::Recording)
                        {
                            if (microphoneService.getDurationMs() >= 700)
                            {
                                microphoneService.stopRecording("RecordScreen::swipeBack", "swipe_back_navigation");
                                targetScreen = ScreenId::Home;
                            }
                            // else: too soon — ignore the swipe, keep recording, stay on screen
                        }
                        else
                        {
                            targetScreen = ScreenId::Home;
                        }
                        swipeBackCandidate = false;
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;
                    if (m_isBackPressed)
                    {
                        if (uiState == UIState::Recording && microphoneService.getState() == RecordingState::Recording)
                        {
                            if (microphoneService.getDurationMs() >= 700)
                            {
                                microphoneService.stopRecording("RecordScreen::backButton", "back_button_pressed");
                                targetScreen = ScreenId::Home;
                            }
                            // else: too soon — ignore, keep recording, stay on screen
                        }
                        else
                        {
                            targetScreen = ScreenId::Home;
                        }
                    }
                    m_isBackPressed = false;
                }
            }

            // Dimensions re-query
            w = Display::width();
            h = Display::height();

            // 2. Render older Recorder UI
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Voice Recorder", true, false, Icon::Plus, w, h);

            // Render Back button
            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back,
                                               backFill, backColor, w, h);

            float cx = w * 0.5f;
            float cy = h * 0.48f; // Shift mic up slightly

            float micR = 30.0f;
            bool isRecording = (uiState == UIState::Recording);

            // ── Concentric ambient halos (animating when recording/idle) ───────
            uint16_t bg = VoxaTheme::getBackground();
            uint8_t bg_r = (bg >> 11) << 3;
            uint8_t bg_g = ((bg >> 5) & 0x3F) << 2;
            uint8_t bg_b = (bg & 0x1F) << 3;

            for (int r = 0; r < 3; ++r)
            {
                float progress = std::fmod((elapsed * 0.6f) + (r * 0.33f), 1.0f);
                float rippleR = micR + progress * 40.0f;
                float alpha = 1.0f - progress; // fade out

                uint8_t target_r = isRecording ? 240 : (uint8_t)((VoxaTheme::getPrimary() >> 11) << 3);
                uint8_t target_g = isRecording ? 40 : (uint8_t)(((VoxaTheme::getPrimary() >> 5) & 0x3F) << 2);
                uint8_t target_b = isRecording ? 40 : (uint8_t)((VoxaTheme::getPrimary() & 0x1F) << 3);

                uint16_t rippleColor = canvas.color565(
                    (uint8_t)((1.0f - alpha) * bg_r + alpha * target_r),
                    (uint8_t)((1.0f - alpha) * bg_g + alpha * target_g),
                    (uint8_t)((1.0f - alpha) * bg_b + alpha * target_b));

                canvas.drawCircle((int)cx, (int)cy, (int)rippleR, rippleColor);
                canvas.drawCircle((int)cx, (int)cy, (int)(rippleR - 1.0f), rippleColor);
            }

            // Core mic button (flashes red when recording, shows yellow/orange spinner color when uploading)
            uint16_t btnColor = isRecording ? 0xE800 : VoxaTheme::getPrimary();
            if (uiState == UIState::Uploading)
                btnColor = 0xFBE0; // Amber/yellow for upload
            else if (uiState == UIState::Result)
                btnColor = 0x2508; // Green for success
            else if (uiState == UIState::Error)
                btnColor = 0xD104; // Darker red for error

            canvas.fillCircle((int)cx, (int)cy, (int)micR, btnColor);
            canvas.drawCircle((int)cx, (int)cy, (int)micR, VoxaTheme::getTextPrimary());

            // Mic icon inside
            ScreenCommon::drawMicShape(canvas, cx, cy, micR * 0.85f * 2.0f, VoxaTheme::getTextPrimary(), btnColor);

            // 3. Render Status Details & Sound Waveforms / Spinner
            canvas.setFont(&fonts::FreeSansBold12pt7b);
            canvas.setTextColor(VoxaTheme::getTextPrimary());
            canvas.setTextDatum(textdatum_t::middle_center);

            if (uiState == UIState::Recording)
            {
                uint32_t durationMs = microphoneService.getDurationMs();
                int mins = (int)durationMs / 60000;
                int secs = ((int)durationMs % 60000) / 1000;
                char timeStr[32];
                sprintf(timeStr, "%02d:%02d", mins, secs);
                canvas.drawString(timeStr, cx, h * 0.73f);

                // Blinking RECORDING label
                float blink = std::sin(elapsed * 6.0f) * 0.4f + 0.6f;
                uint16_t recColor = canvas.color565((uint8_t)(blink * 240), 20, 20);
                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextColor(recColor);

                canvas.fillCircle((int)(cx - 58.0f), (int)(h * 0.81f), 4, recColor);
                canvas.drawString("RECORDING", cx + 6.0f, h * 0.81f);

                // Render visualizer waves
                float waveCenterY = h * 0.90f;
                float waveHeight = 8.0f;
                int prevY1 = (int)waveCenterY;
                int prevY2 = (int)waveCenterY;

                for (int x = 30; x <= (w - 30); x += 4)
                {
                    float envelope = std::sin((x - 30.0f) / (w - 60.0f) * M_PI);
                    float y1 = waveCenterY + std::sin(x * 0.08f + elapsed * 12.0f) * waveHeight * envelope;
                    float y2 = waveCenterY + std::sin(x * 0.14f - elapsed * 9.0f) * waveHeight * 0.5f * envelope;

                    if (x > 30)
                    {
                        canvas.drawLine(x - 4, prevY1, x, (int)y1, VoxaTheme::getPrimary());
                        canvas.drawLine(x - 4, prevY2, x, (int)y2, VoxaTheme::getPrimaryLight());
                    }
                    prevY1 = (int)y1;
                    prevY2 = (int)y2;
                }
            }
            else if (uiState == UIState::Uploading)
            {
                canvas.drawString("Uploading...", cx, h * 0.73f);

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextColor(VoxaTheme::getTextSecondary());
                canvas.drawString("SENDING TO CLOUD", cx, h * 0.81f);

                // Display a rotating spinner line
                float spinAngle = elapsed * 5.0f;
                int spinR = 10;
                int sx1 = cx + cos(spinAngle) * spinR;
                int sy1 = h * 0.90f + sin(spinAngle) * spinR;
                int sx2 = cx - cos(spinAngle) * spinR;
                int sy2 = h * 0.90f - sin(spinAngle) * spinR;
                canvas.drawLine(sx1, sy1, sx2, sy2, VoxaTheme::getPrimary());
            }
            else if (uiState == UIState::Result)
            {
                bool isEmptyTranscript = (resultText.find("Empty transcript") != std::string::npos ||
                                          resultText.find("please record again") != std::string::npos);

                if (isEmptyTranscript)
                {
                    canvas.drawString("No speech detected", cx, h * 0.73f);

                    canvas.setFont(&fonts::FreeSans9pt7b);
                    canvas.setTextColor(canvas.color565(240, 160, 40));
                    canvas.drawString("PLEASE RECORD AGAIN", cx, h * 0.81f);

                    canvas.drawFastHLine(40, h * 0.90f, w - 80, canvas.color565(240, 160, 40));
                }
                else
                {
                    std::string disp = resultText.empty() ? "Success!" : resultText;
                    if (disp.length() > 28)
                        disp = disp.substr(0, 25) + "...";
                    canvas.drawString(disp.c_str(), cx, h * 0.73f);

                    canvas.setFont(&fonts::FreeSans9pt7b);
                    canvas.setTextColor(canvas.color565(80, 200, 100));
                    canvas.drawString("COMPLETED", cx, h * 0.81f);

                    // Draw a static centerline wave in green
                    canvas.drawFastHLine(40, h * 0.90f, w - 80, canvas.color565(80, 200, 100));
                }
            }
            else if (uiState == UIState::Error)
            {
                std::string disp = errorText.empty() ? "Error" : errorText;
                if (disp.length() > 28)
                    disp = disp.substr(0, 25) + "...";
                canvas.drawString(disp.c_str(), cx, h * 0.73f);

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextColor(canvas.color565(220, 60, 60));
                canvas.drawString("TAP MIC TO RETRY", cx, h * 0.81f);

                // Draw a static centerline wave in red
                canvas.drawFastHLine(40, h * 0.90f, w - 80, canvas.color565(220, 60, 60));
            }
            else
            {
                // Idle state
                canvas.drawString("00:00", cx, h * 0.73f);

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextColor(VoxaTheme::getTextSecondary());
                canvas.drawString("TAP TO RECORD", cx, h * 0.81f);

                // Draw static flat centerline wave
                canvas.drawFastHLine(40, h * 0.90f, w - 80, VoxaTheme::getDivider());
            }

            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Record), entryFrame, 10);
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