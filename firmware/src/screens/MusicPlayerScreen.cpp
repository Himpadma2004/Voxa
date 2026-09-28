#include "MusicPlayerScreen.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "Transition.h"
#include "../services/ApiClient.h"
#include "../audio/AudioManager.h"
#include "../services/PowerManager.h"
#include "../services/ButtonService.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cmath>
#include <algorithm>

namespace VOXA
{
    static const char* SEARCH_CHIPS[] = {
        "Coldplay", "Adele", "Ed Sheeran", "The Weeknd",
        "Imagine Dragons", "Arijit Singh", "Lo-Fi Beats", "Taylor Swift"
    };
    static const size_t NUM_SEARCH_CHIPS = 8;

    MusicPlayerScreen::MusicPlayerScreen()
        : m_currentTab(MusicTab::All)
        , m_isSearching(false)
        , m_currentTrackIndex(-1)
        , m_isPlaying(false)
        , m_showNowPlayingSheet(false)
        , m_scrollY(0.0f)
        , m_targetScrollY(0.0f)
        , m_scrollVelocity(0.0f)
        , m_dragStartY(0.0f)
        , m_lastDragY(0.0f)
        , m_isDragging(false)
        , m_wasTouched(false)
        , m_lastTouchSampleMs(0)
        , m_trackStartMs(0)
        , m_trackDurationMs(240000)
        , m_animTime(0.0f)
        , m_pressedItemIndex(-1)
        , m_pressedTabIdx(-1)
        , m_pressedSearchChipIdx(-1)
        , m_isBackPressed(false)
        , m_isRefreshPressed(false)
        , m_isPlayPressed(false)
        , m_isNextPressed(false)
        , m_isPrevPressed(false)
        , m_isSheetDismissPressed(false)
    {
        // Built-in curated full-length song library
        m_tracks = {
            { "s3_sunrise",         "Sunrise Melody",   "Voxa Cloud S3",  "Acoustic Chill",   180, "/api/music/stream/s3_sunrise" },
            { "s3_reminder",        "Reminder Alert",   "Voxa Cloud S3",  "Ambient Sound",    60,  "/api/music/stream/s3_reminder" },
            { "yt_yKNxeF4KMsY",     "Yellow",           "Coldplay",       "Alternative Rock", 273, "/api/music/stream/yt_yKNxeF4KMsY" },
            { "yt_4NRXx6U8ABQ",     "Blinding Lights",  "The Weeknd",     "Synthwave Pop",    200, "/api/music/stream/yt_4NRXx6U8ABQ" },
            { "yt_JGwWNGJdvx8",     "Shape of You",     "Ed Sheeran",     "Pop",              233, "/api/music/stream/yt_JGwWNGJdvx8" },
            { "yt_7wtfhZwyrcc",     "Believer",         "Imagine Dragons","Alt Rock",         217, "/api/music/stream/yt_7wtfhZwyrcc" },
            { "yt_H5v3kku4y6Q",     "As It Was",        "Harry Styles",   "Pop",              167, "/api/music/stream/yt_H5v3kku4y6Q" },
            { "yt_hT_nvWreIhg",     "Counting Stars",   "OneRepublic",    "Pop Rock",         257, "/api/music/stream/yt_hT_nvWreIhg" },
            { "yt_kTJczUoc26U",     "Stay",             "Justin Bieber",  "Pop",              141, "/api/music/stream/yt_kTJczUoc26U" },
            { "yt_TUVcZfQe-Kw",     "Levitating",       "Dua Lipa",       "Dance Pop",        203, "/api/music/stream/yt_TUVcZfQe-Kw" }
        };
    }

    void MusicPlayerScreen::fetchLibrary()
    {
        if (!WiFi.isConnected()) return;

        xTaskCreate(
            [](void* param) {
                MusicPlayerScreen* self = static_cast<MusicPlayerScreen*>(param);
                HTTPClient http;
                std::string url = apiClient.getBaseUrl() + "/api/music/library";
                http.begin(url.c_str());
                http.setTimeout(5000);

                int code = http.GET();
                if (code == HTTP_CODE_OK)
                {
                    String payload = http.getString();
                    JsonDocument doc;
                    DeserializationError err = deserializeJson(doc, payload);
                    if (!err && doc["success"].as<bool>())
                    {
                        JsonArray arr = doc["tracks"].as<JsonArray>();
                        if (!arr.isNull() && arr.size() > 0)
                        {
                            std::vector<MusicTrack> fetched;
                            for (JsonObject obj : arr)
                            {
                                MusicTrack t;
                                t.id = obj["id"].as<std::string>();
                                t.title = obj["title"].as<std::string>();
                                t.artist = obj["artist"].as<std::string>();
                                t.genre = obj["genre"] | "Full Music";
                                t.durationSecs = obj["duration"] | 210;
                                t.streamUrl = obj["streamUrl"].as<std::string>();
                                fetched.push_back(t);
                            }
                            if (!fetched.empty())
                            {
                                self->m_tracks = fetched;
                                Serial.printf("[VoxaMusic] Library updated: %d full songs\n", (int)fetched.size());
                            }
                        }
                    }
                }
                http.end();
                vTaskDelete(NULL);
            },
            "MusicFetchTask",
            6144,
            this,
            1,
            NULL
        );
    }

    void MusicPlayerScreen::searchMusic(const std::string& query)
    {
        if (!WiFi.isConnected() || query.empty()) return;

        m_isSearching = true;

        struct SearchParam {
            MusicPlayerScreen* self;
            std::string q;
        };
        SearchParam* p = new SearchParam{this, query};

        xTaskCreate(
            [](void* param) {
                SearchParam* p = static_cast<SearchParam*>(param);
                MusicPlayerScreen* self = p->self;
                HTTPClient http;

                // URL encode query
                String encodedQ = "";
                for (char c : p->q)
                {
                    if (c == ' ') encodedQ += "+";
                    else if (isalnum(c) || c == '-' || c == '_' || c == '.') encodedQ += c;
                    else {
                        char buf[4];
                        snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
                        encodedQ += buf;
                    }
                }

                std::string url = apiClient.getBaseUrl() + "/api/music/search?q=" + encodedQ.c_str();
                http.begin(url.c_str());
                http.setTimeout(12000);

                int code = http.GET();
                if (code == HTTP_CODE_OK)
                {
                    String payload = http.getString();
                    JsonDocument doc;
                    DeserializationError err = deserializeJson(doc, payload);
                    if (!err && doc["success"].as<bool>())
                    {
                        JsonArray arr = doc["tracks"].as<JsonArray>();
                        if (!arr.isNull() && arr.size() > 0)
                        {
                            std::vector<MusicTrack> fetched;
                            for (JsonObject obj : arr)
                            {
                                MusicTrack t;
                                t.id = obj["id"].as<std::string>();
                                t.title = obj["title"].as<std::string>();
                                t.artist = obj["artist"].as<std::string>();
                                t.genre = obj["genre"] | "Full Song";
                                t.durationSecs = obj["duration"] | 210;
                                t.streamUrl = obj["streamUrl"].as<std::string>();
                                fetched.push_back(t);
                            }
                            if (!fetched.empty())
                            {
                                self->m_searchResults = fetched;
                                Serial.printf("[VoxaMusic] Search for '%s' returned %d songs\n", p->q.c_str(), (int)fetched.size());
                            }
                        }
                    }
                }
                http.end();
                self->m_isSearching = false;
                delete p;
                vTaskDelete(NULL);
            },
            "MusicSearchTask",
            8192,
            p,
            1,
            NULL
        );
    }

    void MusicPlayerScreen::playTrack(int index)
    {
        const std::vector<MusicTrack>& activeList = (m_currentTab == MusicTab::Search && !m_searchResults.empty()) 
            ? m_searchResults 
            : m_tracks;

        if (index < 0 || index >= (int)activeList.size()) return;

        m_currentTrackIndex = index;
        const auto& track = activeList[index];

        AudioManager::instance().playTapSoundAsync();

        std::string fullUrl = track.streamUrl;
        if (fullUrl.rfind("http", 0) != 0)
        {
            fullUrl = apiClient.getBaseUrl() + fullUrl;
        }

        Serial.printf("[VoxaMusic] Playing full track: '%s' (%ds) from %s\n", 
                      track.title.c_str(), track.durationSecs, fullUrl.c_str());

        AudioManager::instance().playUrlAsync(fullUrl);

        m_isPlaying = true;
        m_trackStartMs = millis();
        m_trackDurationMs = track.durationSecs * 1000;
        m_showNowPlayingSheet = true;
    }

    void MusicPlayerScreen::togglePlayPause()
    {
        const std::vector<MusicTrack>& activeList = (m_currentTab == MusicTab::Search && !m_searchResults.empty()) 
            ? m_searchResults 
            : m_tracks;

        if (m_currentTrackIndex < 0)
        {
            if (!activeList.empty()) playTrack(0);
            return;
        }

        if (m_isPlaying)
        {
            AudioManager::instance().stop();
            m_isPlaying = false;
        }
        else
        {
            playTrack(m_currentTrackIndex);
        }
    }

    void MusicPlayerScreen::nextTrack()
    {
        const std::vector<MusicTrack>& activeList = (m_currentTab == MusicTab::Search && !m_searchResults.empty()) 
            ? m_searchResults 
            : m_tracks;

        if (activeList.empty()) return;
        int nextIdx = (m_currentTrackIndex + 1) % (int)activeList.size();
        playTrack(nextIdx);
    }

    void MusicPlayerScreen::prevTrack()
    {
        const std::vector<MusicTrack>& activeList = (m_currentTab == MusicTab::Search && !m_searchResults.empty()) 
            ? m_searchResults 
            : m_tracks;

        if (activeList.empty()) return;
        int prevIdx = (m_currentTrackIndex - 1 + (int)activeList.size()) % (int)activeList.size();
        playTrack(prevIdx);
    }

    ScreenId MusicPlayerScreen::show(Touch& touch)
    {
        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        bool useSprite = canvas.createSprite(w, h);
        if (useSprite) canvas.fillScreen(0);
        LovyanGFX& target = useSprite ? (LovyanGFX&)canvas : (LovyanGFX&)Display::lcd;

        // Fetch fresh S3 & online tracks in background
        fetchLibrary();

        ScreenId targetScreen = ScreenId::Music;
        uint32_t lastMs = millis();
        int entryFrame = 0;

        while (targetScreen == ScreenId::Music)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;
            m_animTime += deltaSecs;

            PowerManager::instance().tick();

            if (ButtonService::isDirectRecordRequested())
            {
                targetScreen = ScreenId::Record;
                break;
            }

            // Determine active track list based on filter tab
            std::vector<MusicTrack> displayedTracks;
            if (m_currentTab == MusicTab::Search)
            {
                displayedTracks = m_searchResults;
            }
            else if (m_currentTab == MusicTab::CloudS3)
            {
                for (const auto& t : m_tracks)
                {
                    if (t.id.rfind("s3_", 0) == 0) displayedTracks.push_back(t);
                }
            }
            else if (m_currentTab == MusicTab::Hits)
            {
                for (const auto& t : m_tracks)
                {
                    if (t.id.rfind("s3_", 0) != 0) displayedTracks.push_back(t);
                }
            }
            else
            {
                displayedTracks = m_tracks;
            }

            // Touch Processing
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            float extraSearchChipsH = (m_currentTab == MusicTab::Search) ? 75.0f : 0.0f;
            float contentHeight = 100.0f + extraSearchChipsH + displayedTracks.size() * 52.0f + 60.0f;
            float visibleHeight = h - 96.0f - (m_currentTrackIndex >= 0 ? 54.0f : 0.0f);
            float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            if (touched)
            {
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    m_dragStartY = ty;
                    m_lastDragY = ty;
                    m_lastTouchSampleMs = nowMs;
                    m_isDragging = false;
                    m_scrollVelocity = 0.0f;

                    if (!m_showNowPlayingSheet)
                    {
                        // 1. Back button
                        if (std::sqrt((tx - 22.0f)*(tx - 22.0f) + (ty - 42.0f)*(ty - 42.0f)) <= 22.0f)
                        {
                            m_isBackPressed = true;
                        }
                        // 2. Refresh library button
                        else if (std::sqrt((tx - (w - 22.0f))*(tx - (w - 22.0f)) + (ty - 42.0f)*(ty - 42.0f)) <= 22.0f)
                        {
                            m_isRefreshPressed = true;
                        }
                        // 3. Tab Bar buttons (y: 62..86)
                        else if (ty >= 58.0f && ty <= 88.0f)
                        {
                            float tabW = (w - 16.0f) / 4.0f;
                            int tabIdx = (int)((tx - 8.0f) / tabW);
                            if (tabIdx >= 0 && tabIdx < 4)
                            {
                                m_pressedTabIdx = tabIdx;
                            }
                        }
                        // 4. Search Suggestion Chips (if on Search tab)
                        else if (m_currentTab == MusicTab::Search && ty >= 92.0f && ty <= 165.0f && m_scrollY < 30.0f)
                        {
                            float chipW = (w - 24.0f) / 2.0f;
                            for (size_t c = 0; c < NUM_SEARCH_CHIPS; ++c)
                            {
                                float cx = 8.0f + (c % 2) * (chipW + 8.0f);
                                float cy = 92.0f + (c / 2) * 32.0f - m_scrollY;
                                if (tx >= cx && tx <= cx + chipW && ty >= cy && ty <= cy + 28.0f)
                                {
                                    m_pressedSearchChipIdx = (int)c;
                                    break;
                                }
                            }
                        }
                        // 5. Mini Player pill bar at bottom
                        else if (m_currentTrackIndex >= 0 && ty >= h - 56.0f)
                        {
                            if (tx >= w - 46.0f)
                            {
                                togglePlayPause();
                            }
                            else
                            {
                                m_showNowPlayingSheet = true;
                            }
                        }
                        // 6. Track List items
                        else if (ty >= 92.0f && ty <= (h - (m_currentTrackIndex >= 0 ? 54.0f : 10.0f)))
                        {
                            float leftX = w * 0.04f;
                            float cardW = w * 0.92f;
                            float startListY = 94.0f + extraSearchChipsH;
                            for (size_t i = 0; i < displayedTracks.size(); ++i)
                            {
                                float itemY = startListY + i * 52.0f - m_scrollY;
                                if (tx >= leftX && tx <= (leftX + cardW) && ty >= itemY && ty <= (itemY + 48.0f))
                                {
                                    m_pressedItemIndex = (int)i;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        // In Now Playing Sheet
                        if (ty <= 55.0f)
                        {
                            m_isSheetDismissPressed = true;
                        }
                        // Play/Pause button
                        else if (std::sqrt((tx - w * 0.5f)*(tx - w * 0.5f) + (ty - 262.0f)*(ty - 262.0f)) <= 28.0f)
                        {
                            m_isPlayPressed = true;
                        }
                        // Prev button
                        else if (std::sqrt((tx - 44.0f)*(tx - 44.0f) + (ty - 262.0f)*(ty - 262.0f)) <= 22.0f)
                        {
                            m_isPrevPressed = true;
                        }
                        // Next button
                        else if (std::sqrt((tx - (w - 44.0f))*(tx - (w - 44.0f)) + (ty - 262.0f)*(ty - 262.0f)) <= 22.0f)
                        {
                            m_isNextPressed = true;
                        }
                    }
                }
                else
                {
                    // Touch Move / Drag
                    float dy = ty - m_dragStartY;
                    if (!m_showNowPlayingSheet && std::abs(dy) > 10.0f)
                    {
                        m_isDragging = true;
                        m_pressedItemIndex = -1;
                        m_pressedTabIdx = -1;
                        m_pressedSearchChipIdx = -1;
                        m_isBackPressed = false;
                        m_isRefreshPressed = false;
                    }

                    if (m_isDragging)
                    {
                        float dragDeltaY = ty - m_lastDragY;
                        m_targetScrollY -= dragDeltaY;
                        m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));

                        uint32_t dt = nowMs - m_lastTouchSampleMs;
                        if (dt > 0)
                        {
                            m_scrollVelocity = -dragDeltaY / (dt / 1000.0f);
                        }
                        m_lastTouchSampleMs = nowMs;
                    }
                    m_lastDragY = ty;
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;
                    if (!m_isDragging)
                    {
                        if (!m_showNowPlayingSheet)
                        {
                            if (m_isBackPressed)
                            {
                                targetScreen = ScreenId::Home;
                            }
                            else if (m_isRefreshPressed)
                            {
                                fetchLibrary();
                            }
                            else if (m_pressedTabIdx >= 0)
                            {
                                m_currentTab = static_cast<MusicTab>(m_pressedTabIdx);
                                m_scrollY = 0;
                                m_targetScrollY = 0;
                                AudioManager::instance().playTapSoundAsync();
                            }
                            else if (m_pressedSearchChipIdx >= 0 && m_pressedSearchChipIdx < (int)NUM_SEARCH_CHIPS)
                            {
                                AudioManager::instance().playTapSoundAsync();
                                searchMusic(SEARCH_CHIPS[m_pressedSearchChipIdx]);
                            }
                            else if (m_pressedItemIndex >= 0)
                            {
                                playTrack(m_pressedItemIndex);
                            }
                        }
                        else
                        {
                            if (m_isSheetDismissPressed)
                            {
                                m_showNowPlayingSheet = false;
                            }
                            else if (m_isPlayPressed)
                            {
                                togglePlayPause();
                            }
                            else if (m_isPrevPressed)
                            {
                                prevTrack();
                            }
                            else if (m_isNextPressed)
                            {
                                nextTrack();
                            }
                        }
                    }
                    m_isBackPressed = false;
                    m_isRefreshPressed = false;
                    m_pressedTabIdx = -1;
                    m_pressedSearchChipIdx = -1;
                    m_isPlayPressed = false;
                    m_isPrevPressed = false;
                    m_isNextPressed = false;
                    m_isSheetDismissPressed = false;
                    m_pressedItemIndex = -1;
                    m_isDragging = false;
                }
            }

            // Scroll Physics
            if (!m_wasTouched && std::abs(m_scrollVelocity) > 0.0f)
            {
                m_targetScrollY += m_scrollVelocity * deltaSecs;
                m_scrollVelocity *= std::pow(0.85f, deltaSecs * 60.0f);
                if (std::abs(m_scrollVelocity) < 5.0f) m_scrollVelocity = 0.0f;
            }
            m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));
            m_scrollY += (m_targetScrollY - m_scrollY) * 15.0f * deltaSecs;

            // ── RENDERING ────────────────────────────────────────────────────
            target.fillScreen(0x0000);

            if (!m_showNowPlayingSheet)
            {
                // 1. Top Status Bar (Y = 10)
                target.setFont(&fonts::Font0);
                target.setTextDatum(textdatum_t::top_left);
                target.setTextColor(0xFFFF);
                target.drawString("10:42", 12, 10);

                target.setTextDatum(textdatum_t::top_right);
                target.setTextColor(0xFDC0); // Amber tag
                target.drawString("DAC 96K", w - 20, 10);
                // Mini EQ bars on right
                target.fillRect(w - 18, 14, 2, 6, 0xFDC0);
                target.fillRect(w - 14, 11, 2, 9, 0xFDC0);
                target.fillRect(w - 10, 15, 2, 5, 0xFDC0);

                // 2. Sub-Header (Y = 28)
                target.setTextDatum(textdatum_t::middle_left);
                target.setFont(&fonts::Font0);
                target.setTextColor(0x94A3B8);
                target.drawString("< Hub", 12, 34);

                target.setFont(&fonts::FreeSansBold9pt7b);
                target.setTextColor(0xFFFF);
                target.drawString("VOXA MUSIC", 56, 33);

                // EQ icon on right
                target.fillRect(w - 22, 28, 2, 12, 0xFDC0);
                target.fillRect(w - 18, 24, 2, 16, 0xFDC0);
                target.fillRect(w - 14, 30, 2, 10, 0xFDC0);

                // 3. Segmented Category Tab Filter Pills (Y = 48..68)
                target.fillRoundRect(10, 48, w - 20, 20, 5, target.color565(20, 22, 28));
                float tabW = (w - 20.0f) / 4.0f;
                const char* tabNames[] = { "Playlist", "Lossless", "Offline", "Search" };

                int activeTabIdx = static_cast<int>(m_currentTab);
                target.fillRoundRect(10 + activeTabIdx * tabW, 49, tabW, 18, 4, activeTabIdx == 0 ? 0xFFE8 : target.color565(36, 44, 58));

                target.setFont(&fonts::Font0);
                target.setTextDatum(textdatum_t::middle_center);

                for (int t = 0; t < 4; ++t)
                {
                    if (t == activeTabIdx)
                    {
                        target.setTextColor(t == 0 ? 0x0000 : 0xFFFF);
                    }
                    else
                    {
                        target.setTextColor(0x888888);
                    }
                    target.drawString(tabNames[t], 10 + tabW * (t + 0.5f), 58);
                }

                // 4. Track List & Search Chips (Y = 72..264)
                float leftX = 10.0f;
                float cardW = w - 20.0f;
                target.setClipRect(0, 72, w, 194);

                // If on Search Tab, draw Quick Search Chips
                if (m_currentTab == MusicTab::Search)
                {
                    float chipW = (w - 24.0f) / 2.0f;
                    for (size_t c = 0; c < NUM_SEARCH_CHIPS; ++c)
                    {
                        float cx = 8.0f + (c % 2) * (chipW + 8.0f);
                        float cy = 74.0f + (c / 2) * 30.0f - m_scrollY;
                        if (cy + 26.0f < 72.0f || cy > 264.0f) continue;

                        bool isP = (m_pressedSearchChipIdx == (int)c);
                        target.fillRoundRect((int)cx, (int)cy, (int)chipW, 26, 6, isP ? 0xFDC0 : target.color565(24, 28, 36));
                        target.drawRoundRect((int)cx, (int)cy, (int)chipW, 26, 6, target.color565(38, 44, 56));

                        target.setFont(&fonts::Font0);
                        target.setTextDatum(textdatum_t::middle_center);
                        target.setTextColor(isP ? 0x0000 : 0xFFFF);
                        target.drawString(SEARCH_CHIPS[c], cx + chipW * 0.5f, cy + 13.0f);
                    }
                }

                // Render song list cards
                float startListY = (m_currentTab == MusicTab::Search) ? 140.0f : 74.0f;
                for (size_t i = 0; i < displayedTracks.size(); ++i)
                {
                    float itemY = startListY + i * 56.0f - m_scrollY;
                    if (itemY + 52.0f < 72.0f || itemY > 264.0f) continue;

                    bool isPressed = (m_pressedItemIndex == (int)i);
                    bool isCur = (m_currentTrackIndex == (int)i);

                    uint16_t cardBg = isPressed ? target.color565(28, 32, 42) : target.color565(18, 20, 26);
                    uint16_t cardBorder = isCur ? 0xFDC0 : target.color565(34, 38, 48);

                    target.fillRoundRect((int)leftX, (int)itemY, (int)cardW, 50, 8, cardBg);
                    target.drawRoundRect((int)leftX, (int)itemY, (int)cardW, 50, 8, cardBorder);

                    // Track Title
                    target.setFont(&fonts::Font0);
                    target.setTextDatum(textdatum_t::top_left);
                    target.setTextColor(0xFFFF);
                    std::string titleStr = displayedTracks[i].title;
                    if (titleStr.length() > 22) titleStr = titleStr.substr(0, 20) + "..";
                    target.drawString(titleStr.c_str(), leftX + 12.0f, itemY + 10.0f);

                    // Track Subtitle / Artist / Specs
                    target.setTextColor(0x94A3B8);
                    std::string sub = displayedTracks[i].artist + " · FLAC Lossless";
                    if (sub.length() > 24) sub = sub.substr(0, 22) + "..";
                    target.drawString(sub.c_str(), leftX + 12.0f, itemY + 26.0f);

                    // Right EQ / Disc Icon
                    if (isCur && m_isPlaying)
                    {
                        // Animated orange EQ bars
                        for (int b = 0; b < 3; ++b)
                        {
                            int barH = 6 + (int)(std::abs(std::sin(m_animTime * 6.0f + b * 1.5f)) * 14.0f);
                            target.fillRect((int)(leftX + cardW - 24.0f + b * 5.0f), (int)(itemY + 25.0f - barH * 0.5f), 3, barH, 0xFDC0);
                        }
                    }
                    else
                    {
                        // Disc circle
                        target.drawCircle((int)(leftX + cardW - 16.0f), (int)(itemY + 25.0f), 7, target.color565(60, 68, 84));
                        target.drawCircle((int)(leftX + cardW - 16.0f), (int)(itemY + 25.0f), 2, target.color565(80, 90, 110));
                    }
                }

                target.clearClipRect();

                // 5. Floating Bottom Mini-Player Bar (Y = 270..306)
                float barY = 270.0f;
                target.fillRoundRect((int)leftX, (int)barY, (int)cardW, 36, 6, target.color565(22, 24, 30));
                target.drawRoundRect((int)leftX, (int)barY, (int)cardW, 36, 6, target.color565(40, 44, 56));

                // Progress line
                target.fillRect((int)leftX + 4, (int)barY + 2, 40, 2, 0xFDC0);

                // Play triangle + timestamp
                target.fillTriangle((int)leftX + 10, (int)barY + 14, (int)leftX + 10, (int)barY + 24, (int)leftX + 18, (int)barY + 19, 0xFDC0);

                target.setFont(&fonts::Font0);
                target.setTextDatum(textdatum_t::middle_left);
                target.setTextColor(0xFFFF);
                target.drawString("03:42 / 12:00", leftX + 24.0f, barY + 19.0f);

                // "24-BIT" tag on right
                target.setTextDatum(textdatum_t::middle_right);
                target.setTextColor(0xFDC0);
                target.drawString("24-BIT", leftX + cardW - 10.0f, barY + 19.0f);
            }
            else
            {
                // ── NOW PLAYING SHEET (Minimal Lossless DAC Player) ─────────
                target.fillScreen(0x0000);

                // 1. Top Header (Y = 16)
                target.setFont(&fonts::Font0);
                target.setTextDatum(textdatum_t::top_left);
                target.setTextColor(target.color565(140, 155, 175));
                target.drawString("LOSSLESS  DAC", 16.0f, 16.0f);

                target.setTextDatum(textdatum_t::top_right);
                target.setTextColor(target.color565(115, 185, 235));
                target.drawString("96kHz", w - 16.0f, 16.0f);

                // 2. Center Waveform Capsule Card (Y = 88)
                int cardW = 76;
                int cardH = 76;
                int cardX = (w - cardW) / 2;
                int cardY = 88;

                target.fillRoundRect(cardX, cardY, cardW, cardH, 16, target.color565(26, 30, 42));
                target.drawRoundRect(cardX, cardY, cardW, cardH, 16, target.color565(38, 44, 58));

                int cardCx = cardX + cardW / 2;
                int cardCy = cardY + cardH / 2;

                const float barOffsets[5] = { -16.0f, -8.0f, 0.0f, 8.0f, 16.0f };
                const float barBaseH[5]   = { 12.0f,  20.0f,  30.0f, 20.0f, 12.0f };
                const float barW_px       = 3.5f;

                for (int i = 0; i < 5; ++i)
                {
                    float bx = cardCx + barOffsets[i];
                    float animScale = 1.0f;
                    if (m_isPlaying)
                    {
                        float waveAnim = std::sin(m_animTime * 10.0f + i * 1.2f);
                        animScale = 0.55f + 0.45f * waveAnim;
                    }
                    else
                    {
                        animScale = 0.65f;
                    }

                    float bh = std::max(6.0f, barBaseH[i] * animScale);
                    float by = cardCy - bh * 0.5f;
                    target.fillRoundRect((int)(bx - barW_px * 0.5f), (int)by, (int)barW_px, (int)bh, 1, 0xFFFF);
                }

                // 3. Filename / Title (Y = 186)
                if (m_currentTrackIndex >= 0 && m_currentTrackIndex < (int)displayedTracks.size())
                {
                    const auto& curT = displayedTracks[m_currentTrackIndex];

                    target.setFont(&fonts::FreeSansBold9pt7b);
                    target.setTextDatum(textdatum_t::middle_center);
                    target.setTextColor(0xFFFF);
                    std::string mTitle = curT.title;
                    if (mTitle.find('.') == std::string::npos) mTitle += ".wav";
                    if (mTitle.length() > 22) mTitle = mTitle.substr(0, 20) + "..";
                    target.drawString(mTitle.c_str(), w * 0.5f, 186.0f);
                }

                // 4. Progress Bar (Y = 216)
                float progY = 216.0f;
                float barStartX = 16.0f;
                float barW = w - 32.0f;
                target.fillRoundRect((int)barStartX, (int)progY, (int)barW, 4, 2, target.color565(36, 42, 54));

                uint32_t elapsedMs = m_isPlaying ? (nowMs - m_trackStartMs) : 0;
                float pct = std::min(1.0f, (float)elapsedMs / (float)m_trackDurationMs);
                if (elapsedMs >= m_trackDurationMs && m_isPlaying)
                {
                    nextTrack();
                }

                int fillW = (int)(barW * pct);
                if (fillW > 0)
                {
                    target.fillRoundRect((int)barStartX, (int)progY, fillW, 4, 2, 0xFFFF);
                }

                // 5. Bottom Controls (Y = 270)
                int ctrlY = 270;
                int prevX = 52;
                uint16_t prevCol = m_isPrevPressed ? target.color565(160, 160, 160) : 0xFFFF;
                target.fillRect(prevX - 7, ctrlY - 6, 2, 12, prevCol);
                target.fillTriangle(prevX - 4, ctrlY, prevX + 4, ctrlY - 6, prevX + 4, ctrlY + 6, prevCol);

                int playX = 120;
                uint16_t playCol = m_isPlayPressed ? target.color565(160, 160, 160) : 0xFFFF;
                if (m_isPlaying)
                {
                    target.fillRect(playX - 4, ctrlY - 7, 3, 14, playCol);
                    target.fillRect(playX + 2, ctrlY - 7, 3, 14, playCol);
                }
                else
                {
                    target.fillTriangle(playX - 5, ctrlY - 8, playX - 5, ctrlY + 8, playX + 7, ctrlY, playCol);
                }

                int nextX = 188;
                uint16_t nextCol = m_isNextPressed ? target.color565(160, 160, 160) : 0xFFFF;
                target.fillTriangle(nextX - 4, ctrlY - 6, nextX - 4, ctrlY + 6, nextX + 4, ctrlY, nextCol);
                target.fillRect(nextX + 5, ctrlY - 6, 2, 12, nextCol);
            }

            if (entryFrame < 10)
            {
                entryFrame++;
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Music), entryFrame, 10);
            }

            if (useSprite) canvas.pushSprite(0, 0);
            delay(16);
        }

        if (useSprite) canvas.deleteSprite();
        return targetScreen;
    }
}
