#include "SearchScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../services/SearchService.h"
#include "../services/ApiClient.h"
#include "../services/MicrophoneService.h"
#include "../services/WiFiManager.h"
#include "TextInputScreen.h"
#include "DetailScreen.h"
#include "Transition.h"
#include <cmath>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace
{
    volatile bool s_aiSearchDone = false;
    volatile bool s_aiSearchOk = false;
    char s_aiSearchQuery[256] = {};
    char s_aiSearchAnswer[512] = {};
    char s_aiSearchError[128] = {};
    std::string s_aiSearchPath;
    std::string s_aiSearchTextQuery;

    void aiAudioSearchTaskFn(void*)
    {
        VOXA::AiSearchResult res = VOXA::apiClient.searchAiAudio(s_aiSearchPath);
        s_aiSearchOk = res.success;
        strncpy(s_aiSearchQuery, res.query.c_str(), 255);
        strncpy(s_aiSearchAnswer, res.answer.c_str(), 511);
        strncpy(s_aiSearchError, res.error.c_str(), 127);
        s_aiSearchDone = true;
        vTaskDelete(nullptr);
    }

    void aiTextSearchTaskFn(void*)
    {
        VOXA::AiSearchResult res = VOXA::apiClient.searchAi(s_aiSearchTextQuery);
        s_aiSearchOk = res.success;
        strncpy(s_aiSearchQuery, res.query.c_str(), 255);
        strncpy(s_aiSearchAnswer, res.answer.c_str(), 511);
        strncpy(s_aiSearchError, res.error.c_str(), 127);
        s_aiSearchDone = true;
        vTaskDelete(nullptr);
    }
}

namespace VOXA
{
    extern SearchService searchService;

    ScreenId SearchScreen::show(Touch& touch)
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
            return ScreenId::Home;
        }

        ScreenId targetScreen = ScreenId::Search;
        uint32_t lastMs = millis();

        // Check if returning from TextInputScreen with typed search result
        std::string typedText = TextInputScreen::getResult();
        if (!typedText.empty())
        {
            m_lastQuery = typedText;
            m_state = AiSearchState::Searching;
            s_aiSearchTextQuery = typedText;
            s_aiSearchDone = false;
            s_aiSearchOk = false;
            xTaskCreate(aiTextSearchTaskFn, "AiTextSearch", 8192, nullptr, 1, nullptr);
            Serial.printf("[SearchScreen] Started text search for: %s\n", typedText.c_str());
        }

        // OPTIMIZATION: Fetch recent search items ONCE at screen start (prevents 60FPS disk lag)
        auto searchItems = searchService.getRecent(10);
        float visibleHeight = h - 70.0f - 18.0f;
        float contentHeight = 130.0f + (m_lastAnswer.empty() ? 0.0f : 70.0f) + searchItems.size() * 50.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        while (targetScreen == ScreenId::Search)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            // Check async search completion
            if (m_state == AiSearchState::Searching && s_aiSearchDone)
            {
                s_aiSearchDone = false;
                if (s_aiSearchOk)
                {
                    m_state = AiSearchState::HasResult;
                    m_lastQuery = s_aiSearchQuery;
                    m_lastAnswer = s_aiSearchAnswer;
                    Serial.printf("[SearchScreen] AI Search Success! Answer: %s\n", m_lastAnswer.c_str());
                }
                else
                {
                    m_state = AiSearchState::HasResult;
                    m_lastAnswer = (strlen(s_aiSearchError) > 0) ? s_aiSearchError : "Search failed. Check Wi-Fi connection.";
                    Serial.printf("[SearchScreen] AI Search Failed: %s\n", m_lastAnswer.c_str());
                }
            }

            contentHeight = 130.0f + (m_lastAnswer.empty() ? 0.0f : 70.0f) + (m_state == AiSearchState::RecordingVoice ? 50.0f : 0.0f) + searchItems.size() * 50.0f;
            maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

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
                    m_dragStartY = ty;
                    m_dragStartScrollY = m_targetScrollY;
                    m_lastTouchSampleMs = nowMs;
                    m_isDragging = false;
                    m_scrollVelocity = 0.0f;

                    // Header Back button bounds Y = 45
                    if (std::sqrt((tx - 20.0f)*(tx - 20.0f) + (ty - 45.0f)*(ty - 45.0f)) <= 18.0f)
                    {
                        m_isBackPressed = true;
                    }

                    // Voice & Type Search Buttons
                    float leftCardX = w * 0.04f;
                    float rightCardX = w * 0.52f;
                    float cardWidth = w * 0.44f;
                    float topBtnY = 72.0f - m_scrollY;

                    if (ty >= topBtnY && ty <= (topBtnY + 38.0f))
                    {
                        if (tx >= leftCardX && tx <= (leftCardX + cardWidth))
                        {
                            m_isVoiceSearchPressed = true;
                        }
                        else if (tx >= rightCardX && tx <= (rightCardX + cardWidth))
                        {
                            m_isTypeSearchPressed = true;
                        }
                    }

                    // Recent Item Cards
                    float itemsStartY = 125.0f + (m_lastAnswer.empty() ? 0.0f : 70.0f) + (m_state == AiSearchState::RecordingVoice ? 50.0f : 0.0f);
                    if (ty >= 70.0f && ty <= (h - 18.0f))
                    {
                        float leftX = w * 0.04f;
                        float cardW = w * 0.92f;
                        for (std::size_t i = 0; i < searchItems.size(); ++i)
                        {
                            float itemY = itemsStartY + i * 50.0f - m_scrollY;
                            if (tx >= leftX && tx <= (leftX + cardW) &&
                                ty >= itemY && ty <= (itemY + 44.0f))
                            {
                                m_pressedItemIndex = i;
                            }
                        }
                    }
                }
                else
                {
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        targetScreen = ScreenId::Home;
                        swipeBackCandidate = false;
                    }

                    float dy = ty - m_dragStartY;
                    if (!m_isDragging && std::abs(dy) > 10.0f)
                    {
                        m_isDragging = true;
                        m_isBackPressed = false;
                        m_isVoiceSearchPressed = false;
                        m_isTypeSearchPressed = false;
                        m_pressedItemIndex = -1;
                    }

                    if (m_isDragging)
                    {
                        m_targetScrollY = m_dragStartScrollY - dy;
                        m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));

                        uint32_t dt = nowMs - m_lastTouchSampleMs;
                        if (dt > 0)
                        {
                            m_scrollVelocity = -dy / (dt / 1000.0f);
                        }
                        m_lastTouchSampleMs = nowMs;
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;

                    if (m_isDragging)
                    {
                        m_isDragging = false;
                    }
                    else
                    {
                        if (m_isBackPressed)
                        {
                            targetScreen = ScreenId::Home;
                        }
                        else if (m_isVoiceSearchPressed)
                        {
                            if (m_state == AiSearchState::RecordingVoice)
                            {
                                static uint32_t s_querySeq = 0;
                                s_querySeq++;
                                char queryPath[64];
                                snprintf(queryPath, sizeof(queryPath), "/search_q_%u_%u.wav", (unsigned int)millis(), (unsigned int)s_querySeq);

                                bool stopOk = microphoneService.stopRecording("SearchScreen::voiceSearch", "stop");
                                if (stopOk)
                                {
                                    m_state = AiSearchState::Searching;
                                    s_aiSearchPath = queryPath;
                                    s_aiSearchDone = false;
                                    s_aiSearchOk = false;
                                    xTaskCreate(aiAudioSearchTaskFn, "AiAudioSearch", 8192, nullptr, 1, nullptr);
                                    Serial.println("[SearchScreen] Voice query recorded — analyzing AI search");
                                }
                                else
                                {
                                    m_state = AiSearchState::Idle;
                                }
                            }
                            else
                            {
                                static uint32_t s_querySeq = 0;
                                s_querySeq++;
                                char queryPath[64];
                                snprintf(queryPath, sizeof(queryPath), "/search_q_%u_%u.wav", (unsigned int)millis(), (unsigned int)s_querySeq);

                                bool startOk = microphoneService.startRecording(queryPath, "SearchScreen::voiceSearch");
                                if (startOk)
                                {
                                    m_state = AiSearchState::RecordingVoice;
                                    Serial.println("[SearchScreen] Voice recording query started");
                                }
                            }
                        }
                        else if (m_isTypeSearchPressed)
                        {
                            TextInputScreen::prepare("Ask Voxa DB (e.g. when to call Jennifer)", ScreenId::Search, false);
                            targetScreen = ScreenId::TextInput;
                        }
                        else if (m_pressedItemIndex >= 0 && m_pressedItemIndex < (int)searchItems.size())
                        {
                            std::string cat = searchItems[m_pressedItemIndex].category;
                            uint32_t srcId = searchItems[m_pressedItemIndex].sourceId;
                            std::string catPlural = cat;
                            if (cat == "reminder") catPlural = "reminders";
                            else if (cat == "idea") catPlural = "ideas";
                            else if (cat == "question") catPlural = "questions";

                            if (cat == "reminder" || cat == "idea" || cat == "question")
                            {
                                DetailScreen::setItem(catPlural, srcId, ScreenId::Search);
                                targetScreen = ScreenId::Detail;
                            }
                        }
                    }
                    m_isBackPressed = false;
                    m_isVoiceSearchPressed = false;
                    m_isTypeSearchPressed = false;
                    m_pressedItemIndex = -1;
                }
            }

            // Dimensions re-query & scroll calculations
            w = Display::width();
            h = Display::height();
            visibleHeight = h - 70.0f - 18.0f;
            maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            if (!m_wasTouched && std::abs(m_scrollVelocity) > 0.0f)
            {
                m_targetScrollY += m_scrollVelocity * deltaSecs;
                m_scrollVelocity *= std::pow(0.85f, deltaSecs * 60.0f);
                if (std::abs(m_scrollVelocity) < 5.0f)
                {
                    m_scrollVelocity = 0.0f;
                }
            }

            m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));
            m_scrollY += (m_targetScrollY - m_scrollY) * 15.0f * deltaSecs;
            if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
            {
                m_scrollY = m_targetScrollY;
            }

            // Render Search Screen
            ScreenCommon::renderSurface(canvas, w, h);
            ScreenCommon::renderHeader(canvas, "AI Memory Search", true, false, Icon::Plus, w, h);

            uint16_t backFill = m_isBackPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
            uint16_t backColor = m_isBackPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
            ScreenCommon::renderCircularButton(canvas, 20.0f, 45.0f, Icon::Back, backFill, backColor, w, h);

            canvas.setClipRect(0, 70, w, h - 70 - 18);

            // 1. Voice Search & Type Search Top Action Cards
            float leftCardX = w * 0.04f;
            float rightCardX = w * 0.52f;
            float cardWidth = w * 0.44f;
            float topBtnY = 72.0f - m_scrollY;

            if (topBtnY + 38.0f >= 70.0f && topBtnY <= (h - 18.0f))
            {
                bool isMicRec = (m_state == AiSearchState::RecordingVoice);
                uint16_t voiceBg = isMicRec ? canvas.color565(220, 40, 40) : (m_isVoiceSearchPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface());
                uint16_t voiceBorder = isMicRec ? canvas.color565(255, 120, 120) : VoxaTheme::getDivider();
                uint16_t voiceText = isMicRec ? TFT_WHITE : VoxaTheme::getTextPrimary();

                canvas.fillRoundRect((int)leftCardX, (int)topBtnY, (int)cardWidth, 38, 8, voiceBg);
                canvas.drawRoundRect((int)leftCardX, (int)topBtnY, (int)cardWidth, 38, 8, voiceBorder);

                ScreenCommon::drawMicShape(canvas, leftCardX + 16.0f, topBtnY + 19.0f, 18.0f, voiceText, voiceBg);
                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextDatum(textdatum_t::middle_left);
                canvas.setTextColor(voiceText);
                canvas.drawString(isMicRec ? "Stop & Search" : "Voice Search", leftCardX + 32.0f, topBtnY + 19.0f);

                uint16_t typeBg = m_isTypeSearchPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                uint16_t typeText = m_isTypeSearchPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
                canvas.fillRoundRect((int)rightCardX, (int)topBtnY, (int)cardWidth, 38, 8, typeBg);
                canvas.drawRoundRect((int)rightCardX, (int)topBtnY, (int)cardWidth, 38, 8, VoxaTheme::getDivider());

                ScreenCommon::drawIcon(canvas, Icon::Search, rightCardX + 14.0f, topBtnY + 12.0f, 14.0f, typeText);
                canvas.drawString("Type Search", rightCardX + 32.0f, topBtnY + 19.0f);
            }

            // 2. Animated Voice Recording Banner or AI Searching Status or Answer Card
            float resultY = topBtnY + 46.0f;
            if (m_state == AiSearchState::RecordingVoice)
            {
                // Animated live mic visualizer card
                canvas.fillRoundRect((int)leftCardX, (int)resultY, (int)(w * 0.92f), 55, 8, canvas.color565(35, 10, 15));
                canvas.drawRoundRect((int)leftCardX, (int)resultY, (int)(w * 0.92f), 55, 8, canvas.color565(220, 50, 50));

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(canvas.color565(255, 120, 120));
                canvas.drawString("Listening... Speak your question!", w * 0.5f, resultY + 18.0f);

                // Draw pulsating audio wave
                float waveCy = resultY + 38.0f;
                float pulseSec = (nowMs % 1000) / 1000.0f;
                for (int bar = 0; bar < 12; ++bar)
                {
                    float barX = w * 0.30f + bar * 12.0f;
                    float hFactor = std::abs(std::sin(pulseSec * 6.28f + bar * 0.5f));
                    float barH = 4.0f + hFactor * 14.0f;
                    canvas.fillRoundRect((int)barX, (int)(waveCy - barH * 0.5f), 4, (int)barH, 2, canvas.color565(255, 80, 80));
                }
            }
            else if (m_state == AiSearchState::Searching)
            {
                canvas.fillRoundRect((int)leftCardX, (int)resultY, (int)(w * 0.92f), 55, 8, VoxaTheme::getSurface());
                canvas.drawRoundRect((int)leftCardX, (int)resultY, (int)(w * 0.92f), 55, 8, VoxaTheme::getPrimary());

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(VoxaTheme::getPrimaryLight());
                canvas.drawString("Searching Database & AI Memories...", w * 0.5f, resultY + 28.0f);
            }
            else if (!m_lastAnswer.empty())
            {
                canvas.fillRoundRect((int)leftCardX, (int)resultY, (int)(w * 0.92f), 64, 8, VoxaTheme::getSurface());
                canvas.drawRoundRect((int)leftCardX, (int)resultY, (int)(w * 0.92f), 64, 8, VoxaTheme::getPrimaryLight());

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(VoxaTheme::getPrimaryLight());
                std::string qDisp = "Q: " + (m_lastQuery.empty() ? "Voice Search" : m_lastQuery);
                if (qDisp.length() > 30) qDisp = qDisp.substr(0, 27) + "...";
                canvas.drawString(qDisp.c_str(), leftCardX + 10.0f, resultY + 8.0f);

                canvas.setTextColor(VoxaTheme::getTextPrimary());
                std::string aDisp = m_lastAnswer;
                if (aDisp.length() > 38) aDisp = aDisp.substr(0, 35) + "...";
                canvas.drawString(aDisp.c_str(), leftCardX + 10.0f, resultY + 34.0f);
            }

            // 3. Recent Items Header & List
            float itemsStartY = 125.0f + (m_lastAnswer.empty() ? 0.0f : 70.0f) + (m_state == AiSearchState::RecordingVoice ? 50.0f : 0.0f) - m_scrollY;
            if (itemsStartY + 15.0f >= 70.0f && itemsStartY <= (h - 18.0f))
            {
                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::middle_left);
                canvas.setTextColor(VoxaTheme::getTextSecondary());
                canvas.drawString("Recent Activities", leftCardX, itemsStartY - 10.0f);
            }

            float listStartY = itemsStartY + 10.0f;
            for (std::size_t i = 0; i < searchItems.size(); ++i)
            {
                float itemY = listStartY + i * 50.0f;
                if (itemY + 44.0f < 70.0f || itemY > (h - 18.0f))
                    continue;

                bool isPressed = (m_pressedItemIndex == (int)i);
                uint16_t cardBg = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                uint16_t cardBorder = isPressed ? VoxaTheme::getPrimaryLight() : VoxaTheme::getDivider();
                uint16_t labelColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextPrimary();
                uint16_t subColor = isPressed ? VoxaTheme::getBackground() : VoxaTheme::getTextSecondary();

                canvas.fillRoundRect((int)leftCardX, (int)itemY, (int)(w * 0.92f), 44, 8, cardBg);
                canvas.drawRoundRect((int)leftCardX, (int)itemY, (int)(w * 0.92f), 44, 8, cardBorder);

                float cy = itemY + 22.0f;
                float iconCx = leftCardX + 22.0f;

                Icon itemIcon = Icon::Folder;
                uint16_t itemColor = 0x52AA;
                std::string cat = searchItems[i].category;
                if (cat == "reminder") { itemIcon = Icon::Bell; itemColor = 0x79CF; }
                else if (cat == "idea") { itemIcon = Icon::Lightbulb; itemColor = 0xFD20; }
                else if (cat == "question") { itemIcon = Icon::Question; itemColor = 0x067F; }

                canvas.fillCircle((int)iconCx, (int)cy, 12, itemColor);
                ScreenCommon::drawIcon(canvas, itemIcon, iconCx - 6.0f, cy - 6.0f, 12.0f, VoxaTheme::getBackground());

                canvas.setFont(&fonts::FreeSans9pt7b);
                canvas.setTextDatum(textdatum_t::middle_left);
                canvas.setTextColor(labelColor);
                canvas.drawString(searchItems[i].title.c_str(), leftCardX + 42.0f, cy - 8.0f);

                canvas.setTextColor(subColor);
                std::string subStr = cat + "  |  " + searchItems[i].timestamp;
                canvas.drawString(subStr.c_str(), leftCardX + 42.0f, cy + 8.0f);

                float chevX = leftCardX + (w * 0.92f) - 16.0f;
                ScreenCommon::drawIcon(canvas, Icon::ChevronRight, chevX - 5.0f, cy - 5.0f, 10.0f, subColor);
            }

            canvas.clearClipRect();
            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Search), entryFrame, 10);
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
