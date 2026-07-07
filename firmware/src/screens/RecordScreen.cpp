#include "RecordScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/RecordingService.h"
#include "Transition.h"
#include <cmath>

namespace VOXA
{
    extern RecordingService recordingService;

    ScreenId RecordScreen::show(Touch& touch)
    {
        int entryFrame = 0;
        float dragStartX = 0.0f;
        float dragStartY = 0.0f;
        bool swipeBackCandidate = false;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            return ScreenId::Home;
        }

        ScreenId targetScreen = ScreenId::Record;
        uint32_t lastMs = millis();
        float elapsed = 0.0f;

        bool isRecording = false;
        float recordingDuration = 0.0f;

        while (targetScreen == ScreenId::Record)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            elapsed += deltaSecs;
            if (isRecording)
            {
                recordingDuration += deltaSecs;
            }

            // 1. Process Touch
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            if (touched && entryFrame >= 10)
            {
                m_lastDragX = tx;
                m_lastDragY = ty;
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    dragStartX = tx;
                    dragStartY = ty;
                    swipeBackCandidate = (tx < 50);

                    // Back button bounds Y = 45
                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isBackPressed = true;
                    }

                    // Mic button bounds
                    float micCx = w * 0.5f;
                    float micCy = h * 0.52f;
                    if (std::sqrt((tx - micCx)*(tx - micCx) + (ty - micCy)*(ty - micCy)) <= 42.0f)
                    {
                        m_isMicPressed = true;
                    }
                }
                else
                {
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        if (isRecording)
                        {
                            isRecording = false;
                            recordingService.add("Voice Memo", "filePath.wav", (uint32_t)recordingDuration, "Jul 07");
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
                    float rx = m_lastDragX;
                    float ry = m_lastDragY;

                    if (m_isBackPressed)
                    {
                        // If recording, stop it first
                        if (isRecording)
                        {
                            isRecording = false;
                            recordingService.add("Voice Memo", "filePath.wav", (uint32_t)recordingDuration, "Jul 07");
                        }
                        targetScreen = ScreenId::Home;
                    }
                    else if (m_isMicPressed)
                    {
                        // Toggle recording
                        isRecording = !isRecording;
                        if (!isRecording)
                        {
                            // Save recording
                            recordingService.add("Voice Memo", "filePath.wav", (uint32_t)recordingDuration, "Jul 07");
                            Serial.println("[Recorder] Saved recording");
                        }
                        else
                        {
                            recordingDuration = 0.0f;
                            Serial.println("[Recorder] Started recording");
                        }
                    }
                    m_isBackPressed = false;
                    m_isMicPressed = false;
                }
            }

            // Dimensions re-query
            w = Display::width();
            h = Display::height();

            // 2. Render
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Voice Recorder", true, false, Icon::Plus, w, h);

            // Render Back button pressed state
            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, 
                                              backFill, backColor, w, h);

            float cx = w * 0.5f;
            float cy = h * 0.52f;

            // Concentric ambient halos (pulsating if recording)
            float micR = m_isMicPressed ? 26.0f : 30.0f;
            float pulse = isRecording ? (std::sin(elapsed * 8.0f) * 0.5f + 0.5f) : 0.0f;

            uint16_t bg = VoxaTheme::getBackground();
            uint8_t bg_r = (bg >> 11) << 3;
            uint8_t bg_g = ((bg >> 5) & 0x3F) << 2;
            uint8_t bg_b = (bg & 0x1F) << 3;
            
            float alpha1 = isRecording ? ((30.0f + pulse * 25.0f) / 255.0f) : 0.08f;
            
            uint16_t haloColor = canvas.color565(
                (uint8_t)((1.0f - alpha1) * bg_r + alpha1 * 255), // red halo if recording
                (uint8_t)((1.0f - alpha1) * bg_g + alpha1 * (isRecording ? 80 : 92)),
                (uint8_t)((1.0f - alpha1) * bg_b + alpha1 * (isRecording ? 80 : 255))
            );

            canvas.drawCircle((int)cx, (int)cy, (int)(micR + 6.0f + pulse * 6.0f), haloColor);
            canvas.drawCircle((int)cx, (int)cy, (int)(micR + 12.0f + pulse * 12.0f), haloColor);

            uint16_t btnColor = isRecording ? 0xF800 : (m_isMicPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getPrimary());
            canvas.fillCircle((int)cx, (int)cy, (int)micR, btnColor);
            canvas.drawCircle((int)cx, (int)cy, (int)micR, VoxaTheme::getTextPrimary());

            // Mic icon inside
            ScreenCommon::drawMicShape(canvas, cx, cy, micR * 0.85f * 2.0f, VoxaTheme::getTextPrimary(), btnColor);

            // Timer & Status Details
            canvas.setFont(&fonts::DejaVu18);
            canvas.setTextColor(VoxaTheme::getTextPrimary());
            canvas.setTextDatum(textdatum_t::middle_center);

            if (isRecording)
            {
                int mins = (int)recordingDuration / 60;
                int secs = (int)recordingDuration % 60;
                char timeStr[32];
                sprintf(timeStr, "%02d:%02d", mins, secs);
                canvas.drawString(timeStr, cx, h * 0.82f);

                canvas.setFont(&fonts::DejaVu12);
                canvas.setTextColor(0xF800); // red recording tag
                canvas.drawString("RECORDING", cx, h * 0.90f);
            }
            else
            {
                canvas.drawString("00:00", cx, h * 0.82f);

                canvas.setFont(&fonts::DejaVu12);
                canvas.setTextColor(VoxaTheme::getTextSecondary());
                canvas.drawString("TAP TO RECORD", cx, h * 0.90f);
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

        return targetScreen;
    }
}
