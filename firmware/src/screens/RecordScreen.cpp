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
            float cy = h * 0.48f; // Shift mic up slightly to make room for audio waves at the bottom

            float micR = m_isMicPressed ? 26.0f : 30.0f;

            // 1. Draw 3 expanding and fading ripple rings
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
                uint8_t target_g = isRecording ? 40  : (uint8_t)(((VoxaTheme::getPrimary() >> 5) & 0x3F) << 2);
                uint8_t target_b = isRecording ? 40  : (uint8_t)((VoxaTheme::getPrimary() & 0x1F) << 3);

                uint16_t rippleColor = canvas.color565(
                    (uint8_t)((1.0f - alpha) * bg_r + alpha * target_r),
                    (uint8_t)((1.0f - alpha) * bg_g + alpha * target_g),
                    (uint8_t)((1.0f - alpha) * bg_b + alpha * target_b)
                );

                canvas.drawCircle((int)cx, (int)cy, (int)rippleR, rippleColor);
                canvas.drawCircle((int)cx, (int)cy, (int)(rippleR - 1.0f), rippleColor); // 2px thick
            }

            // 2. Core mic button (flashes red when recording)
            uint16_t btnColor = isRecording ? 0xE800 : (m_isMicPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getPrimary());
            canvas.fillCircle((int)cx, (int)cy, (int)micR, btnColor);
            canvas.drawCircle((int)cx, (int)cy, (int)micR, VoxaTheme::getTextPrimary());

            // Mic icon inside
            ScreenCommon::drawMicShape(canvas, cx, cy, micR * 0.85f * 2.0f, VoxaTheme::getTextPrimary(), btnColor);

            // 3. Render Status Details & Live Sound Waveforms
            canvas.setFont(&fonts::FreeSansBold12pt7b);
            canvas.setTextColor(VoxaTheme::getTextPrimary());
            canvas.setTextDatum(textdatum_t::middle_center);

            if (isRecording)
            {
                int mins = (int)recordingDuration / 60;
                int secs = (int)recordingDuration % 60;
                char timeStr[32];
                sprintf(timeStr, "%02d:%02d", mins, secs);
                canvas.drawString(timeStr, cx, h * 0.74f);

                // Breathing RECORDING tag
                float blink = std::sin(elapsed * 6.0f) * 0.4f + 0.6f;
                uint16_t recColor = canvas.color565((uint8_t)(blink * 240), 20, 20);
                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextColor(recColor);
                
                // Draw blinking recording dot + text
                canvas.fillCircle((int)(cx - 58.0f), (int)(h * 0.82f), 4, recColor);
                canvas.drawString("RECORDING", cx + 6.0f, h * 0.82f);

                // Render beautiful Siri-style layered sine audio visualizer waves
                float waveCenterY = h * 0.90f;
                float waveHeight = 8.0f;
                int prevY1 = (int)waveCenterY;
                int prevY2 = (int)waveCenterY;

                for (int x = 30; x <= (w - 30); x += 4)
                {
                    float envelope = std::sin((x - 30.0f) / (w - 60.0f) * M_PI); // smoothly pinch edges to 0
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
            else
            {
                canvas.drawString("00:00", cx, h * 0.74f);

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextColor(VoxaTheme::getTextSecondary());
                canvas.drawString("TAP TO RECORD", cx, h * 0.82f);
                
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

        return targetScreen;
    }
}
