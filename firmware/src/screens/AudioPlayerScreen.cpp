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
            
            for (size_t i = 0; i <= text.size(); i++)
            {
                char c = (i < text.size()) ? text[i] : '\0';
                if (c == ' ' || c == '\n' || c == '\0')
                {
                    std::string testLine = currentLine.empty() ? word : (currentLine + " " + word);
                    if (canvas.textWidth(testLine.c_str()) > maxW && !currentLine.empty())
                    {
                        canvas.drawString(currentLine.c_str(), x, y);
                        y += canvas.fontHeight() + 2.0f;
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
                        currentLine = "";
                    }
                }
                else
                {
                    word += c;
                }
            }

            if (!currentLine.empty())
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

        // Mock Playback State variables
        bool isPlaying = false;
        float currentProgressSec = 0.0f;
        uint32_t durationSec = rec.durationSeconds > 0 ? rec.durationSeconds : 10;
        
        bool m_isBackPressed = false;
        bool m_isPlayPressed = false;
        bool m_isDeletePressed = false;
        bool m_isScrubbing = false;

        bool m_wasTouched = false;

        // Visualizer bar scale animations
        float vizScales[6] = {0.2f, 0.4f, 0.6f, 0.8f, 0.5f, 0.3f};

        while (targetScreen == ScreenId::AudioPlayer)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            // Increment simulated playback position
            if (isPlaying && !m_isScrubbing)
            {
                currentProgressSec += deltaSecs;
                if (currentProgressSec >= (float)durationSec)
                {
                    currentProgressSec = 0.0f;
                    isPlaying = false;
                }
            }

            // Animate visualizer bars when playing
            if (isPlaying)
            {
                for (int i = 0; i < 6; ++i)
                {
                    float angle = (nowMs * 0.005f) + (i * 1.0f);
                    vizScales[i] = 0.2f + 0.8f * std::abs(std::sin(angle));
                }
            }
            else
            {
                for (int i = 0; i < 6; ++i)
                {
                    vizScales[i] = 0.15f; // Flat line/dormant state
                }
            }

            // Touch Input
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            if (touched && entryFrame >= 10)
            {
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    dragStartX = tx;
                    dragStartY = ty;
                    swipeBackCandidate = (tx < 50);

                    // Back button bounds (Y = 45)
                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isBackPressed = true;
                    }

                    // Play/Pause button bounds (centered at Y = h - 50, radius = 24)
                    float playCx = w * 0.5f;
                    float playCy = h - 50.0f;
                    if (std::sqrt((tx - playCx)*(tx - playCx) + (ty - playCy)*(ty - playCy)) <= 24.0f)
                    {
                        m_isPlayPressed = true;
                    }

                    // Delete button bounds (right side, Y = h - 50, radius = 18)
                    float delCx = w * 0.85f;
                    float delCy = h - 50.0f;
                    if (std::sqrt((tx - delCx)*(tx - delCx) + (ty - delCy)*(ty - delCy)) <= 18.0f)
                    {
                        m_isDeletePressed = true;
                    }

                    // Scrubbing bounds check (Y slider area)
                    float sliderY = h - 110.0f;
                    if (ty >= sliderY - 15.0f && ty <= sliderY + 15.0f && tx >= 20 && tx <= w - 20)
                    {
                        m_isScrubbing = true;
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

                    // Scrubbing update
                    if (m_isScrubbing)
                    {
                        float pct = (float)(tx - 20) / (w - 40);
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
                    else if (m_isPlayPressed)
                    {
                        isPlaying = !isPlaying;
                        Serial.printf("[AudioPlayer] Toggle play: %s (%s)\n", isPlaying ? "PLAYING" : "PAUSED", rec.filePath.c_str());
                        if (isPlaying)
                        {
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
                                Serial.printf("[AudioPlayer] Streaming audio to speaker: %s\n", playUrl.c_str());
                                AudioManager::instance().playUrlAsync(playUrl);
                            }
                        }
                        else
                        {
                            AudioManager::instance().stop();
                        }
                    }
                    else if (m_isDeletePressed)
                    {
                        Serial.printf("[AudioPlayer] Deleting recording memo ID: %u\n", s_recordingId);
                        recordingService.remove(s_recordingId);
                        targetScreen = s_backRoute;
                    }

                    m_isBackPressed = false;
                    m_isPlayPressed = false;
                    m_isDeletePressed = false;
                }
            }

            // Render Layout
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "Now Playing", true, false, Icon::Plus, w, h);

            // Back button
            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, 
                                              backFill, backColor, w, h);

            // Content Area clipping
            canvas.setClipRect(0, 70, w, h - 70);

            // 1. Text Info (Title and details)
            float startY = 78.0f;
            float maxW = w * 0.92f;
            float startX = w * 0.04f;

            canvas.setFont(&fonts::FreeSansBold12pt7b);
            drawWrappedString(canvas, rec.title.empty() ? "Voice Memo" : rec.title, startX, startY, maxW, VoxaTheme::getTextPrimary());
            
            // Format details (Duration, File Size/Timestamp)
            canvas.setFont(&fonts::FreeSans9pt7b);
            canvas.setTextColor(VoxaTheme::getTextSecondary());
            canvas.drawString(rec.timestamp.c_str(), startX, startY + 2.0f);
            
            // 2. Beautiful soundwave visualizer in center
            float vizY = h - 145.0f;
            float vizSpacing = 8.0f;
            float vizW = 4.0f;
            float vizMaxH = 40.0f;
            float totalVizW = 6 * vizSpacing;
            float vizStartX = (w - totalVizW) * 0.5f;

            for (int i = 0; i < 6; ++i)
            {
                float barH = vizMaxH * vizScales[i];
                float barX = vizStartX + i * vizSpacing;
                // Draw rounded vertical bar
                canvas.fillRoundRect((int)barX, (int)(vizY - barH * 0.5f), (int)vizW, (int)barH, 2, VoxaTheme::getPrimary());
            }

            // 3. Track slider / Progress Bar
            float sliderY = h - 110.0f;
            float progressPct = (float)currentProgressSec / durationSec;
            float barStartX = 20.0f;
            float barEndX = w - 20.0f;
            float barW = barEndX - barStartX;
            float thumbX = barStartX + progressPct * barW;

            // Draw track line background
            canvas.fillRect((int)barStartX, (int)(sliderY - 2.0f), (int)barW, 4, VoxaTheme::getDivider());
            // Draw active track line
            canvas.fillRect((int)barStartX, (int)(sliderY - 2.0f), (int)(progressPct * barW), 4, VoxaTheme::getPrimary());
            // Draw thumb circle
            canvas.fillCircle((int)thumbX, (int)sliderY, 5, VoxaTheme::getPrimaryLight());

            // Track Time Text label (Current duration / total duration)
            canvas.setFont(&fonts::FreeSans9pt7b);
            canvas.setTextColor(VoxaTheme::getTextSecondary());
            canvas.setTextDatum(textdatum_t::top_left);
            canvas.drawString(formatTime((uint32_t)currentProgressSec).c_str(), barStartX, sliderY + 8.0f);

            canvas.setTextDatum(textdatum_t::top_right);
            canvas.drawString(formatTime(durationSec).c_str(), barEndX, sliderY + 8.0f);

            // 4. Large center Play/Pause control button
            float playCx = w * 0.5f;
            float playCy = h - 50.0f;
            uint16_t playBg = m_isPlayPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t playColor = m_isPlayPressed ? VoxaTheme::getBackground() : VoxaTheme::getPrimary();
            uint16_t playBorder = m_isPlayPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getDivider();

            canvas.fillCircle((int)playCx, (int)playCy, 24, playBg);
            canvas.drawCircle((int)playCx, (int)playCy, 24, playBorder);
            ScreenCommon::drawIcon(canvas, isPlaying ? Icon::Pause : Icon::Play, playCx - 10.0f, playCy - 10.0f, 20.0f, playColor);

            // 5. Delete Button (trash bin shortcut style)
            float delCx = w * 0.85f;
            float delCy = h - 50.0f;
            uint16_t delBg = m_isDeletePressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t delColor = m_isDeletePressed ? VoxaTheme::getBackground() : VoxaTheme::getWarning();
            uint16_t delBorder = m_isDeletePressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getDivider();

            canvas.fillCircle((int)delCx, (int)delCy, 18, delBg);
            canvas.drawCircle((int)delCx, (int)delCy, 18, delBorder);
            // Draw a basic trash/dustbin representation using lines
            canvas.fillRect((int)(delCx - 5.0f), (int)(delCy - 5.0f), 10, 2, delColor);
            canvas.fillRect((int)(delCx - 4.0f), (int)(delCy - 2.0f), 8, 8, delColor);
            canvas.fillRect((int)(delCx - 2.0f), (int)(delCy - 7.0f), 4, 2, delColor);

            canvas.clearClipRect();

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
