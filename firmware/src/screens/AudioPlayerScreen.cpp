#include "AudioPlayerScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/RecordingService.h"
#include "../services/ApiClient.h"
#include "../audio/AudioManager.h"
#include "Transition.h"
#include <cmath>
#include <algorithm>

namespace VOXA
{
    extern RecordingService recordingService;
    extern ApiClient apiClient;

    uint32_t AudioPlayerScreen::s_recordingId = 0;
    ScreenId AudioPlayerScreen::s_backRoute = ScreenId::RecordingsLibrary;

    void AudioPlayerScreen::setRecording(uint32_t id, ScreenId backRoute)
    {
        s_recordingId = id;
        s_backRoute = backRoute;
    }

    ScreenId AudioPlayerScreen::show(Touch& touch)
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
            return s_backRoute;
        }

        ScreenId targetScreen = ScreenId::AudioPlayer;
        uint32_t lastMs = millis();
        float elapsed = 0.0f;

        // Retrieve Recording Details
        Recording rec;
        auto all = recordingService.getAll();
        for (const auto& r : all)
        {
            if (r.id == s_recordingId)
            {
                rec = r;
                break;
            }
        }

        bool isPlaying = false;
        float currentProgressSec = 0.0f;
        
        uint32_t durationSec = rec.durationSeconds;
        if (durationSec == 0)
        {
            durationSec = 180; // default 3 min
        }
        
        bool m_isBackPressed = false;
        bool m_isPlayPressed = false;
        bool m_isRewindPressed = false;
        bool m_isForwardPressed = false;
        bool m_isScrubbing = false;
        bool m_wasTouched = false;

        while (targetScreen == ScreenId::AudioPlayer)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;
            elapsed += deltaSecs;

            // Track Real Audio Playback Status
            bool streamActive = AudioManager::instance().isVoiceStreamPlaying();
            if (isPlaying && !streamActive && currentProgressSec > 1.0f && !m_isScrubbing)
            {
                isPlaying = false;
                currentProgressSec = 0.0f;
            }

            // Increment playback position
            if (isPlaying && !m_isScrubbing)
            {
                currentProgressSec += deltaSecs;
                if (currentProgressSec >= (float)durationSec)
                {
                    currentProgressSec = (float)durationSec;
                    isPlaying = false;
                }
            }

            // ── Touch Processing ───────────────────────────────────────────
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            float progY = 216.0f;
            float barStartX = 16.0f;
            float barEndX = w - 16.0f;
            float barW = barEndX - barStartX;

            if (touched)
            {
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    dragStartX = tx;
                    dragStartY = ty;
                    swipeBackCandidate = (tx < 50);

                    // Back button / Top Bar
                    if (ty <= 40)
                    {
                        m_isBackPressed = true;
                        AudioManager::instance().playTapSoundAsync();
                    }

                    // Progress Seek Bar (Y = 200 to 235)
                    if (ty >= progY - 14.0f && ty <= progY + 16.0f && tx >= barStartX && tx <= barEndX)
                    {
                        m_isScrubbing = true;
                        float pct = (float)(tx - barStartX) / barW;
                        pct = std::max(0.0f, std::min(1.0f, pct));
                        currentProgressSec = pct * durationSec;
                        AudioManager::instance().playTapSoundAsync();
                    }

                    // Rewind button (X: 30..75, Y: 250..295)
                    if (tx >= 30 && tx <= 75 && ty >= 250 && ty <= 295)
                    {
                        m_isRewindPressed = true;
                        AudioManager::instance().playTapSoundAsync();
                    }

                    // Center Play/Pause button (X: 95..145, Y: 250..295)
                    if (tx >= 95 && tx <= 145 && ty >= 250 && ty <= 295)
                    {
                        m_isPlayPressed = true;
                        AudioManager::instance().playTapSoundAsync();
                    }

                    // Forward button (X: 165..210, Y: 250..295)
                    if (tx >= 165 && tx <= 210 && ty >= 250 && ty <= 295)
                    {
                        m_isForwardPressed = true;
                        AudioManager::instance().playTapSoundAsync();
                    }
                }
                else
                {
                    float dx = tx - dragStartX;
                    float dy = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dy) < 40)
                    {
                        targetScreen = s_backRoute;
                        swipeBackCandidate = false;
                    }

                    if (m_isScrubbing)
                    {
                        float pct = (float)(tx - barStartX) / barW;
                        pct = std::max(0.0f, std::min(1.0f, pct));
                        currentProgressSec = pct * durationSec;
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;
                    m_isScrubbing = false;

                    if (m_isBackPressed)
                    {
                        targetScreen = s_backRoute;
                    }
                    else if (m_isRewindPressed)
                    {
                        currentProgressSec = std::max(0.0f, currentProgressSec - 10.0f);
                    }
                    else if (m_isForwardPressed)
                    {
                        currentProgressSec = std::min((float)durationSec, currentProgressSec + 10.0f);
                    }
                    else if (m_isPlayPressed)
                    {
                        isPlaying = !isPlaying;
                        if (isPlaying)
                        {
                            if (!rec.filePath.empty())
                            {
                                AudioManager::instance().playUrlAsync(apiClient.getBaseUrl() + rec.filePath);
                            }
                        }
                        else
                        {
                            AudioManager::instance().stop();
                        }
                    }

                    m_isBackPressed = false;
                    m_isRewindPressed = false;
                    m_isForwardPressed = false;
                    m_isPlayPressed = false;
                }
            }

            // ── RENDERING ───────────────────────────────────────────────────
            canvas.fillScreen(0x0000); // Pure OLED pitch black

            // ── 1. Top Header (Y = 16) ───────────────────────────────
            // "LOSSLESS DAC" on left
            canvas.setFont(&fonts::Font0);
            canvas.setTextDatum(textdatum_t::top_left);
            canvas.setTextColor(canvas.color565(140, 155, 175));
            canvas.drawString("LOSSLESS  DAC", 16.0f, 16.0f);

            // "96kHz" on right in cyan/soft sky blue
            canvas.setTextDatum(textdatum_t::top_right);
            canvas.setTextColor(canvas.color565(115, 185, 235));
            canvas.drawString("96kHz", w - 16.0f, 16.0f);

            // ── 2. Center Waveform Capsule Card (Y = 88) ────────────
            int cardW = 76;
            int cardH = 76;
            int cardX = (w - cardW) / 2;
            int cardY = 88;

            canvas.fillRoundRect(cardX, cardY, cardW, cardH, 16, canvas.color565(26, 30, 42));
            canvas.drawRoundRect(cardX, cardY, cardW, cardH, 16, canvas.color565(38, 44, 58));

            // Symmetrical Waveform Bars centered inside the card
            int cardCx = cardX + cardW / 2;
            int cardCy = cardY + cardH / 2;

            const float barOffsets[5] = { -16.0f, -8.0f, 0.0f, 8.0f, 16.0f };
            const float barBaseH[5]   = { 12.0f,  20.0f,  30.0f, 20.0f, 12.0f };
            const float barW_px       = 3.5f;

            for (int i = 0; i < 5; ++i)
            {
                float bx = cardCx + barOffsets[i];
                float animScale = 1.0f;
                if (isPlaying)
                {
                    float waveAnim = std::sin(elapsed * 10.0f + i * 1.2f);
                    animScale = 0.55f + 0.45f * waveAnim;
                }
                else
                {
                    animScale = 0.65f;
                }

                float bh = std::max(6.0f, barBaseH[i] * animScale);
                float by = cardCy - bh * 0.5f;
                canvas.fillRoundRect((int)(bx - barW_px * 0.5f), (int)by, (int)barW_px, (int)bh, 1, 0xFFFF);
            }

            // ── 3. Filename / Title (Y = 186) ────────────────────────
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.setTextColor(0xFFFF);

            std::string titleStr = rec.title.empty() ? "Interview_Part1.wav" : rec.title;
            // Append .wav if not already having an extension for authentic display
            if (titleStr.find('.') == std::string::npos)
            {
                titleStr += ".wav";
            }
            if (titleStr.length() > 22) titleStr = titleStr.substr(0, 20) + "..";
            canvas.drawString(titleStr.c_str(), w * 0.5f, 186.0f);

            // ── 4. Progress Bar (Y = 216) ────────────────────────────
            canvas.fillRoundRect((int)barStartX, (int)progY, (int)barW, 4, 2, canvas.color565(36, 42, 54));

            float pct = (durationSec > 0) ? (currentProgressSec / durationSec) : 0.0f;
            pct = std::max(0.0f, std::min(1.0f, pct));
            int fillW = (int)(barW * pct);
            if (fillW > 0)
            {
                canvas.fillRoundRect((int)barStartX, (int)progY, fillW, 4, 2, 0xFFFF);
            }

            // ── 5. Bottom Playback Controls (Y = 270) ────────────────
            int ctrlY = 270;

            // Previous Button (|◀) at X = 52
            int prevX = 52;
            uint16_t prevCol = m_isRewindPressed ? canvas.color565(160, 160, 160) : 0xFFFF;
            // Left bar
            canvas.fillRect(prevX - 7, ctrlY - 6, 2, 12, prevCol);
            // Left-pointing triangle
            canvas.fillTriangle(prevX - 4, ctrlY, prevX + 4, ctrlY - 6, prevX + 4, ctrlY + 6, prevCol);

            // Center Play / Pause Button at X = 120
            int playX = 120;
            uint16_t playCol = m_isPlayPressed ? canvas.color565(160, 160, 160) : 0xFFFF;
            if (isPlaying)
            {
                // Pause bars (||)
                canvas.fillRect(playX - 4, ctrlY - 7, 3, 14, playCol);
                canvas.fillRect(playX + 2, ctrlY - 7, 3, 14, playCol);
            }
            else
            {
                // Play triangle (▶ outline / solid)
                canvas.fillTriangle(playX - 5, ctrlY - 8, playX - 5, ctrlY + 8, playX + 7, ctrlY, playCol);
                // Subtle inner cutout if matching exact outline or clean solid
            }

            // Next Button (▶|) at X = 188
            int nextX = 188;
            uint16_t nextCol = m_isForwardPressed ? canvas.color565(160, 160, 160) : 0xFFFF;
            // Right-pointing triangle
            canvas.fillTriangle(nextX - 4, ctrlY - 6, nextX - 4, ctrlY + 6, nextX + 4, ctrlY, nextCol);
            // Right bar
            canvas.fillRect(nextX + 5, ctrlY - 6, 2, 12, nextCol);

            // Screen Slide Transition
            if (entryFrame < 6)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::AudioPlayer), entryFrame, 6);
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
