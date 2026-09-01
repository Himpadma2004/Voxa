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

    namespace
    {
        // Spotify Theme Color Palette
        constexpr uint16_t SPOTIFY_BG       = 0x10A2; // #121212
        constexpr uint16_t SPOTIFY_SURFACE  = 0x18E3; // #181818
        constexpr uint16_t SPOTIFY_CARD     = 0x2124; // #242424
        constexpr uint16_t SPOTIFY_GREEN    = 0x1DCB; // #1DB954
        constexpr uint16_t SPOTIFY_GREEN_LO = 0x0BE4; // #0E682E
        constexpr uint16_t SPOTIFY_WHITE    = 0xFFFF; // #FFFFFF
        constexpr uint16_t SPOTIFY_GRAY     = 0xBDF7; // #B3B3B3
        constexpr uint16_t SPOTIFY_DARK_GRAY= 0x4228; // #404040
        constexpr uint16_t SPOTIFY_TRACK    = 0x3186; // #303030

        std::string formatTime(uint32_t seconds)
        {
            uint32_t m = seconds / 60;
            uint32_t s = seconds % 60;
            char buf[16];
            sprintf(buf, "%01u:%02u", m, s);
            return buf;
        }

        void drawWrappedString(LGFX_Sprite& canvas, const std::string& text, float x, float& y, float maxW, uint16_t color)
        {
            canvas.setTextColor(color);
            canvas.setTextDatum(textdatum_t::top_left);

            std::string currentLine = "";
            std::string word = "";
            int linesDrawn = 0;
            
            for (size_t i = 0; i <= text.size(); i++)
            {
                char c = (i < text.size()) ? text[i] : '\0';
                if (c == ' ' || c == '\n' || c == '\0')
                {
                    std::string testLine = currentLine.empty() ? word : (currentLine + " " + word);
                    if (canvas.textWidth(testLine.c_str()) > maxW && !currentLine.empty())
                    {
                        if (linesDrawn >= 2)
                        {
                            currentLine += "...";
                            canvas.drawString(currentLine.c_str(), x, y);
                            y += canvas.fontHeight() + 2.0f;
                            return;
                        }
                        canvas.drawString(currentLine.c_str(), x, y);
                        y += canvas.fontHeight() + 2.0f;
                        linesDrawn++;
                        currentLine = word;
                    }
                    else
                    {
                        currentLine = testLine;
                    }
                    word = "";
                    if (c == '\n')
                    {
                        canvas.drawString(currentLine.c_str(), x, y);
                        y += canvas.fontHeight() + 2.0f;
                        linesDrawn++;
                        currentLine = "";
                    }
                }
                else
                {
                    word += c;
                }
            }

            if (!currentLine.empty() && linesDrawn < 2)
            {
                canvas.drawString(currentLine.c_str(), x, y);
                y += canvas.fontHeight() + 2.0f;
            }
        }
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

        // Dynamic Playback State
        bool isPlaying = false;
        float currentProgressSec = 0.0f;
        
        // Accurate Duration: use recorded duration or dynamic fallback
        uint32_t durationSec = rec.durationSeconds;
        if (durationSec == 0)
        {
            durationSec = 8;
        }
        
        bool m_isBackPressed = false;
        bool m_isPlayPressed = false;
        bool m_isRewindPressed = false;
        bool m_isForwardPressed = false;
        bool m_isDeletePressed = false;
        bool m_isScrubbing = false;
        bool m_wasTouched = false;

        // Visualizer 12-band equalizer
        float vizScales[12] = {0.2f, 0.4f, 0.6f, 0.8f, 0.5f, 0.7f, 0.9f, 0.6f, 0.4f, 0.8f, 0.5f, 0.3f};

        while (targetScreen == ScreenId::AudioPlayer)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            // Track Real Audio Playback Status
            bool streamActive = AudioManager::instance().isVoiceStreamPlaying();
            if (isPlaying && !streamActive && currentProgressSec > 1.0f && !m_isScrubbing)
            {
                isPlaying = false;
                currentProgressSec = 0.0f;
            }

            // Increment simulated/actual playback position
            if (isPlaying && !m_isScrubbing)
            {
                currentProgressSec += deltaSecs;
                if (currentProgressSec >= (float)durationSec)
                {
                    currentProgressSec = (float)durationSec;
                    isPlaying = false;
                }
            }

            // Animate visualizer bars when playing
            if (isPlaying)
            {
                for (int i = 0; i < 12; ++i)
                {
                    float angle = (nowMs * 0.008f) + (i * 0.75f);
                    vizScales[i] = 0.25f + 0.75f * std::abs(std::sin(angle));
                }
            }
            else
            {
                for (int i = 0; i < 12; ++i)
                {
                    vizScales[i] = 0.15f; // Resting low bar
                }
            }

            // ── Touch Processing ───────────────────────────────────────────
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            float sliderY = h - 88.0f;
            float barStartX = 24.0f;
            float barEndX = w - 24.0f;
            float barW = barEndX - barStartX;

            if (touched && entryFrame >= 10)
            {
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    dragStartX = tx;
                    dragStartY = ty;
                    swipeBackCandidate = (tx < 50);

                    // Back button (Top Left)
                    if (std::sqrt((tx - 24.0f)*(tx - 24.0f) + (ty - 38.0f)*(ty - 38.0f)) <= 22.0f)
                    {
                        m_isBackPressed = true;
                    }

                    // Delete button (Top Right)
                    if (std::sqrt((tx - (w - 24.0f))*(tx - (w - 24.0f)) + (ty - 38.0f)*(ty - 38.0f)) <= 22.0f)
                    {
                        m_isDeletePressed = true;
                    }

                    // Responsive Scrub Bar (Generous touch hit area +-20px)
                    if (ty >= sliderY - 20.0f && ty <= sliderY + 20.0f && tx >= (barStartX - 10.0f) && tx <= (barEndX + 10.0f))
                    {
                        m_isScrubbing = true;
                        float pct = (float)(tx - barStartX) / barW;
                        pct = std::max(0.0f, std::min(1.0f, pct));
                        currentProgressSec = pct * durationSec;
                    }

                    // Center Play/Pause button (Y = h - 38, radius = 28)
                    float playCx = w * 0.5f;
                    float playCy = h - 38.0f;
                    if (std::sqrt((tx - playCx)*(tx - playCx) + (ty - playCy)*(ty - playCy)) <= 28.0f)
                    {
                        m_isPlayPressed = true;
                    }

                    // Rewind -5s button
                    float rwdCx = w * 0.22f;
                    float rwdCy = h - 38.0f;
                    if (std::sqrt((tx - rwdCx)*(tx - rwdCx) + (ty - rwdCy)*(ty - rwdCy)) <= 22.0f)
                    {
                        m_isRewindPressed = true;
                    }

                    // Forward +5s button
                    float fwdCx = w * 0.78f;
                    float fwdCy = h - 38.0f;
                    if (std::sqrt((tx - fwdCx)*(tx - fwdCx) + (ty - fwdCy)*(ty - fwdCy)) <= 22.0f)
                    {
                        m_isForwardPressed = true;
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

                    // Continuous Scrubbing drag
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
                    else if (m_isDeletePressed)
                    {
                        Serial.printf("[AudioPlayer] Deleting recording ID: %u\n", s_recordingId);
                        recordingService.remove(s_recordingId);
                        targetScreen = s_backRoute;
                    }
                    else if (m_isRewindPressed)
                    {
                        currentProgressSec = std::max(0.0f, currentProgressSec - 5.0f);
                    }
                    else if (m_isForwardPressed)
                    {
                        currentProgressSec = std::min((float)durationSec, currentProgressSec + 5.0f);
                    }
                    else if (m_isPlayPressed)
                    {
                        isPlaying = !isPlaying;
                        Serial.printf("[AudioPlayer] Spotify toggle play: %s (%s)\n", 
                                      isPlaying ? "PLAYING" : "PAUSED", rec.filePath.c_str());
                        if (isPlaying)
                        {
                            if (currentProgressSec >= (float)durationSec)
                            {
                                currentProgressSec = 0.0f;
                            }
                            std::string playUrl = rec.filePath;
                            if (!playUrl.empty())
                            {
                                if (playUrl.rfind("http://", 0) != 0 && playUrl.rfind("https://", 0) != 0)
                                {
                                    if (playUrl.find("/api/") == std::string::npos)
                                    {
                                        playUrl = VOXA::apiClient.getBaseUrl() + "/api/audio/" + playUrl;
                                    }
                                    else
                                    {
                                        playUrl = VOXA::apiClient.getBaseUrl() + playUrl;
                                    }
                                }
                                Serial.printf("[AudioPlayer] Streaming high-def audio: %s\n", playUrl.c_str());
                                AudioManager::instance().playUrlAsync(playUrl);
                            }
                        }
                        else
                        {
                            AudioManager::instance().stop();
                        }
                    }

                    m_isBackPressed = false;
                    m_isPlayPressed = false;
                    m_isRewindPressed = false;
                    m_isForwardPressed = false;
                    m_isDeletePressed = false;
                }
            }

            // ── Spotify Dark UI Render ─────────────────────────────────────
            canvas.fillScreen(SPOTIFY_BG);

            // 1. Header Bar: Chevron Back, "NOW PLAYING", Delete Icon
            // Back button
            canvas.fillCircle(24, 38, 16, m_isBackPressed ? SPOTIFY_CARD : SPOTIFY_SURFACE);
            canvas.drawCircle(24, 38, 16, SPOTIFY_DARK_GRAY);
            // Draw clean '<' back arrow
            canvas.drawLine(26, 32, 21, 38, SPOTIFY_WHITE);
            canvas.drawLine(21, 38, 26, 44, SPOTIFY_WHITE);

            // Title: "PLAYING FROM VOXA"
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextColor(SPOTIFY_GRAY);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.drawString("PLAYING FROM VOXA", w * 0.5f, 38);

            // Delete trash button (Top Right)
            canvas.fillCircle(w - 24, 38, 16, m_isDeletePressed ? 0xB800 : SPOTIFY_SURFACE);
            canvas.drawCircle(w - 24, 38, 16, SPOTIFY_DARK_GRAY);
            // Draw minimalist trash can
            int delX = w - 24;
            canvas.drawLine(delX - 5, 34, delX + 5, 34, SPOTIFY_GRAY);
            canvas.drawLine(delX - 3, 34, delX - 3, 43, SPOTIFY_GRAY);
            canvas.drawLine(delX + 3, 34, delX + 3, 43, SPOTIFY_GRAY);
            canvas.drawLine(delX - 3, 43, delX + 3, 43, SPOTIFY_GRAY);

            // 2. Spotify Album Art / Vinyl Waveform Card
            float cardX = 24.0f;
            float cardY = 62.0f;
            float cardW = w - 48.0f;
            float cardH = 96.0f;

            // Glassmorphism Card
            canvas.fillRoundRect((int)cardX, (int)cardY, (int)cardW, (int)cardH, 12, SPOTIFY_CARD);
            canvas.drawRoundRect((int)cardX, (int)cardY, (int)cardW, (int)cardH, 12, SPOTIFY_DARK_GRAY);

            // Center Equalizer Waveform Soundbar (12 neon green dynamic bars)
            float vizSpacing = 12.0f;
            float vizBarW = 5.0f;
            float vizMaxH = 48.0f;
            float totalVizW = 12 * vizSpacing;
            float vizStartX = (w - totalVizW) * 0.5f + 2.0f;
            float vizCenterY = cardY + cardH * 0.5f;

            for (int i = 0; i < 12; ++i)
            {
                float barH = vizMaxH * vizScales[i];
                float barX = vizStartX + i * vizSpacing;
                uint16_t barColor = isPlaying ? SPOTIFY_GREEN : SPOTIFY_DARK_GRAY;
                canvas.fillRoundRect((int)barX, (int)(vizCenterY - barH * 0.5f), (int)vizBarW, (int)barH, 2, barColor);
            }

            // Spotify Live Indicator badge in card top-left
            canvas.fillRoundRect((int)(cardX + 10), (int)(cardY + 8), 44, 14, 7, isPlaying ? SPOTIFY_GREEN_LO : SPOTIFY_SURFACE);
            canvas.setFont(&fonts::TomThumb);
            canvas.setTextColor(isPlaying ? SPOTIFY_GREEN : SPOTIFY_GRAY);
            canvas.setTextDatum(textdatum_t::middle_center);
            canvas.drawString(isPlaying ? "LIVE" : "VOICE", cardX + 32, cardY + 15);

            // 3. Track Title & Artist Metadata
            float metaY = cardY + cardH + 12.0f;
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            drawWrappedString(canvas, rec.title.empty() ? "Voice Recording" : rec.title, 24.0f, metaY, w - 48.0f, SPOTIFY_WHITE);

            // Date / Status subtext
            canvas.setFont(&fonts::FreeSans9pt7b);
            canvas.setTextColor(SPOTIFY_GRAY);
            canvas.setTextDatum(textdatum_t::top_left);
            std::string subtext = rec.timestamp.empty() ? "Recorded on VOXA" : rec.timestamp;
            canvas.drawString(subtext.c_str(), 24.0f, metaY + 2.0f);

            // 4. Spotify Progress Scrub Bar (Clean, thin, responsive)
            float progressPct = durationSec > 0 ? ((float)currentProgressSec / durationSec) : 0.0f;
            progressPct = std::max(0.0f, std::min(1.0f, progressPct));
            float thumbX = barStartX + progressPct * barW;

            // Inactive track line (dark gray)
            canvas.fillRoundRect((int)barStartX, (int)(sliderY - 2.0f), (int)barW, 4, 2, SPOTIFY_TRACK);
            // Active track line (Spotify Neon Green)
            if (progressPct > 0.001f)
            {
                canvas.fillRoundRect((int)barStartX, (int)(sliderY - 2.0f), (int)(progressPct * barW), 4, 2, SPOTIFY_GREEN);
            }
            // Scrubber Thumb (Clean white circle with green center)
            canvas.fillCircle((int)thumbX, (int)sliderY, m_isScrubbing ? 7 : 5, SPOTIFY_WHITE);
            canvas.fillCircle((int)thumbX, (int)sliderY, m_isScrubbing ? 3 : 2, SPOTIFY_GREEN);

            // Time Labels (Spotify style: 0:14 on left, 1:42 on right)
            canvas.setFont(&fonts::TomThumb);
            canvas.setTextColor(SPOTIFY_GRAY);
            canvas.setTextDatum(textdatum_t::top_left);
            canvas.drawString(formatTime((uint32_t)currentProgressSec).c_str(), barStartX, sliderY + 7.0f);

            canvas.setTextDatum(textdatum_t::top_right);
            canvas.drawString(formatTime(durationSec).c_str(), barEndX, sliderY + 7.0f);

            // 5. Spotify Bottom Media Controls (Rewind, Play/Pause, Forward)
            // Rewind -5s button
            float rwdCx = w * 0.22f;
            float rwdCy = h - 38.0f;
            canvas.fillCircle((int)rwdCx, (int)rwdCy, 18, m_isRewindPressed ? SPOTIFY_CARD : SPOTIFY_SURFACE);
            canvas.drawCircle((int)rwdCx, (int)rwdCy, 18, SPOTIFY_DARK_GRAY);
            // Draw << icon
            canvas.drawLine(rwdCx + 2, rwdCy - 5, rwdCx - 4, rwdCy, SPOTIFY_WHITE);
            canvas.drawLine(rwdCx - 4, rwdCy, rwdCx + 2, rwdCy + 5, SPOTIFY_WHITE);
            canvas.drawLine(rwdCx + 6, rwdCy - 5, rwdCx, rwdCy, SPOTIFY_WHITE);
            canvas.drawLine(rwdCx, rwdCy, rwdCx + 6, rwdCy + 5, SPOTIFY_WHITE);

            // Large Circular Spotify Play/Pause Button (Neon Green)
            float playCx = w * 0.5f;
            float playCy = h - 38.0f;
            uint16_t playBg = m_isPlayPressed ? SPOTIFY_WHITE : SPOTIFY_GREEN;
            canvas.fillCircle((int)playCx, (int)playCy, 24, playBg);
            
            // Black Play or Pause icon
            if (isPlaying)
            {
                // Pause double bars
                canvas.fillRect((int)(playCx - 6), (int)(playCy - 7), 4, 14, 0x0000);
                canvas.fillRect((int)(playCx + 2), (int)(playCy - 7), 4, 14, 0x0000);
            }
            else
            {
                // Play triangle
                canvas.fillTriangle((int)(playCx - 4), (int)(playCy - 8),
                                    (int)(playCx - 4), (int)(playCy + 8),
                                    (int)(playCx + 7), (int)(playCy), 0x0000);
            }

            // Forward +5s button
            float fwdCx = w * 0.78f;
            float fwdCy = h - 38.0f;
            canvas.fillCircle((int)fwdCx, (int)fwdCy, 18, m_isForwardPressed ? SPOTIFY_CARD : SPOTIFY_SURFACE);
            canvas.drawCircle((int)fwdCx, (int)fwdCy, 18, SPOTIFY_DARK_GRAY);
            // Draw >> icon
            canvas.drawLine(fwdCx - 2, fwdCy - 5, fwdCx + 4, fwdCy, SPOTIFY_WHITE);
            canvas.drawLine(fwdCx + 4, fwdCy, fwdCx - 2, fwdCy + 5, SPOTIFY_WHITE);
            canvas.drawLine(fwdCx - 6, fwdCy - 5, fwdCx, fwdCy, SPOTIFY_WHITE);
            canvas.drawLine(fwdCx, fwdCy, fwdCx - 6, fwdCy + 5, SPOTIFY_WHITE);

            // Screen Slide Transition
            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::AudioPlayer), entryFrame, 10);
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

        AudioManager::instance().stop();
        canvas.deleteSprite();
        return targetScreen;
    }
}
